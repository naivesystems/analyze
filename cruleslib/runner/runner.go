/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

package runner

import (
	"container/list"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"regexp"
	"runtime"
	"runtime/debug"
	"sort"
	"strconv"
	"strings"
	"sync"
	"syscall"

	"github.com/golang/glog"
	"golang.org/x/text/message"
	"google.golang.org/protobuf/proto"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/cruleslib/filter"
	"naive.systems/analyzer/cruleslib/i18n"
	"naive.systems/analyzer/cruleslib/issuecode"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/severity"
	"naive.systems/analyzer/cruleslib/stats"
	"naive.systems/analyzer/misra/analyzer"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/clangsema"
	"naive.systems/analyzer/misra/checker_integration/cppcheck"
	"naive.systems/analyzer/misra/checker_integration/cpplint"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/checker_integration/gcc_diagnostics"
	"naive.systems/analyzer/misra/checker_integration/infer"
	"naive.systems/analyzer/misra/checker_integration/libtooling"
	"naive.systems/analyzer/misra/checker_integration/misra"
)

// The task for Runner to run in parallels
type AnalyzerTask struct {
	Id      int
	Srcdir  string
	Opts    *options.CheckOptions
	Analyze func(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error)
	Rule    string
}

type analyzerResult struct {
	id             int
	rule           string
	srcdir         string
	resultsList    *pb.ResultsList
	customSeverity string
	err            error
}

// A goroutine workgroup to run analyzers in parallel.
type ParaTaskRunner struct {
	showProgress   bool
	workerWg       sync.WaitGroup
	collectorWg    sync.WaitGroup
	jobs_chan      chan AnalyzerTask
	results_chan   chan analyzerResult
	sigs_exiting   chan bool
	results        *pb.ResultsList
	errors         []error
	processPrinter basic.CheckingProcessPrinter
}

// modify the analyzer result.
// eg. add rule name prefix to the report message.
func modifyResult(result *analyzerResult) {
	for _, r := range result.resultsList.Results {
		// fix error message prefix
		// misra_cpp_2008/rule_X_X_X -> [CXX0000][misra-cpp2008-X.X.X]
		// gjb8114/rule_X_X_X -> [C0000][gjb-8114-X.X.X]
		// gjb8114/rule_A_X_X_X -> [C0000][gjb-8114-A.X.X.X]
		// googlecpp/g1149 -> [G1149][googlecpp-g1149]
		// clang-tidy/Y0711 -> [Y0711][clang-tidy-Y0711]
		// autosar/rule_AX_X_X -> [AX_X_X][autosar-AX.X.X]
		// autosar/rule_MX_X_X -> [MX_X_X][autosar-MX.X.X]
		// cwe/cwe_401 -> [CWE_401][cwe-cwe_401]
		edition := strings.Split(result.rule, "/")[0]
		ruleName := strings.Split(result.rule, "/")[1]
		ruleStr := strings.Join(strings.Split(ruleName, "_")[1:], ".")
		issueCode := issuecode.GetIssueCode(edition, ruleName)
		if issueCode == "" {
			if edition == "misra_cpp_2008" || strings.HasPrefix(edition, "gjb") {
				glog.Warning("There is no available issue code for ", result.rule)
				// mock for the issue code parsing of the vscode extension
				issueCode = "-"
			} else if edition == "autosar" {
				ruleName = strings.TrimPrefix(ruleName, "rule_")
				r.ErrorMessage = fmt.Sprintf("[%s][%s-%s]: %s", ruleName, edition, ruleStr, r.ErrorMessage)
			} else if edition != "misra_c_2012" {
				r.ErrorMessage = fmt.Sprintf("[%s][%s-%s]: %s", strings.ToUpper(ruleName), edition, ruleName, r.ErrorMessage)
			}
		}
		issueCode = "[" + issueCode + "]"
		if edition == "misra_cpp_2008" {
			r.ErrorMessage = issueCode + "[misra-cpp2008-" + ruleStr + "]: " + r.ErrorMessage
		} else if strings.HasPrefix(edition, "gjb") {
			subEdition := strings.TrimPrefix(edition, "gjb")
			r.ErrorMessage = issueCode + "[gjb-" + subEdition + "-" + ruleStr + "]: " + r.ErrorMessage
		}
		r.Ruleset = edition
		r.RuleId = ruleName
	}
}

