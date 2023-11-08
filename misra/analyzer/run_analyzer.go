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

package analyzer

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cpumem"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/cruleslib/i18n"
	"naive.systems/analyzer/cruleslib/severity"
	"naive.systems/analyzer/cruleslib/stats"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/compilecommand"
	"naive.systems/analyzer/misra/checker_integration/cppcheck"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/checker_integration/libtooling"
	"naive.systems/analyzer/misra/utils"
)

func ListSourceFiles(compileCommandsPath string, ignoreDirPatterns []string) ([]string, error) {
	content, err := os.ReadFile(compileCommandsPath)
	if err != nil {
		return nil, fmt.Errorf("ioutil.ReadFile: %v", err)
	}

	commandList := make([]compilecommand.CompileCommand, 0)
	err = json.Unmarshal(content, &commandList)
	if err != nil {
		return nil, fmt.Errorf("json.Unmarshal: %v", err)
	}
	fileMap := make(map[string]bool)
	fileList := make([]string, 0)
	for _, cmd := range commandList {
		_, exist := fileMap[cmd.File]
		if !exist {
			fileMap[cmd.File] = true
			matched, err := analyzerinterface.MatchIgnoreDirPatterns(ignoreDirPatterns, cmd.File)
			if err != nil {
				return nil, err
			}
			if !matched {
				fileList = append(fileList, cmd.File)
			}
		}
	}
	return fileList, nil
}

type AnalyzerTask struct {
	Rule                          checkrule.CheckRule
	ResultDir                     string
	CompileCommandsPath           string
	FilteredCompileCommandsFolder string
	SourceFiles                   []string
	CheckerConfig                 *pb.CheckerConfiguration
	DumpErrors                    map[string]string
}

type AnalyzerResult struct {
	Rule        checkrule.CheckRule
	ResultsList *pb.ResultsList
	Error       error
}

func touchFile(name string) error {
	file, err := os.OpenFile(name, os.O_RDONLY|os.O_CREATE, 0644)
	if err != nil {
		return err
	}
	return file.Close()
}

func allocateCpuMem(taskName string, mem int, checker checker_integration.Checker, checkerConfig *pb.CheckerConfiguration) (int, error) {
	var cpu int
	var err error
	cpu = 1
	err = basic.CreateCgroup(taskName, mem)
	if err != nil {
		return 0, fmt.Errorf("limit memory usage: %v", err)
	}
	if checker == checker_integration.Infer || checker == checker_integration.Mixture {
		cpu = int(checkerConfig.InferJobs)
	}
	if err = cpumem.Acquire(cpu, mem, taskName); err != nil {
		return 0, err
	}

	return cpu, nil
}

func allocateCpu(taskName string, checker checker_integration.Checker, checkerConfig *pb.CheckerConfiguration) (int, error) {
	var cpu int
	var err error
	cpu = 1
	if err != nil {
		return 0, fmt.Errorf("limit cpu usage: %v", err)
	}
	if checker == checker_integration.Infer || checker == checker_integration.Mixture {
		cpu = int(checkerConfig.InferJobs)
	}
	if err = cpumem.Acquire(cpu, 0, taskName); err != nil {
		return 0, err
	}

	return cpu, nil
}