func (pt *ParaTaskRunner) worker(jobs <-chan AnalyzerTask, results chan<- analyzerResult, printer *message.Printer) {
	for j := range jobs {
		if pt.showProgress {
			pt.processPrinter.StartAnalyzeTask(j.Rule, printer)
			// disable CheckProgress to avoid print other progress when use another runner
			// for this rule. eg. an misra checker in gjb will call misra runner and
			// print like "misra cheking 1/1 rules" results.
			// TODO: fix this
			j.Opts.EnvOption.CheckProgress = false
		}
		func() {
			defer func() {
				// recover from possible panic
				if r := recover(); r != nil {
					glog.Error("Recovered in analyze: ", r, string(debug.Stack()))
					results <- analyzerResult{id: j.Id, err: errors.New("panic in analyze rule"), resultsList: nil, rule: j.Rule, srcdir: j.Srcdir}
					if pt.showProgress {
						j.Opts.EnvOption.CheckProgress = true
						pt.processPrinter.FinishAnalyzeTask(j.Rule, printer)
					}
				}
			}()
			resultList, err := j.Analyze(j.Srcdir, j.Opts)
			customSeverity := ""
			if j.Opts.JsonOption.Severity != nil {
				customSeverity = *j.Opts.JsonOption.Severity
			}
			results <- analyzerResult{id: j.Id, err: err, resultsList: resultList, rule: j.Rule, srcdir: j.Srcdir, customSeverity: customSeverity}
			if pt.showProgress {
				j.Opts.EnvOption.CheckProgress = true
				pt.processPrinter.FinishAnalyzeTask(j.Rule, printer)
				stats.WriteProgress(j.Opts.EnvOption.ResultsDir, stats.AC, pt.processPrinter.GetPercentString(), pt.processPrinter.GetStartedAt())
			}
		}()
	}
	pt.workerWg.Done()
}

// Create a new task runner and results collectors.
func NewParaTaskRunner(numWorkers int32, taskNums int, showProgress bool, lang string) *ParaTaskRunner {
	printer := i18n.GetPrinter(lang)
	if numWorkers == 0 {
		numWorkers = int32(runtime.NumCPU())
		if showProgress {
			basic.PrintfWithTimeStamp(printer.Sprintf("Use %d CPU(s)", numWorkers))
		}
	}
	paraRunner := &ParaTaskRunner{
		showProgress:   showProgress,
		jobs_chan:      make(chan AnalyzerTask, numWorkers),
		results_chan:   make(chan analyzerResult, numWorkers),
		sigs_exiting:   make(chan bool, 1),
		results:        &pb.ResultsList{},
		errors:         make([]error, taskNums),
		processPrinter: basic.NewCheckingProcessPrinter(taskNums),
	}
	for w := 0; w < int(numWorkers); w++ {
		paraRunner.workerWg.Add(1)
		go paraRunner.worker(paraRunner.jobs_chan, paraRunner.results_chan, printer)
	}

	// signal handler in crules
	sigs := make(chan os.Signal, 1)
	// if a signal is received, notify the loop to stop sending new workers
	signal.Notify(sigs, syscall.SIGINT)
	// collect results
	paraRunner.collectorWg.Add(1)
	go func() {
		for job_result := range paraRunner.results_chan {
			select {
			case <-sigs:
				// if recived a SIGINT, stop collector and analyze rule loop
				if paraRunner.showProgress {
					basic.PrintfWithTimeStamp("Ctrl C Pressed. Stop analysis")
				}
				// notifie the 'for i, rule := range rules' loop to exit
				paraRunner.sigs_exiting <- true
				paraRunner.collectorWg.Done()
				return
			default:
			}
			if job_result.err == nil {
				modifyResult(&job_result)
				resultsWithSeverity := severity.AddSeverity(job_result.resultsList, job_result.rule, job_result.customSeverity)
				paraRunner.results.Results = append(paraRunner.results.Results, resultsWithSeverity.Results...)
			} else {
				glog.Errorf("Analyze %v got error %v", job_result.rule, job_result.err)
			}
			paraRunner.errors[job_result.id] = job_result.err
		}
		paraRunner.collectorWg.Done()
	}()
	return paraRunner
}

// check for the SIGINT existing signal
// If the existing signal is received, it will return results and errors.
// results will never be nil if the existing signal is received.
// If the existing signal is not received, it will return nil for results and nil for errors.
func (pt *ParaTaskRunner) CheckSignalExiting() (results *pb.ResultsList, errors []error) {
	select {
	// if recived a SIGINT, stop analyze rule loop
	case <-pt.sigs_exiting:
		// close the jobs_chan to let worker end
		close(pt.jobs_chan)
		pt.collectorWg.Wait()
		// return results and errors directly because collector has stop.
		return pt.results, pt.errors
	default:
		return nil, nil
	}
}

// Add a task to the task runner and start running the task.
// The rule name will be added to the report message.
func (pt *ParaTaskRunner) AddTask(task AnalyzerTask) {
	pt.jobs_chan <- task
}

// Wait until all the tasks workers and collectors are finished and all results are collected.
// Return the results and errors.
func (pt *ParaTaskRunner) CollectResultsAndErrors() (results *pb.ResultsList, errors []error) {
	go func() {
		pt.workerWg.Wait()
		close(pt.results_chan)
	}()
	close(pt.jobs_chan)
	pt.collectorWg.Wait()
	return pt.results, pt.errors
}

func SortResult(results *pb.ResultsList) *pb.ResultsList {
	sort.Slice(results.Results, func(i, j int) bool {
		list := results.Results
		if list[i].Path != list[j].Path {
			return list[i].Path < list[j].Path
		}
		return list[i].LineNumber < list[j].LineNumber
	})
	return results
}

func GetCompileCommandsPath(srcdir string, opts *options.CheckOptions) string {
	if opts.EnvOption.IsDev {
		return options.GetCompileCommandsPath(srcdir)
	}
	return options.GetActualCompileCommandsPath(srcdir)
}

func GetSourceFiles(srcdir string, opts *options.CheckOptions) ([]string, error) {
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	if !opts.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	return analyzer.ListSourceFiles(compileCommandsPath, opts.EnvOption.IgnoreDirPatterns)
}

func RunMisra(srcdir, checkRule string, checkOptions *options.CheckOptions) *pb.ResultsList {
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	envOptions := checkOptions.EnvOption
	allResults, err := misra.Exec(
		srcdir,
		compileCommandsPath,
		envOptions.BuildActions,
		checkRule,
		checkOptions.RuleSpecificOption.RuleSpecificResultDir,
		envOptions.CheckerConfig,
		envOptions.LimitMemory,
		envOptions.TimeoutNormal,
		envOptions.TimeoutOom,
	)
	if err != nil {
		glog.Errorf("failed to run misra.Exec: %v", err)
	}
	return allResults
}

// Deprecated: directly call the Analyze funciton under misra_c_2012_crules
func RunMisraAnalyze(srcdir string, misraCheckRules []checkrule.CheckRule, checkOptions *options.CheckOptions) *pb.ResultsList {
	allResults := &pb.ResultsList{}

	envOptions := checkOptions.EnvOption
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	// TODO: pass compileCommandsPath as function arguments
	if !checkOptions.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath)
	if err != nil {
		glog.Fatalf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)

	if envOptions.IsDev {
		for _, rule := range misraCheckRules {
			ruleNumber := strings.Split(rule.Name, "/")[1]

			individualCheckerConfig := proto.Clone(envOptions.CheckerConfig).(*pb.CheckerConfiguration)
			individualCheckerConfig.MisraCheckerPath = filepath.Join(individualCheckerConfig.MisraCheckerPath, ruleNumber)

			results := analyzer.Analyze(
				[]checkrule.CheckRule{rule},
				envOptions.ResultsDir,
				compileCommandsPath,
				envOptions.BuildActions,
				envOptions.IgnoreDirPatterns,
				individualCheckerConfig,
				envOptions.DumpErrors,
				envOptions.CheckProgress,
				envOptions.NumWorkers,
				envOptions.LimitMemory,
				envOptions.TimeoutNormal,
				envOptions.TimeoutOom,
				envOptions.Lang,
				filteredCompileCommandsFolder,
				/*onlyCppcheck=*/ false,
			)
			allResults.Results = append(allResults.Results, results.Results...)
		}
	} else {
		allResults = analyzer.Analyze(
			misraCheckRules,
			envOptions.ResultsDir,
			compileCommandsPath,
			envOptions.BuildActions,
			envOptions.IgnoreDirPatterns,
			envOptions.CheckerConfig,
			envOptions.DumpErrors,
			envOptions.CheckProgress,
			envOptions.NumWorkers,
			envOptions.LimitMemory,
			envOptions.TimeoutNormal,
			envOptions.TimeoutOom,
			envOptions.Lang,
			filteredCompileCommandsFolder,
			/*onlyCppcheck=*/ false,
		)
	}

	return SortResult(allResults)
}

func RunCSA(srcdir string, csaOptions string, checkOptions *options.CheckOptions) (*csa.CSAReport, error) {
	envOptions := checkOptions.EnvOption
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	// TODO: pass compileCommandsPath as function arguments
	if !checkOptions.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	resultPath := checkOptions.RuleSpecificOption.CSAResultDir

	if _, err := os.Stat(resultPath); err != nil {
		err = os.Mkdir(resultPath, os.ModePerm)
		if err != nil {
			glog.Errorf("failed to create csa result dir: %v", err)
			return nil, err
		}
	}
	ctuBaseDir := filepath.Join(checkOptions.EnvOption.ResultsDir, "ctu-dir")
	reportJson, err := csa.GetCSAReport(compileCommandsPath, csaOptions, ctuBaseDir, resultPath, checkOptions.JsonOption, envOptions.BuildActions, envOptions.CheckerConfig, envOptions.LimitMemory, envOptions.TimeoutNormal, envOptions.TimeoutOom, envOptions.DisableParallelismInChecker)
	if err != nil {
		glog.Errorf("CSA execution failed: %v", err)
		return nil, err
	}
	return &reportJson, nil
}