func runChecker(
	ruleResults *pb.ResultsList,
	checker checker_integration.Checker,
	checkRule checkrule.CheckRule,
	taskName, resultsDir,
	libtoolingLogDir,
	resultsPath,
	compileCommandsPath string,
	buildActions *[]csa.BuildAction,
	sourceFiles []string,
	checkerConfig *pb.CheckerConfiguration,
	dumpErrors map[string]string,
	limitMemory,
	useGCC bool,
	timeoutNormal,
	timeoutOom int,
	filteredCompileCommandsFolder string,
	onlyCppcheck bool,
) (*pb.ResultsList, []error, error) {
	var err error
	var errs []error
	allResults := &pb.ResultsList{} // Results of current checker
	if checker == checker_integration.Infer && !onlyCppcheck {
		err = checker_integration.ExecInfer(
			checkRule,
			filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
			resultsDir,
			checkerConfig,
			allResults,
			limitMemory,
			timeoutNormal,
			timeoutOom,
		)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.CSA && !onlyCppcheck {
		err = checker_integration.ExecCSA(checkRule, compileCommandsPath, resultsDir, buildActions, checkerConfig, allResults, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Cppcheck_STU {
		err = checker_integration.ExecCppcheckSTU(checkRule, compileCommandsPath, resultsDir, buildActions, checkerConfig, allResults, dumpErrors, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Cppcheck_CTU {
		err = checker_integration.ExecCppcheckCTU(checkRule, compileCommandsPath, resultsDir, buildActions, checkerConfig, allResults, dumpErrors, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Clangtidy && !onlyCppcheck {
		err = checker_integration.ExecClangtidy(
			filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
			checkRule.Name,
			resultsDir,
			buildActions,
			checkerConfig.CsaSystemLibOptions,
			checkerConfig.ClangtidyBin,
			allResults,
			limitMemory,
			timeoutNormal,
			timeoutOom)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Misra && !onlyCppcheck {
		err = checker_integration.ExecMisra(
			filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
			checkRule.Name,
			resultsDir,
			buildActions,
			checkerConfig,
			allResults,
			limitMemory,
			timeoutNormal,
			timeoutOom,
		)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Clangsema && !onlyCppcheck {
		err = checker_integration.ExecClangsema(compileCommandsPath, checkRule.Name, resultsDir, buildActions, checkerConfig, allResults, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.GCC && useGCC && !onlyCppcheck {
		err = checker_integration.ExecGCC(compileCommandsPath, checkRule.Name, resultsDir, checkerConfig.GetCsaSystemLibOptions(), buildActions, allResults, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Libtooling_STU || checker == checker_integration.Libtooling_CTU && !onlyCppcheck {
		err = RunCmdWithRuleName(
			filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
			checkRule.Name,
			resultsPath,
			resultsDir,
			libtoolingLogDir,
			checkerConfig.GetMisraCheckerPath(),
			checkRule.JSONOptions,
			buildActions,
			sourceFiles,
			checkerConfig,
			allResults,
			checker,
			limitMemory,
			timeoutNormal,
			timeoutOom,
		)
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
		}
	} else if checker == checker_integration.Mixture && !onlyCppcheck {
		// TODO: remove Mixture
		errs = ExecMixture(
			checkRule,
			filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson),
			resultsDir,
			libtoolingLogDir,
			checkerConfig.GetMisraCheckerPath(),
			buildActions,
			sourceFiles,
			checkerConfig,
			allResults,
			limitMemory,
			timeoutNormal,
			timeoutOom,
		)
		if errs != nil {
			errStrings := []string{}
			for _, err := range errs {
				errStrings = append(errStrings, err.Error())
			}
			glog.Errorf("in %s: %v errors: %v", taskName, len(errs), strings.Join(errStrings, "\n"))
		}
	} else if checker == checker_integration.EmptyChecker {
		// TODO: remove this hack
		glog.Infof("in %s: checking %s", taskName, checkRule.Name)
	}
	glog.Infof("in %s: checker %s reported %d results", taskName, checker, len(allResults.Results))
	ruleResults.Results = append(ruleResults.Results, allResults.Results...)
	return ruleResults, errs, err
}

func Run(
	checkRule checkrule.CheckRule,
	resultsDir,
	compileCommandsPath string,
	buildActions *[]csa.BuildAction,
	sourceFiles []string,
	checkerConfig *pb.CheckerConfiguration,
	dumpErrors map[string]string,
	numWorkers int32,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int,
	filteredCompileCommandsFolder string,
	onlyCppcheck bool,
) (*pb.ResultsList, error) {
	integratedRuleCheckers := checker_integration.RuleCheckerMap
	checkers, exist := integratedRuleCheckers[checkRule.Name]
	if !exist || len(checkers) == 0 {
		return nil, errors.New("unrecognized rule name " + checkRule.Name)
	}
	// only use gcc if usable
	_, err := exec.LookPath("gcc")
	useGCC := err == nil
	if !useGCC {
		glog.Warning("gcc is disabled and this may introduce false negatives")
	}
	resultsDir, err = os.MkdirTemp(resultsDir, filepath.Base(checkRule.Name)+"-*")
	// TODO` if succeeded.
	if err != nil {
		glog.Errorf("rule: %v, os.MkdirTemp: %v", checkRule.Name, err)
		return nil, err
	}
	libtoolingLogDir := ""
	if flag.Lookup("log_dir").Value.String() != "" {
		libtoolingLogDir = filepath.Join(filepath.Clean(flag.Lookup("log_dir").Value.String()), "libtooling") + "/"
	}
	taskName := filepath.Base(resultsDir)
	glog.Infof("in %s: %s %v", taskName, checkRule.Name, checkRule.JSONOptions)
	resultsPath := filepath.Join(resultsDir, "results")
	err = touchFile(resultsPath)
	if err != nil {
		glog.Errorf("rule: %v, touch file: %v", checkRule.Name, err)
		return nil, err
	}
	ruleResults := &pb.ResultsList{}
	mem := cpumem.GetTotalMem() / int(numWorkers)
	for _, checker := range checkers {
		var cpu int
		if limitMemory {
			cpu, err = allocateCpuMem(taskName, mem, checker, checkerConfig)
			if err != nil {
				glog.Errorf("rule: %v: %v", checkRule.Name, err)
				return nil, err
			}
		} else {
			cpu, err = allocateCpu(taskName, checker, checkerConfig)
			if err != nil {
				glog.Errorf("rule: %v: %v", checkRule.Name, err)
				return nil, err
			}
		}
		var errs []error
		ruleResults, errs, err = runChecker(
			ruleResults,
			checker,
			checkRule,
			taskName,
			resultsDir,
			libtoolingLogDir,
			resultsPath,
			compileCommandsPath,
			buildActions,
			sourceFiles,
			checkerConfig,
			dumpErrors,
			limitMemory,
			useGCC,
			timeoutNormal,
			timeoutOom,
			filteredCompileCommandsFolder,
			onlyCppcheck,
		)
		if limitMemory {
			cpumem.Release(cpu, mem)
		} else {
			cpumem.Release(cpu, 0)
		}
		oomKill := false
		if err != nil && (err.Error() == "signal: killed" || strings.Contains(err.Error(), "timed out")) {
			oomKill = true
		}
		for _, e := range errs {
			if e.Error() == "signal: killed" || strings.Contains(e.Error(), "timed out") {
				oomKill = true
				break
			}
		}
		if limitMemory && oomKill {
			// TODO: magnification should be an option
			mem = int(float64(mem) * 1.5)
			cpu, err := allocateCpuMem(taskName, mem, checker, checkerConfig)
			if err != nil {
				glog.Errorf("rule: %v: %v", checkRule.Name, err)
				return nil, err
			}
			ruleResults, _, _ = runChecker(
				ruleResults,
				checker,
				checkRule,
				taskName,
				resultsDir,
				libtoolingLogDir,
				resultsPath,
				compileCommandsPath,
				buildActions,
				sourceFiles,
				checkerConfig,
				dumpErrors,
				/* limitMemory */ true,
				useGCC,
				timeoutNormal,
				timeoutOom,
				filteredCompileCommandsFolder,
				onlyCppcheck,
			)
			cpumem.Release(cpu, mem)
		}
		// for misra_c_2012/rule_8_1, if the misra checker to execute clang will
		// be used only if the gcc checker returns some error
		if err == nil && checkRule.Name == "misra_c_2012/rule_8_1" &&
			checker == checker_integration.GCC {
			break
		}
	}
	glog.Infof("in %s: totally reported %d results", taskName, len(ruleResults.Results))

	// Filter results for misra_c_2012/rule_2_5
	// according to the flag checkRule.JSONOptions.CheckIncludeGuards
	// TODO: Move this part into misra_c_2012_crules/rule_2_5/rule_2_5.go
	// after the test case migration (b/8549) is done.
	filteredResults := []*pb.Result{}
	if checkRule.Name == "misra_c_2012/rule_2_5" && checkRule.JSONOptions.ToString() != "{}" && len(ruleResults.Results) != 0 {
		for _, result := range ruleResults.Results {
			// The error is not in include guards, reported anyway.
			if !strings.HasSuffix(result.ErrorMessage, "(optional)") {
				filteredResults = append(filteredResults, result)
			}
			// The flag is not specified, with the false default value.
			if checkRule.JSONOptions.CheckIncludeGuards == nil {
				continue
			}
			// Otherwise, the flag is specified as true,
			// and results with an 'optional' mark should be reported.
			if checkRule.JSONOptions.CheckIncludeGuards != nil && *checkRule.JSONOptions.CheckIncludeGuards {
				filteredResults = append(filteredResults, result)
			}
		}
		ruleResults.Results = filteredResults
	}

	return ruleResults, nil
}

func PreAnalyze(
	compileCommandsPath string,
	buildActions *[]csa.BuildAction,
	checkRules []checkrule.CheckRule,
	resultsDir string,
	config *pb.CheckerConfiguration,
	checkProgress bool,
	enableCodeChecker bool,
	numWorkers int32,
	ignoreDirPatterns []string,
	lang string,
	onlyCppcheck bool,
) (map[string]string, error) {
	glog.Infof("Start PreAnalyze")

	err := resolveCheckerBinariesPath(checkRules, config, enableCodeChecker)
	if err != nil {
		return nil, err
	}

	standardSet, err := checkrule.ConstructStandardSet(checkRules)
	if err != nil {
		return map[string]string{}, err
	}

	usedCheckers := make(map[checker_integration.Checker]bool)
	ruleCheckerMap := checker_integration.RuleCheckerMap
	for _, rule := range checkRules {
		checkers, ok := ruleCheckerMap[rule.Name]
		if ok {
			for _, checker := range checkers {
				usedCheckers[checker] = true
			}
		}
	}

	dumpError, err := PreAnalyzeBody(
		compileCommandsPath,
		buildActions,
		usedCheckers,
		standardSet,
		resultsDir,
		config,
		checkProgress,
		enableCodeChecker,
		numWorkers,
		ignoreDirPatterns,
		lang,
		onlyCppcheck,
	)

	return dumpError, err
}

func PreAnalyzeBody(
	compileCommandsPath string,
	buildActions *[]csa.BuildAction,
	usedCheckers map[checker_integration.Checker]bool,
	standardSet map[string]bool,
	resultsDir string,
	config *pb.CheckerConfiguration,
	checkProgress bool,
	enableCodeChecker bool,
	numWorkers int32,
	ignoreDirPatterns []string,
	lang string,
	onlyCppcheck bool,
) (map[string]string, error) {
	dumpErrors := make(map[string]string, 0)
	printer := i18n.GetPrinter(lang)
	if numWorkers == 0 {
		numWorkers = int32(runtime.NumCPU())
	}
	if (usedCheckers[checker_integration.CSA] || usedCheckers[checker_integration.Misra]) && !onlyCppcheck {
		start := time.Now()
		if checkProgress {
			basic.PrintfWithTimeStamp(printer.Sprintf("Start preparing CTU information"))
			stats.WriteProgress(resultsDir, stats.CTU, "0%", start)
		}
		if enableCodeChecker {
			err := csa.GenerateCTUExtraArgumentsFromCodeChecker(compileCommandsPath, resultsDir, config)
			if err != nil {
				return dumpErrors, fmt.Errorf("csa.GenerateCTUExtraArgumentsFromCodeChecker: %v", err)
			}
		} else {
			// currently use our pre-analyzer
			err := csa.GenerateCTUExtraArguments(compileCommandsPath, resultsDir, config, checkProgress, lang, numWorkers)
			if err != nil {
				return dumpErrors, fmt.Errorf("csa.GenerateCTUExtraArguments: %v", err)
			}
		}
		elapsed := time.Since(start)
		if checkProgress {
			timeUsed := basic.FormatTimeDuration(elapsed)
			basic.PrintfWithTimeStamp(printer.Sprintf("CTU information preparation completed [%s]", timeUsed))
		}
	}
	if usedCheckers[checker_integration.Cppcheck_STU] || usedCheckers[checker_integration.Cppcheck_CTU] {
		taskNumbers := len(*buildActions)
		results := make(chan []error, taskNumbers)
		syncDumpErrors := sync.Map{}
		var finishededTaskNumbers int = 0

		start := time.Now()
		if checkProgress {
			basic.PrintfWithTimeStamp(printer.Sprintf("Start preparing STU information"))
			stats.WriteProgress(resultsDir, stats.STU, "0%", start)
		}

		tasks := make(chan csa.BuildAction, numWorkers)
		for i := 0; i < int(numWorkers); i++ {
			go createDumpFile(tasks, results, config, standardSet, resultsDir, ignoreDirPatterns, &syncDumpErrors)
		}
		for _, buildAction := range *buildActions {
			tasks <- buildAction
		}
		close(tasks)
		for i := 0; i < taskNumbers; i++ {
			actionErrs := <-results
			finishededTaskNumbers++
			if checkProgress {
				percent := basic.GetPercentString(finishededTaskNumbers, taskNumbers)
				stats.WriteProgress(resultsDir, stats.STU, percent, start)
				percent += "%"
				basic.PrintfWithTimeStamp(printer.Sprintf("%s STU information preparation completed (%v/%v)", percent, finishededTaskNumbers, taskNumbers))
			}
			if len(actionErrs) > 0 {
				for err := range actionErrs {
					glog.Errorf("cppcheck pre-analyze: %v", err)
				}
			}
		}
		// wait until all tasks complete, and collect results
		syncDumpErrors.Range(func(k, v interface{}) bool {
			dumpErrors[k.(string)] = v.(string)
			return true
		})

		elapsed := time.Since(start)
		if checkProgress {
			timeUsed := basic.FormatTimeDuration(elapsed)
			basic.PrintfWithTimeStamp(printer.Sprintf("STU information preparation completed [%s]", timeUsed))
		}
	}
	glog.Infof("Finish PreAnalyze")
	return dumpErrors, nil
}

func createDumpFile(
	actions <-chan csa.BuildAction,
	results chan<- []error,
	config *pb.CheckerConfiguration,
	standardSet map[string]bool,
	resultsDir string,
	ignoreDirPatterns []string,
	dumpErrors *sync.Map) {
	for action := range actions {
		var actionErrs []error
		preprocessorSymbols := csa.ParsePreprocessorSymbols(action, false, false)
		preprocessorSymbols = append(preprocessorSymbols, csa.ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), false)...)
		for standard := range standardSet {
			if strings.HasSuffix(action.Command.File, ".S") || strings.HasSuffix(action.Command.File, ".s") {
				glog.Warningf(".S or .s file skipped for Cppcheck: %s", action.Command.File)
				continue
			}
			matched, err := analyzerinterface.MatchIgnoreDirPatterns(ignoreDirPatterns, action.Command.File)
			if err != nil {
				glog.Error(err)
				continue
			}
			if matched {
				continue
			}
			dumpFilePath := filepath.Join(resultsDir, action.Command.File) + "." + standard + ".dump"
			cmd_args := []string{"--abspath", "--dump", "--quiet", "--max-configs=1", "--std=" + standard, "--dump-file=" + dumpFilePath, action.Command.File}
			cmd_args = append(cmd_args, preprocessorSymbols...)
			_, err = os.Stat(filepath.Dir(dumpFilePath))
			if errors.Is(err, os.ErrNotExist) {
				err = os.MkdirAll(filepath.Dir(dumpFilePath), os.ModePerm)
				if err != nil {
					glog.Errorf("GenerateXMLDumpFile: os.MkdirAll: %v failed, %v ", dumpFilePath, err)
					continue
				}
			}
			dumpError, err := cppcheck.ExecCppcheckBinary(action.Command.Directory, "", cmd_args, config.CppcheckBin, false)
			if err != nil {
				actionErrs = append(actionErrs, fmt.Errorf("failed to GenerateXMLDumpFile for %s: %v", action.Command.File, err))
			}
			if len(dumpError) > 0 {
				dumpErrors.Store(dumpFilePath, string(dumpError))
			}
		}
		results <- actionErrs
	}
}

func Analyze(
	checkRules []checkrule.CheckRule,
	resultsDir string,
	compileCommandsPath string,
	buildActions *[]csa.BuildAction,
	ignoreDirPatterns []string,
	checkerConfig *pb.CheckerConfiguration,
	dumpErrors map[string]string,
	checkProgress bool,
	numWorkers int32,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int,
	lang,
	filteredCompileCommandsFolder string,
	onlyCppcheck bool,
) *pb.ResultsList {
	glog.Infof("Start Analyze")
	printer := i18n.GetPrinter(lang)
	analyzeStart := time.Now()
	allResults := &pb.ResultsList{}
	sourceFiles, err := ListSourceFiles(compileCommandsPath, ignoreDirPatterns)
	if err != nil {
		glog.Errorf("analyzer.ListSourceFiles: %v", err)
	}
	var finishedTaskNumbers int32 = 0
	// start NumWorkers workers
	if numWorkers == 0 {
		numWorkers = int32(runtime.NumCPU())
	}
	if checkProgress {
		basic.PrintfWithTimeStamp(printer.Sprintf("Use %d CPU(s)", numWorkers))
	}

	var workerWg sync.WaitGroup
	worker := func(jobs <-chan *AnalyzerTask, results chan<- *AnalyzerResult) {
		for j := range jobs {
			func() {
				start := time.Now()
				// TODO: refactor this, just pass job is suitable
				resultList, err := Run(
					j.Rule,
					j.ResultDir,
					j.CompileCommandsPath,
					buildActions,
					j.SourceFiles,
					j.CheckerConfig,
					j.DumpErrors,
					numWorkers,
					limitMemory,
					timeoutNormal,
					timeoutOom,
					j.FilteredCompileCommandsFolder,
					onlyCppcheck,
				)
				results <- &AnalyzerResult{Rule: j.Rule, ResultsList: resultList, Error: err}

				elapsed := time.Since(start)
				currentFinishedNumber := atomic.AddInt32(&finishedTaskNumbers, 1)
				if checkProgress {
					percent := basic.GetPercentString(int(currentFinishedNumber), len(checkRules))
					stats.WriteProgress(resultsDir, stats.AC, percent, analyzeStart)
					percent += "%"
					totalRuleNumber := len(checkRules)
					ruleName := j.Rule.Name
					timeUsed := basic.FormatTimeDuration(elapsed)
					basic.PrintfWithTimeStamp(printer.Sprintf("Analysis of %s completed (%s, %v/%v) [%s]", ruleName, percent, currentFinishedNumber, totalRuleNumber, timeUsed))

				}
			}()
		}
		workerWg.Done()
	}

	jobs_chan := make(chan *AnalyzerTask, numWorkers)
	results_chan := make(chan *AnalyzerResult, numWorkers)
	for w := 0; w < int(numWorkers); w++ {
		workerWg.Add(1)
		go worker(jobs_chan, results_chan)
	}
	var collectorWg sync.WaitGroup
	collectorWg.Add(1)

	// signal handler in go
	sigs := make(chan os.Signal, 1)
	// if a signal is received, noti
	sigs_exiting := make(chan bool, 1)
	signal.Notify(sigs, syscall.SIGINT)
	go func() {
		// collect results
		for job_result := range results_chan {
			select {
			case <-sigs:
				// if recived a SIGINT, stop collector and analyze rule loop
				if checkProgress {
					basic.PrintfWithTimeStamp("Ctrl C Pressed. Stop analysis")
				}
				// notifie the 'for i, rule := range rules' loop to exit
				sigs_exiting <- true
				collectorWg.Done()
				return
			default:
			}
			if job_result.Error != nil {
				glog.Errorf("while processing %v, analyzer.RunAnalyzeTask: %v", job_result.Rule.Name, job_result.Error)
				continue
			}
			customSeverity := ""
			if job_result.Rule.JSONOptions.Severity != nil {
				customSeverity = *job_result.Rule.JSONOptions.Severity
			}
			resultsWithSeverity := severity.AddSeverity(job_result.ResultsList, job_result.Rule.Name, customSeverity)
			allResults.Results = append(allResults.Results, resultsWithSeverity.Results...)
		}
		collectorWg.Done()
	}()
	for _, rule := range checkRules {
		select {
		// if recived a SIGINT, stop analyze rule loop
		case <-sigs_exiting:
			// close the jobs_chan to let worker end
			close(jobs_chan)
			collectorWg.Wait()
			// return results and errors directly because collector has stop.
			return allResults
		default:
		}
		jobs_chan <- &AnalyzerTask{
			Rule:                          rule,
			ResultDir:                     resultsDir,
			CompileCommandsPath:           compileCommandsPath,
			FilteredCompileCommandsFolder: filteredCompileCommandsFolder,
			SourceFiles:                   sourceFiles,
			CheckerConfig:                 checkerConfig,
			DumpErrors:                    dumpErrors,
		}
	}
	go func() {
		// wait for all worker to stop
		workerWg.Wait()
		// close results chan to let collector end
		close(results_chan)
	}()
	close(jobs_chan)
	// wait for the collector to collect all results
	collectorWg.Wait()
	glog.Infof("Finish Analyze")
	analyzeElapsed := time.Since(analyzeStart)
	if checkProgress {
		timeUsed := basic.FormatTimeDuration(analyzeElapsed)
		basic.PrintfWithTimeStamp(printer.Sprintf("Analysis completed [%s]", timeUsed))
	}
	return allResults
}

type CheckerScope int

const (
	Libtooling_STU CheckerScope = iota
	Libtooling_CTU
)

func RunCmdWithRuleName(compileCommandsPath, ruleName, resultsPath, resultDir, logDir, misraCheckerPath string, jsonOptions checkrule.JSONOption, buildActions *[]csa.BuildAction, sourceFiles []string, checkerConfig *pb.CheckerConfiguration, allResults *pb.ResultsList, checker checker_integration.Checker, limitMemory bool, timeoutNormal, timeoutOom int) error {
	allArgs := make([]string, 0)

	// Rule specific JSON options
	basic.FilterLibToolingExtraArgs(&allArgs, ruleName, jsonOptions)

	ruleNumber := strings.Split(ruleName, "/")[1]
	// TODO: make it work on Windows filesystems
	progName := filepath.Join(misraCheckerPath, ruleNumber)

	results, err := libtooling.ExecLibtooling(
		progName,
		resultsPath,
		resultDir,
		logDir,
		allArgs,
		buildActions,
		sourceFiles,
		checkerConfig,
		checker,
		limitMemory,
		timeoutNormal,
		timeoutOom,
		compileCommandsPath,
	)
	if err != nil {
		glog.Errorf("libtooling execution failed: %v", err)
		return err
	}

	allResults.Results = append(allResults.Results, results.Results...)
	return nil
}

func ExecMixture(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir, logDir, misraCheckerPath string,
	buildActions *[]csa.BuildAction, sourceFiles []string, config *pb.CheckerConfiguration, allResults *pb.ResultsList, limitMemory bool, timeoutNormal, timeoutOom int) []error {
	resultsList, errs := Exec(checkRule, compileCommandsPath, resultsDir, logDir, misraCheckerPath,
		buildActions, sourceFiles, config, limitMemory, timeoutNormal, timeoutOom)
	allResults.Results = append(allResults.Results, resultsList.Results...)
	if errs != nil {
		return errs
	}
	return nil
}

func resolveCheckerBinariesPath(checkRules []checkrule.CheckRule, config *pb.CheckerConfiguration, enableCodeChecker bool) error {
	binPathSet := []*string{
		&config.InferBin,
		&config.ClangBin,
		&config.CodeCheckerBin,
		&config.CppcheckBin,
		&config.PythonBin,
		&config.ClangtidyBin,
		&config.ClangqueryBin,
		&config.ClangmappingBin,
	}
	for _, binPath := range binPathSet {
		if *binPath == config.CodeCheckerBin && !enableCodeChecker {
			continue
		}
		resolvedBinPath, err := utils.ResovleBinaryPath(*binPath)
		if err != nil {
			glog.Fatalf("utils.ResovleBinaryPath: %v", err)
		}
		*binPath = resolvedBinPath
	}
	libtoolingBinPathSet := map[string]bool{}
	for _, rule := range checkRules {
		for _, checker := range checker_integration.RuleCheckerMap[rule.Name] {
			if checker == checker_integration.Libtooling_STU || checker == checker_integration.Libtooling_CTU {
				libtoolingBinPathSet[rule.Name] = true
			}
		}
	}
	for libtoolingBinPath := range libtoolingBinPathSet {
		ruleNumber := strings.Split(libtoolingBinPath, "/")[1]
		progName := filepath.Join(config.MisraCheckerPath, ruleNumber)
		_, err := utils.ResovleBinaryPath(progName)
		if err != nil {
			glog.Fatalf("utils.ResovleBinaryPath: %v", err)
		}
	}
	return nil
}