func RunInfer(srcdir string, inferPlugin string, checkOptions *options.CheckOptions) (*[]infer.InferReport, error) {
	envOptions := checkOptions.EnvOption
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	// TODO: pass compileCommandsPath as function arguments
	if !checkOptions.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	resultPath := checkOptions.RuleSpecificOption.InferResultDir

	if _, err := os.Stat(resultPath); err != nil {
		err = os.Mkdir(resultPath, os.ModePerm)
		if err != nil {
			glog.Errorf("failed to create infer result dir: %v", err)
			return nil, err
		}
	}
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath)
	if err != nil {
		glog.Fatalf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)

	reportJsons, err := infer.GetInferReport(
		srcdir,
		filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
		inferPlugin,
		resultPath,
		envOptions.CheckerConfig,
		envOptions.LimitMemory,
		envOptions.TimeoutNormal,
		envOptions.TimeoutOom,
	)
	if err != nil {
		glog.Errorf("Infer execution failed: %v", err)
		return nil, err
	}
	return &reportJsons, nil
}

func RunLibtoolingWithExtraArgs(srcdir string, ruleName string, extraArgsStr string, checkerType checker_integration.Checker, opts *options.CheckOptions) (*pb.ResultsList, error) {
	ruleInfo := strings.Split(ruleName, "/")
	if len(ruleInfo) != 2 {
		return nil, fmt.Errorf("invalid rule name format: %s", ruleName)
	}

	edition := ruleInfo[0]
	ruleStr := ruleInfo[1]
	checkerPath, ok := opts.EnvOption.CheckerPathsMap[edition]
	if !ok {
		return nil, fmt.Errorf("invalid edition name: %s", edition)
	}

	extraArgs := []string{}
	if extraArgsStr != "" {
		extraArgs = strings.Split(extraArgsStr, " ")
	}
	if len(opts.JsonOption.OptionalInfoFile) > 0 {
		extraArgs = append(extraArgs, fmt.Sprintf("--OptionalInfoFile=%s", opts.JsonOption.OptionalInfoFile))
	}
	if opts.JsonOption.MaximumAllowedFuncLine != nil {
		extraArgs = append(extraArgs, fmt.Sprintf("--MaximumAllowedFuncLine=%d", *opts.JsonOption.MaximumAllowedFuncLine))
	}

	// Rule specific JSON options
	basic.FilterLibToolingExtraArgs(&extraArgs, ruleName, opts.JsonOption)

	resultsPath := filepath.Join(opts.RuleSpecificOption.LibtoolingResultDir, edition+ruleStr+"results")

	progName := checkerPath
	isMisra := edition == "misra"
	// Since some old infra problems, misra libtooling checker is not in same path with others.
	if opts.EnvOption.IsDev && !isMisra {
		progName = filepath.Join(progName, ruleStr, "libtooling", ruleStr)
	} else if opts.EnvOption.IsDev && isMisra {
		progName = filepath.Join(progName, ruleStr, ruleStr)
	} else {
		progName = filepath.Join(progName, ruleStr)
	}

	if _, err := os.Stat(progName); err != nil {
		glog.Errorf("error when finding checker: %v", err)
		return nil, err
	}

	// TODO: list files without compile_commands.json
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	// TODO: pass compileCommandsPath as function arguments
	if !opts.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath)
	if err != nil {
		glog.Fatalf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)
	sourceFiles, err := analyzer.ListSourceFiles(compileCommandsPath, opts.EnvOption.IgnoreDirPatterns)
	if err != nil {
		glog.Errorf("failed to get source files: %v", err)
	}

	libtoolingLogDir := filepath.Join(opts.EnvOption.LogDir, "libtooling") + "/"
	allResults, err := libtooling.ExecLibtooling(
		progName,
		resultsPath,
		opts.RuleSpecificOption.LibtoolingResultDir,
		libtoolingLogDir,
		extraArgs,
		opts.EnvOption.BuildActions,
		sourceFiles,
		opts.EnvOption.CheckerConfig,
		checkerType,
		opts.EnvOption.LimitMemory,
		opts.EnvOption.TimeoutNormal,
		opts.EnvOption.TimeoutOom,
		filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
	)
	if err != nil {
		glog.Errorf("failed to execute checker: %v", err)
		return nil, err
	}

	for _, result := range allResults.Results {
		path, err := basic.ConvertRelativePathToAbsolute(srcdir, result.Path)
		if err != nil {
			glog.Error(err)
			continue
		}
		result.Path = path
	}

	return SortResult(allResults), nil
}

func RunLibtooling(srcdir string, ruleName string, checkerType checker_integration.Checker, opts *options.CheckOptions) (*pb.ResultsList, error) {
	return RunLibtoolingWithExtraArgs(srcdir, ruleName, "", checkerType, opts)
}

func RunCppcheck(srcdir string, ruleName string, checkerType checker_integration.Checker, opts *options.CheckOptions) (*pb.ResultsList, error) {
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	// TODO: pass compileCommandsPath as function arguments
	if !opts.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	if checkerType == checker_integration.Cppcheck_STU {
		results, err := cppcheck.ExecSTU(
			ruleName,
			opts.JsonOption,
			compileCommandsPath,
			opts.RuleSpecificOption.CppcheckResultDir,
			opts.EnvOption.ResultsDir,
			opts.EnvOption.BuildActions,
			opts.EnvOption.CheckerConfig.CppcheckBin,
			opts.EnvOption.CheckerConfig.PythonBin,
			opts.EnvOption.DumpErrors,
			opts.EnvOption.LimitMemory,
			opts.EnvOption.TimeoutNormal,
			opts.EnvOption.TimeoutOom)
		if err != nil {
			return nil, err
		}
		return SortResult(results), err
	} else if checkerType == checker_integration.Cppcheck_CTU {
		results, err := cppcheck.ExecCTU(
			ruleName,
			opts.JsonOption,
			compileCommandsPath,
			opts.RuleSpecificOption.CppcheckResultDir,
			opts.EnvOption.ResultsDir,
			opts.EnvOption.BuildActions,
			opts.EnvOption.CheckerConfig.CppcheckBin,
			opts.EnvOption.CheckerConfig.PythonBin,
			opts.EnvOption.DumpErrors,
			opts.EnvOption.LimitMemory,
			opts.EnvOption.TimeoutNormal,
			opts.EnvOption.TimeoutOom)
		if err != nil {
			return nil, err
		}
		return SortResult(results), err
	} else if checkerType == checker_integration.Cppcheck_Binary {
		results, err := cppcheck.GetCppCheckCoreXMLErrors(
			ruleName, opts.JsonOption,
			compileCommandsPath,
			opts.RuleSpecificOption.CppcheckResultDir,
			opts.EnvOption.ResultsDir,
			opts.EnvOption.BuildActions,
			opts.EnvOption.CheckerConfig.CppcheckBin,
			opts.EnvOption.DumpErrors,
			opts.EnvOption.LimitMemory)
		if err != nil {
			return nil, err
		}
		return SortResult(results), err
	}

	err := fmt.Errorf("invalid checke type: %v", checkerType)
	glog.Errorf("error while trying to execute cppcheck: %v", err)

	return nil, err
}

func RunCpplint(srcdir, filter string, opts *options.CheckOptions) ([]cpplint.Report, error) {
	runner := &cpplint.Runner{
		PythonBin:     opts.EnvOption.CheckerConfig.PythonBin,
		Script:        opts.EnvOption.CheckerConfig.CpplintScript,
		TaskName:      filepath.Base(opts.EnvOption.ResultsDir),
		LimitMemory:   opts.EnvOption.LimitMemory,
		TimeoutNormal: opts.EnvOption.TimeoutNormal,
		TimeoutOom:    opts.EnvOption.TimeoutOom,
		Srcdir:        srcdir,
	}
	if opts.JsonOption.RootDir != "" {
		return runner.RunRoot(srcdir, opts.JsonOption.RootDir, filter)
	}
	return runner.Run(srcdir, filter)
}

func RunClangSema(srcdir, cmd_arg string, opts *options.CheckOptions) ([]clangsema.Diagnostic, error) {
	// TODO: pass compileCommandsPath as function arguments
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	if !opts.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	return clangsema.GetClangSemaDiagnostics(
		compileCommandsPath,
		cmd_arg,
		opts.RuleSpecificOption.RuleSpecificResultDir,
		opts.EnvOption.BuildActions,
		opts.EnvOption.CheckerConfig.GetCsaSystemLibOptions(),
		opts.EnvOption.CheckerConfig.GetClangBin(),
		opts.EnvOption.CheckerConfig.GetGccPredefinedMacros(),
		opts.EnvOption.LimitMemory,
		opts.EnvOption.TimeoutNormal,
		opts.EnvOption.TimeoutOom)
}

func RunClangTidy(srcdir string, cmd_arg []string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}

	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	if !opts.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath)
	if err != nil {
		glog.Fatalf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)

	for _, action := range *opts.EnvOption.BuildActions {
		if action.Command.ContainsCC1() {
			continue
		}
		sourceFile := action.Command.File
		// TODO: pre-exec checks
		cmdArgs := []string{"--checks=-*," + cmd_arg[0]}
		if len(cmd_arg) > 1 {
			cmdArgs = append(cmdArgs, cmd_arg[1])
		}
		cmdArgs = append(cmdArgs, fmt.Sprintf("--p=%s", filteredCompileCommandsFolder))
		cmdArgs = append(cmdArgs, sourceFile)
		for _, field := range strings.Fields(opts.EnvOption.CheckerConfig.CsaSystemLibOptions) {
			cmdArgs = append(cmdArgs, "--extra-arg", field)
		}
		cmdArgs = append(cmdArgs, opts.JsonOption.ClangTidyArgsString...)
		cmd := exec.Command(opts.EnvOption.CheckerConfig.ClangtidyBin, cmdArgs...)
		cmd.Dir = action.Command.Directory
		glog.Info("executing: ", cmd.String())
		// TODO: Output is based on []byte, maybe exceed buffer size
		out, err := cmd.CombinedOutput()
		if err != nil {
			glog.Errorf("clang-tidy execution error: executing: %s, reported:\n%s", cmd.String(), string(out))
			return &resultsList, err
		}
		re := regexp.MustCompile(fmt.Sprintf(`(.*):(\d*):(\d*): (.*) \[(%s)\]\n`, strings.ReplaceAll(cmd_arg[0], ",", "|")))
		matches := re.FindAllStringSubmatch(string(out), -1)
		for _, match := range matches {
			path := match[1]
			linenum, err := strconv.Atoi(match[2])
			errMessage := match[4]
			if err != nil {
				glog.Error(errors.New("ClangTidy output cannot be parsed"))
				return &resultsList, err
			}
			path, err = basic.ConvertRelativePathToAbsolute(action.Command.Directory, path)
			if err != nil {
				glog.Error(err)
				continue
			}
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:         path,
				LineNumber:   int32(linenum),
				ErrorMessage: errMessage,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func RunClangForErrorsOrWarnings(srcdir string, getErrors bool, extraArgs string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	args := []string{}
	if extraArgs != "" {
		args = strings.Split(extraArgs, " ")
	}
	// TODO: pass compileCommandsPath as function arguments
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	if !opts.EnvOption.IsDev {
		compileCommandsPath = options.GetActualCompileCommandsPath(srcdir)
	}
	return misra.GetClangErrorsOrWarningsWithProtoReturn(compileCommandsPath, opts.EnvOption.ResultsDir, getErrors, args, opts.EnvOption.BuildActions, opts.EnvOption.CheckerConfig, opts.EnvOption.LimitMemory, opts.EnvOption.TimeoutNormal, opts.EnvOption.TimeoutOom)
}

func RunGCC(opts *options.CheckOptions) ([]gcc_diagnostics.Diagnostic, error) {
	actions := opts.EnvOption.BuildActions
	resultsDir := opts.EnvOption.ResultsDir
	limitMemory := opts.EnvOption.LimitMemory
	diagnostics := []gcc_diagnostics.Diagnostic{}
	for _, action := range *actions {
		actionDiagnostics, err := gcc_diagnostics.GetActionDiagnostics(action, resultsDir, opts.EnvOption.CheckerConfig.CsaSystemLibOptions, limitMemory, opts.EnvOption.TimeoutNormal, opts.EnvOption.TimeoutOom)
		if err != nil {
			return nil, fmt.Errorf("GetActionDiagnostics: %v", err)
		}
		diagnostics = append(diagnostics, actionDiagnostics...)
	}
	return diagnostics, nil
}

func RunHeaderCompile(srcdir string, ruleName string, checkOptions *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}
	cmd := exec.Command("find", srcdir, "-name", "*.h")
	output, err := cmd.Output()
	if err != nil {
		return nil, err
	}
	outputStr := strings.Trim(string(output), "\n")
	if len(outputStr) == 0 {
		return results, nil
	}
	lines := strings.Split(outputStr, "\n")

	cmdArgs := []string{"-x", "c++-header"}
	cmdArgs = append(cmdArgs, strings.Fields(checkOptions.EnvOption.CheckerConfig.CsaSystemLibOptions)...)

	headerCompileDirOptionsMap, err := getHeaderCompileDirOptionsMap(srcdir, checkOptions)
	if err != nil {
		return nil, err
	}
	for _, headerFile := range lines {
		headerCompileDirOptionsCollection, exist := headerCompileDirOptionsMap[headerFile]
		if !exist {
			continue
		}
		cmdArgs := append(cmdArgs, headerFile)
		hasSucceeded := false
		for _, headerCompileDirOptions := range headerCompileDirOptionsCollection {
			for _, headerCompileDirOption := range headerCompileDirOptions {
				cmdArgs = append(cmdArgs, "-I"+headerCompileDirOption)
			}
			cmd := exec.Command(checkOptions.EnvOption.CheckerConfig.ClangBin, cmdArgs...)
			glog.Infof("for %s, executing: %s", ruleName, cmd.String())
			if _, err := cmd.CombinedOutput(); err == nil {
				hasSucceeded = true
				break
			}
		}
		if !hasSucceeded {
			results.Results = append(results.Results, &pb.Result{
				Path:       headerFile,
				LineNumber: 1,
			})
		}
	}
	return results, nil
}

type empty struct{}
type set map[string]empty

func getIncludeRelationship(filename string) (map[string]set, error) {
	// read file-headerFile mapping relationship from the temporary file.
	// 'file' means it can be a source file or a header file.
	fileHeaderMap := make(map[string]set)
	contentBytes, _ := os.ReadFile(filename)
	if len(contentBytes) == 0 {
		return fileHeaderMap, nil
	}
	IncludeRelationshipPairs := strings.Split(strings.Trim(string(contentBytes), "\n"), "\n")
	for _, IncludeRelationshipPair := range IncludeRelationshipPairs {
		pairFields := strings.Split(IncludeRelationshipPair, " ")
		if len(pairFields) != 2 {
			return nil, fmt.Errorf("unexpected pair format")
		}
		file := pairFields[0]
		headerIncludedByFile := pairFields[1]
		_, exist := fileHeaderMap[file]
		if !exist {
			fileHeaderMap[file] = make(set)
		}
		var value empty
		fileHeaderMap[file][headerIncludedByFile] = value
	}
	return fileHeaderMap, nil
}

func getAbsSrcCompileDirOptions(srcdir string, opts *options.CheckOptions) (map[string][][]string, error) {
	absCompileDirOptionsCollection := make(map[string][][]string)
	for _, buildAction := range *opts.EnvOption.BuildActions {
		// `srcdir` is also an dir option.
		absCompileDirOptions := []string{srcdir}
		for _, arg := range buildAction.Command.Arguments {
			for _, compileOption := range []string{"-I", "-idirafter", "-iquote", "-isysroot", "-isystem", "-sysroot", "--sysroot"} {
				if !strings.HasPrefix(arg, compileOption) {
					continue
				}
				compileDirOption := strings.TrimPrefix(arg, compileOption)
				if !filepath.IsAbs(compileDirOption) {
					compileDirOption = filepath.Join(srcdir, compileDirOption)
				}
				// Find out whether its a path of a file. If it's a file,
				// split the filename and just keep the dir.
				f, err := os.Stat(compileDirOption)
				if err != nil {
					return nil, err
				}
				if !f.IsDir() {
					compileDirOption = filepath.Dir(compileDirOption)
				}
				absCompileDirOptions = append(absCompileDirOptions, compileDirOption)
			}
		}
		absCompileDirOptionsCollection[buildAction.Command.File] = append(absCompileDirOptionsCollection[buildAction.Command.File], absCompileDirOptions)
	}
	return absCompileDirOptionsCollection, nil
}

func getAbsHeaderPath(header string, dirOptionsCollection [][]string) (string, error) {
	absHeaderPath := ""
	if filepath.IsAbs(header) {
		absHeaderPath = header
	} else {
		for _, dirOptions := range dirOptionsCollection {
			for _, dirOption := range dirOptions {
				absHeaderPath := filepath.Join(dirOption, header)
				_, err := os.Stat(absHeaderPath)
				if err == nil {
					return absHeaderPath, nil
				} else if !os.IsNotExist(err) {
					return "", err
				}
			}
		}
	}
	return absHeaderPath, nil
}

func getHeaderSrcsMap(rawFileHeadersMap *map[string]set, absSrcCompileDirOptions *map[string][][]string) (map[string]set, error) {
	// for a header file, find all source files which include it
	headerSrcsMap := make(map[string]set)
	// 'file' can be a source file or a header file, and here it's already an absolute path
	for file, headersIncludedByFile := range *rawFileHeadersMap {
		if !filter.IsCCFile(file) {
			continue
		}
		srcCompileDirsCollection, exist := (*absSrcCompileDirOptions)[file]
		if !exist {
			srcCompileDirsCollection = [][]string{}
		}
		queue := list.New()
		for headerIncludedByFile := range headersIncludedByFile {
			queue.PushBack(headerIncludedByFile)
		}
		for queue.Len() > 0 {
			header := queue.Front().Value.(string)
			queue.Remove(queue.Front())
			absHeaderPath, err := getAbsHeaderPath(header, srcCompileDirsCollection)
			if err != nil {
				continue
			}
			_, exist := headerSrcsMap[absHeaderPath]
			if !exist {
				headerSrcsMap[absHeaderPath] = make(set)
			}
			var value empty
			headerSrcsMap[absHeaderPath][file] = value
			headersIncludedByAbsHeaderPath, exist := (*rawFileHeadersMap)[absHeaderPath]
			if exist {
				for header := range headersIncludedByAbsHeaderPath {
					queue.PushBack(header)
				}
			}
		}
	}
	return headerSrcsMap, nil
}

func getHeaderCompileDirOptionsMap(srcdir string, checkOptions *options.CheckOptions) (map[string][][]string, error) {
	temporaryIncludeRelationshipFile := checkOptions.JsonOption.OptionalInfoFile
	if !filepath.IsAbs(temporaryIncludeRelationshipFile) {
		temporaryIncludeRelationshipFile = filepath.Join(checkOptions.EnvOption.ResultsDir, temporaryIncludeRelationshipFile)
	}
	// content in rawFileHeadersMap: file (src or header) - [headerFile1, headerFile2, ...]
	rawFileHeadersMap := make(map[string]set)
	if _, err := os.Stat(temporaryIncludeRelationshipFile); err == nil {
		rawFileHeadersMap, err = getIncludeRelationship(temporaryIncludeRelationshipFile)
		if err != nil {
			return nil, err
		}
	}
	err := os.Remove(temporaryIncludeRelationshipFile)
	if err != nil && !os.IsNotExist(err) {
		return nil, err
	}
	absSrcCompileDirOptions, err := getAbsSrcCompileDirOptions(srcdir, checkOptions)
	if err != nil {
		return nil, err
	}
	headerSrcsMap, err := getHeaderSrcsMap(&rawFileHeadersMap, &absSrcCompileDirOptions)
	if err != nil {
		return nil, err
	}
	headerCompileDirOptionsMap := make(map[string][][]string)
	for header, srcs := range headerSrcsMap {
		headerCompileDirOptionsCollection := [][]string{}
		for src := range srcs {
			headerCompileDirOptions, exist := absSrcCompileDirOptions[src]
			if !exist {
				headerCompileDirOptions = [][]string{}
			}
			headerCompileDirOptionsCollection = append(headerCompileDirOptionsCollection, headerCompileDirOptions...)
		}
		headerCompileDirOptionsMap[header] = headerCompileDirOptionsCollection
	}
	return headerCompileDirOptionsMap, nil
}

func MergeResults(results ...[]*pb.Result) []*pb.Result {
	merged := []*pb.Result{}
	for _, list := range results {
		merged = append(merged, list...)
	}
	return merged
}

func HasTargetErrorMessageFragment(result *pb.Result, targetErrorMessage string) bool {
	messages := strings.Split(result.ErrorMessage, "\n")
	for _, msg := range messages {
		if strings.Contains(msg, targetErrorMessage) {
			return true
		}
	}
	return false
}

func KeepNeededErrorByFilterTargetMsgInResults(results *pb.ResultsList, targetErrorMessage string, newErrorMessage string) *pb.ResultsList {
	newResults := &pb.ResultsList{}
	for _, result := range results.Results {
		if HasTargetErrorMessageFragment(result, targetErrorMessage) {
			result.ErrorMessage = newErrorMessage
			newResults.Results = append(newResults.Results, result)
		}
	}
	return newResults
}

func KeepNeededErrorByFilterTargetMsgInCSAReports(reportJson *csa.CSAReport, targetErrorMessagePattern string, newErrorMessage string) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	if reportJson == nil || len(reportJson.Runs) == 0 {
		return resultsList
	}
	for _, run := range reportJson.Runs {
		results := run.Results
		targetErrorMessageReg := regexp.MustCompile(targetErrorMessagePattern)
		for _, resultsContent := range results {
			if targetErrorMessageReg.Match([]byte(resultsContent.Message.Text)) {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorMessage = newErrorMessage + "\n" + resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CSAReportsToPBResults(reportJson *csa.CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	if reportJson == nil || len(reportJson.Runs) == 0 {
		return nil
	}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			result := &pb.Result{}
			result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
			result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
			result.ErrorMessage = resultsContent.Message.Text
			resultsList.Results = append(resultsList.Results, result)
		}
	}
	return resultsList
}

func KeepNeededErrorByFilteringRuleIdInCSAReports(reportJson *csa.CSAReport, matchingRuleIdPattern string, newErrorMessage string, errKind pb.Result_ErrorKind) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	if reportJson == nil || len(reportJson.Runs) == 0 {
		return nil
	}
	reg := regexp.MustCompile(matchingRuleIdPattern)
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if reg.Match([]byte(resultsContent.RuleId)) {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorMessage = newErrorMessage
				result.ExternalMessage = resultsContent.Message.Text
				result.ErrorKind = errKind
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func FindDuplicate(res *pb.Result, results *pb.ResultsList) bool {
	for _, r := range results.Results {
		if r.Path == res.Path && r.LineNumber == res.LineNumber {
			return true
		}
	}
	return false
}

func RemoveDup(results *pb.ResultsList) *pb.ResultsList {
	final := &pb.ResultsList{}
	allKeys := make(map[string]bool)
	for _, res := range results.Results {
		key := (res.Path + string(res.LineNumber))
		if _, value := allKeys[key]; !value {
			allKeys[key] = true
			final.Results = append(final.Results, res)
		}
	}
	return final
}

type Loc struct {
	string
	int32
}

func AddResult(rule, path, externalErrorMessage string, line int32, results map[Loc]struct{}, resultsList *pb.ResultsList) {
	loc := Loc{path, line}
	if _, reported := results[loc]; !reported {
		results[loc] = struct{}{}
		errorMessage := externalErrorMessage
		if externalErrorMessage != "" {
			errorMessage = "\n" + externalErrorMessage
		}
		resultsList.Results = append(resultsList.Results, &pb.Result{
			Path:         path,
			LineNumber:   line,
			ErrorMessage: rule + errorMessage,
		})
	}
}
