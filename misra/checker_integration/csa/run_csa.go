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

package csa

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"syscall"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
)

type CheckFunction func(reportJson CSAReport) *pb.ResultsList

type Rule struct {
	Options string
	Check   CheckFunction
}

// If the error is known, do not exit and report detailed error messages in warning.
func containKnownError(s string) bool {
	knownErrors := []string{
		"error: cannot import unsupported AST node MacroQualified",
		"error: incompatible [a-zA-Z0-9\\s\\_]+ to [a-zA-Z0-9\\s\\_]+ conversion",
		"error: error opening '/output/ctu-dir/[a-zA-Z0-9\\s\\_]+/externalDefMap.txt': required by the CrossTU functionality"}
	for _, knownError := range knownErrors {
		knownErrorReg := regexp.MustCompile(knownError)
		if knownErrorReg.Match([]byte(s)) {
			return true
		}
	}
	return false
}

func genExternalSolverArgs(jsonOptions checkrule.JSONOption) []string {
	switch jsonOptions.UseZ3 {
	case "always":
		return []string{
			"-Xanalyzer", "-analyzer-constraints=z3",
		}
	case "crosscheck", "":
		return []string{
			"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "crosscheck-with-z3=true",
		}
	case "never":
		return []string{}
	default:
		glog.Errorf("unrecognized z3 option: %s", jsonOptions.UseZ3)
		return []string{}
	}
}

func runSingleCSA(
	action BuildAction,
	jsonOptions checkrule.JSONOption,
	taskName, checkerOptions, ctuBaseDir, resultsDir string,
	config *pb.CheckerConfiguration,
	limitMemory bool,
	timeoutNormal, timeoutOom int) (CSAReport, error) {
	entranceFile := action.Command.File
	// TODO: remove this config path hack
	systemLibOptions := config.CsaSystemLibOptions
	var reportJson CSAReport
	_, err := os.Stat(entranceFile)
	if err != nil {
		if os.IsNotExist(err) {
			glog.Warningf("in %s: %s does not exist", taskName, entranceFile)
		}
		return reportJson, nil
	}
	cmdRoot, err := os.Getwd()
	if err != nil {
		return reportJson, fmt.Errorf("get working dir err: %v", err)
	}

	ctuDir := filepath.Join(ctuBaseDir, action.Arch)

	if !filepath.IsAbs(ctuDir) {
		absCTUDir, err := filepath.Abs(ctuDir)
		if err != nil {
			glog.Errorf("in %s: failed to convert ctuDir %s to abspath: %v", taskName, ctuDir, err)
		} else {
			ctuDir = absCTUDir
		}
	}
	reportDir, err := os.MkdirTemp(filepath.Join(resultsDir, "csa-out"), "")
	if err != nil {
		return reportJson, err
	}
	reportPath := filepath.Join(reportDir, "report.json")
	cmd_arr := []string{"--analyze", "--analyzer-no-default-checks", "--analyzer-output", "sarif",
		"-o", reportPath, entranceFile,
		"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "experimental-enable-naive-ctu-analysis=true",
		"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "ctu-dir=" + ctuDir,
		"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "ctu-invocation-list=" + ctuDir + "/invocation-list.yml"}
	if action.Target != "" {
		cmd_arr = append(cmd_arr, "-target", action.Target)
	}

	csaAnalyzerInliningMode := jsonOptions.CSAAnalyzerInliningMode
	if csaAnalyzerInliningMode != nil {
		cmd_arr = append(cmd_arr, "-Xanalyzer", "-analyzer-inlining-mode="+*csaAnalyzerInliningMode)
	}

	maxLoop := jsonOptions.MaxLoop
	if maxLoop != nil {
		cmd_arr = append(cmd_arr, "-Xanalyzer", "-analyzer-max-loop", "-Xanalyzer", strconv.Itoa(*maxLoop))
	}
	maxNodes := jsonOptions.MaxNodes
	if maxNodes != nil {
		cmd_arr = append(cmd_arr, "-Xanalyzer", "-analyzer-config", "-Xanalyzer", "max-nodes="+strconv.Itoa(*maxNodes))
	}
	// this cmd is only for cwe 789
	if checkerOptions == "-analyzer-checker=cwe.ExcessiveMemoryAllocation" {
		MaximumAllowedSize := jsonOptions.MaximumAllowedSize
		if MaximumAllowedSize != nil {
			cmd_arr = append(cmd_arr, "-Xanalyzer", "-analyzer-config", "-Xanalyzer", "cwe.ExcessiveMemoryAllocation:MaximumAllowedSize="+strconv.Itoa(*MaximumAllowedSize))
		}
	}

	// this cmd arg is only for dir 4.14
	if checkerOptions == "-analyzer-checker=misra_c_2012.TaintMisra" {
		err = AddYamlConfigPathToRunCmd(jsonOptions, resultsDir, &cmd_arr, "misra_c_2012.TaintMisra", "conf.yaml")
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
			return reportJson, err
		}
	}

	// this cmd arg is only for cwe 252
	if checkerOptions == "-analyzer-checker=cwe.UncheckedReturnValue" {
		err = AddYamlConfigPathToRunCmd(jsonOptions, resultsDir, &cmd_arr, "cwe.UncheckedReturnValue", "conf-cwe-252.yaml")
		if err != nil {
			glog.Errorf("in %s: %v", taskName, err)
			return reportJson, err
		}
	}

	// enable/disable external solver z3
	cmd_arr = append(cmd_arr, genExternalSolverArgs(jsonOptions)...)

	preprocessorSymbols := ParsePreprocessorSymbols(action, true, false)
	cmd_arr = append(cmd_arr, preprocessorSymbols...)
	options_summary := append(cmd_arr, strings.Fields(systemLibOptions)...)
	for _, opt := range strings.Fields(checkerOptions) {
		options_summary = append(options_summary, "-Xanalyzer", opt)
	}
	options_summary = append(options_summary, ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), false)...)

	runClangInDir := func(dir string) (knownError bool, errMessage string, err error) {
		cmd := exec.Command(config.ClangBin, options_summary...)
		cmd.Dir = dir
		glog.Infof("in %s, cd to :%s", taskName, cmd.Dir)
		glog.Infof("in %s, executing: %s", taskName, cmd.String())
		if out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom); err != nil {
			outStr := string(out)
			knownError := containKnownError(outStr)
			errMessage := fmt.Sprintf("in %s, cd to %s, executing:\n%s\nreported:\n%s", taskName, cmd.Dir, cmd.String(), outStr)
			return knownError, errMessage, err
		}
		return false, "", nil
	}
	if knownError, errMessage, err := runClangInDir(action.Command.Directory); err != nil {
		if action.Command.Directory != cmdRoot {
			// try to run clang--analyze in project root, see issue 6598
			glog.Warningf(errMessage)
			knownError, errMessage, err = runClangInDir(cmdRoot)
		}
		if err != nil && !knownError {
			glog.Errorf(errMessage)
			return reportJson, err
		} else if err != nil {
			// some errors are skipped, see issue 6646
			glog.Warningf(errMessage)
		}
	}
	reportFile, err := os.Open(reportPath)
	if err != nil {
		return reportJson, err
	}
	defer reportFile.Close()

	reportContent, err := io.ReadAll(reportFile)
	if err != nil {
		return reportJson, err
	}
	err = json.Unmarshal(reportContent, &reportJson)
	if err != nil {
		return reportJson, err
	}
	return reportJson, nil
}

type CSAResult struct {
	Filename   string
	ReportJson CSAReport
	Error      error
}

func GetCSAReport(compileCommandsPath, checkerOptions, ctuBaseDir, resultsDir string, jsonOptions checkrule.JSONOption, buildActions *[]BuildAction, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int, disableParallelism bool) (CSAReport, error) {
	taskName := filepath.Base(resultsDir)
	var allReportJson CSAReport
	allReportsDir := filepath.Join(resultsDir, "csa-out")
	err := os.MkdirAll(allReportsDir, os.ModePerm)
	if err != nil {
		return allReportJson, err
	}

	if disableParallelism {
		for _, action := range *buildActions {
			reportJson, err := runSingleCSA(
				action,
				jsonOptions,
				taskName,
				checkerOptions,
				ctuBaseDir,
				resultsDir,
				config,
				limitMemory,
				timeoutNormal,
				timeoutOom,
			)
			if err != nil {
				glog.Errorf("%s: when analyzing %s, runSingleCSA: %v", taskName, action.Command.File, err)
				continue
			}
			allReportJson.Runs = append(allReportJson.Runs, reportJson.Runs...)
		}
		return allReportJson, nil
	}

	numWorkers := config.NumWorkers
	if numWorkers == 0 {
		numWorkers = int32(runtime.NumCPU())
	}
	var workerWg sync.WaitGroup
	worker := func(jobs <-chan BuildAction, results chan<- *CSAResult) {
		for j := range jobs {
			func() {
				reportJson, err := runSingleCSA(
					j,
					jsonOptions,
					taskName,
					checkerOptions,
					ctuBaseDir,
					resultsDir,
					config,
					limitMemory,
					timeoutNormal,
					timeoutOom,
				)
				results <- &CSAResult{Filename: j.Command.File, ReportJson: reportJson, Error: err}
			}()
		}
		workerWg.Done()
	}

	jobs_chan := make(chan BuildAction, numWorkers)
	results_chan := make(chan *CSAResult, numWorkers)
	for w := 0; w < int(numWorkers); w++ {
		workerWg.Add(1)
		go worker(jobs_chan, results_chan)
	}
	var collectorWg sync.WaitGroup
	collectorWg.Add(1)

	// signal handler in go
	sigs := make(chan os.Signal, 1)
	// if a signal is received, notify the loop to stop sending new workers
	sigs_exiting := make(chan bool, 1)
	signal.Notify(sigs, syscall.SIGINT)
	go func() {
		// collect results
		for job_result := range results_chan {
			select {
			case <-sigs:
				sigs_exiting <- true
				collectorWg.Done()
				return
			default:
			}
			if job_result.Error != nil {
				glog.Errorf("%s: when analyzing %s, runSingleCSA: %v", taskName, job_result.Filename, job_result.Error)
				continue
			}
			allReportJson.Runs = append(allReportJson.Runs, job_result.ReportJson.Runs...)
		}
		collectorWg.Done()
	}()
	for _, action := range *buildActions {
		select {
		// if received a SIGINT, stop analyze rule loop
		case <-sigs_exiting:
			// close the jobs_chan to let worker end
			close(jobs_chan)
			collectorWg.Wait()
			// return results and errors directly because collector has stop.
			return allReportJson, err
		default:
		}
		jobs_chan <- action
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
	return allReportJson, err
}

// this lambda is only for csa yaml file.
func AddYamlConfigPathToRunCmd(jsonOptions checkrule.JSONOption, resultsDir string, cmd_arr *[]string, checker string, yamlConfigPath string) error {
	taintConfFilePath, err := jsonOptions.GenerateTaintAnalyzerConf(resultsDir, yamlConfigPath)
	if err != nil {
		return err
	}
	if taintConfFilePath != "" {
		taintConfArg := checker + ":Config=" + taintConfFilePath
		*cmd_arr = append(*cmd_arr, "-Xanalyzer", "-analyzer-config", "-Xanalyzer", taintConfArg)
	}
	return nil
}

func CheckDir4_11(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "alpha.unix.StdCLibraryFunctionArgs" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_DIR_4_11
				result.ErrorMessage = "[C2314][misra-c2012-dir-4.11]: violation of misra-c2012-dir-4.11\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckDir4_14(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.TaintMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_DIR_4_14
				result.ErrorMessage = "[C2317][misra-c2012-dir-4.14]: violation of misra-c2012-dir-4.14\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule1_3(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "alpha.security.ArrayBoundV2" ||
				resultsContent.RuleId == "core.DivideZero" ||
				resultsContent.RuleId == "core.NullDereference" ||
				resultsContent.RuleId == "unix.Malloc" ||
				resultsContent.RuleId == "alpha.security.ReturnPtrRange" ||
				resultsContent.RuleId == "core.StackAddressEscape" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_1_3
				result.ErrorMessage = "[C2203][misra-c2012-1.3]: violation of misra-c2012-1.3"
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule2_1(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "alpha.deadcode.UnreachableCode" && strings.Contains(resultsContent.Message.Text, "never executed") {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_2_1
				result.ErrorMessage = "[C2007][misra-c2012-2.1]: violation of misra-c2012-2.1\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule2_2(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "deadcode.DeadStores" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_2_2_EXTERNAL
				result.ErrorMessage = "[C2006][misra-c2012-2.2]: violation of misra-c2012-2.2\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule9_1(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	RuleIds := []string{
		"core.UndefinedBinaryOperatorResult",
		"core.uninitialized.ArraySubscript",
		"core.uninitialized.Assign",
		"core.uninitialized.Branch",
		"core.uninitialized.CapturedBlockVariable",
		"core.uninitialized.UndefReturn",
	}
	sort.Strings(RuleIds)
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			index := sort.SearchStrings(RuleIds, resultsContent.RuleId)
			if index < len(RuleIds) && RuleIds[index] == resultsContent.RuleId {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_9_1
				result.ErrorMessage = "[C1205][misra-c2012-9.1]: violation of misra-c2012-9.1\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule11_5(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.VoidToObjectPtr" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_11_5
				result.ErrorMessage = "[C1405][misra-c2012-11.5]: violation of misra-c2012-11.5\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule12_2(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.ShiftOp" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_12_2
				result.ErrorMessage = "[C0604][misra-c2012-12.2]: violation of misra-c2012-12.2\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule13_2(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.VarUnsequencedAccess" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_13_2_EXTERNAL
				result.ErrorMessage = "[C1605][misra-c2012-13.2]: violation of misra-c2012-13.2\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule14_1(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "security.FloatLoopCounter" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_14_1_CSA
				result.ErrorMessage = "[C1704][misra-c2012-14.1]: violation of misra-c2012-14.1\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule18_1(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.PointerArithMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_18_1
				result.ErrorMessage = "[C1308][misra-c2012-18.1]: Pointer arithmetic may cause array out of bound.\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule18_2(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.PointerSubMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_18_2
				result.ErrorMessage = "[C1307][misra-c2012-18.2]: violation of misra-c2012-18.2\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule18_3(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.PointerComp" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_18_3
				result.ErrorMessage = "[C1306][misra-c2012-18.3]: violation of misra-c2012-18.3\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule18_6(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "core.StackAddressEscape" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_18_6
				result.ErrorMessage = "[C1303][misra-c2012-18.6]: violation of misra-c2012-18.6\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule19_1(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "alpha.unix.cstring.BufferOverlap" || resultsContent.RuleId == "misra_c_2012.AssignmentOverlap" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_19_1_EXTERNAL
				result.ErrorMessage = "[C0302][misra-c2012-19.1]: violation of misra-c2012-19.1, assign or copy is overlapped\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule21_13(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.CtypeFunctionArgs" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_13
				result.ErrorMessage = "[C0408][misra-c2012-21.13]: violation of misra-c2012-21.13\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule21_14(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.MemcmpBufferArgument" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_14
				result.ErrorMessage = "[C0407][misra-c2012-21.14]: violation of misra-c2012-21.14\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule21_17(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.CStringBoundMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_17
				result.ErrorMessage = "[C0404][misra-c2012-21.17]: The cstring function call may cause read or write out of bound.\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule21_18(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "alpha.unix.cstring.OutOfBounds" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_18_EXTERNAL
				result.ErrorMessage = "[C0403][misra-c2012-21.18]: violation of misra-c2012-21.18\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule21_19(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.ConstPointerReturn" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_19_EXTERNAL
				result.ErrorMessage = "[C0402][misra-c2012-21.19]: violation of misra-c2012-21.19\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule21_20(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.DeprecatedResource" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_20
				result.ErrorMessage = "[C0401][misra-c2012-21.20]: violation of misra-c2012-21.20\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_1(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "unix.Malloc" &&
				(strings.Contains(resultsContent.Message.Text, "Potential leak of memory") ||
					// _bad0009 reports "Potential memory leak [unix.Malloc]"
					strings.Contains(resultsContent.Message.Text, "Potential memory leak")) {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_1
				result.ErrorMessage = "[C0210][misra-c2012-22.1]: violation of misra-c2012-22.1\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_2(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "unix.Malloc" && (resultsContent.Message.Text == "Attempt to free released memory" || strings.Contains(resultsContent.Message.Text, "Argument to free() is the address of")) {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_2
				result.ErrorMessage = "[C0209][misra-c2012-22.2]: violation of misra-c2012-22.2\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_3(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.OpenSameFile" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_3
				result.ErrorMessage = "[C0208][misra-c2012-22.3]: violation of misra-c2012-22.3\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_4(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.WriteReadOnly" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_4
				result.ErrorMessage = "[C0207][misra-c2012-22.4]: violation of rule 22.4\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_5(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.FileDereference" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_5
				result.ErrorMessage = "[C0206][misra-c2012-22.5]: violation of rule 22.5\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_6(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.UseClosedFile" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_6
				result.ErrorMessage = "[C0205][misra-c2012-22.6]: violation of rule 22.6\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_7(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.EOFComparison" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_7
				result.ErrorMessage = "[C0204][misra-c2012-22.7]: violation of rule 22.7\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_8(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.errno.SetErrnoMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_8
				result.ErrorMessage = "[C0203][misra-c2012-22.8]: violation of misra-c2012-22.8\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_9(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.errno.TestErrnoMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_9
				result.ErrorMessage = "[C0202][misra-c2012-22.9]: violation of misra-c2012-22.9\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func CheckRule22_10(reportJson CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		results := run.Results
		for _, resultsContent := range results {
			if resultsContent.RuleId == "misra_c_2012.errno.MisusedTestErrnoMisra" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_10
				result.ErrorMessage = "[C0201][misra-c2012-22.10]: violation of misra-c2012-22.10\n" + resultsContent.Message.Text
				result.ExternalMessage = resultsContent.Message.Text
				resultsList.Results = append(resultsList.Results, result)
			}
		}
	}
	return resultsList
}

func GetRuleMap() map[string]Rule {
	ruleMap := map[string]Rule{
		"misra_c_2012/dir_4_11": {
			Options: "-analyzer-checker=alpha.unix.StdCLibraryFunctionArgs",
			Check:   CheckDir4_11,
		},
		"misra_c_2012/dir_4_14": {
			Options: "-analyzer-checker=misra_c_2012.TaintMisra",
			Check:   CheckDir4_14,
		},
		"misra_c_2012/rule_1_3": {
			Options: "-analyzer-checker=alpha.security.ArrayBoundV2 -analyzer-checker=core.DivideZero -analyzer-checker=core.NullDereference -analyzer-checker=unix.Malloc -analyzer-checker=alpha.security.ReturnPtrRange -analyzer-checker=core.StackAddressEscape",
			Check:   CheckRule1_3,
		},
		"misra_c_2012/rule_2_1": {
			Options: "-analyzer-checker=alpha.deadcode.UnreachableCode",
			Check:   CheckRule2_1,
		},
		"misra_c_2012/rule_2_2": {
			Options: "-analyzer-checker=deadcode.DeadStores",
			Check:   CheckRule2_2,
		},
		"misra_c_2012/rule_9_1": {
			Options: "-analyzer-checker=core.UndefinedBinaryOperatorResult -analyzer-checker=core.uninitialized",
			Check:   CheckRule9_1,
		},
		"misra_c_2012/rule_11_5": {
			Options: "-analyzer-checker=misra_c_2012.VoidToObjectPtr",
			Check:   CheckRule11_5,
		},
		"misra_c_2012/rule_12_2": {
			Options: "-analyzer-checker=misra_c_2012.ShiftOp",
			Check:   CheckRule12_2,
		},
		"misra_c_2012/rule_13_2": {
			Options: "-analyzer-checker=misra_c_2012.VarUnsequencedAccess",
			Check:   CheckRule13_2,
		},
		"misra_c_2012/rule_14_1": {
			Options: "-analyzer-checker=security.FloatLoopCounter",
			Check:   CheckRule14_1,
		},
		"misra_c_2012/rule_18_1": {
			Options: "-analyzer-checker=misra_c_2012.PointerArithMisra",
			Check:   CheckRule18_1,
		},
		"misra_c_2012/rule_18_2": {
			Options: "-analyzer-checker=misra_c_2012.PointerSubMisra",
			Check:   CheckRule18_2,
		},
		"misra_c_2012/rule_18_3": {
			Options: "-analyzer-checker=misra_c_2012.PointerComp",
			Check:   CheckRule18_3,
		},
		"misra_c_2012/rule_18_6": {
			Options: "-analyzer-checker=core.StackAddressEscape",
			Check:   CheckRule18_6,
		},
		"misra_c_2012/rule_19_1": {
			Options: "-analyzer-checker=alpha.unix.cstring.BufferOverlap",
			Check:   CheckRule19_1,
		},
		"misra_c_2012/rule_21_13": {
			Options: "-analyzer-checker=misra_c_2012.CtypeFunctionArgs",
			Check:   CheckRule21_13,
		},
		"misra_c_2012/rule_21_14": {
			Options: "-analyzer-checker=misra_c_2012.MemcmpBufferArgument",
			Check:   CheckRule21_14,
		},
		"misra_c_2012/rule_21_17": {
			Options: "-analyzer-checker=misra_c_2012.CStringBoundMisra",
			Check:   CheckRule21_17,
		},
		"misra_c_2012/rule_21_18": {
			Options: "-analyzer-checker=core,alpha.unix.cstring.OutOfBounds",
			Check:   CheckRule21_18,
		},
		"misra_c_2012/rule_21_19": {
			Options: "-analyzer-checker=misra_c_2012.ConstPointerReturn",
			Check:   CheckRule21_19,
		},
		"misra_c_2012/rule_21_20": {
			Options: "-analyzer-checker=misra_c_2012.DeprecatedResource",
			Check:   CheckRule21_20,
		},
		"misra_c_2012/rule_22_1": {
			Options: "-analyzer-checker=unix.Malloc",
			Check:   CheckRule22_1,
		},
		"misra_c_2012/rule_22_2": {
			Options: "-analyzer-checker=unix.Malloc",
			Check:   CheckRule22_2,
		},
		"misra_c_2012/rule_22_3": {
			Options: "-analyzer-checker=misra_c_2012.OpenSameFile",
			Check:   CheckRule22_3,
		},
		"misra_c_2012/rule_22_4": {
			Options: "-analyzer-checker=misra_c_2012.WriteReadOnly",
			Check:   CheckRule22_4,
		},
		"misra_c_2012/rule_22_5": {
			Options: "-analyzer-checker=misra_c_2012.FileDereference",
			Check:   CheckRule22_5,
		},
		"misra_c_2012/rule_22_6": {
			Options: "-analyzer-checker=misra_c_2012.UseClosedFile",
			Check:   CheckRule22_6,
		},
		"misra_c_2012/rule_22_7": {
			Options: "-analyzer-checker=misra_c_2012.EOFComparison",
			Check:   CheckRule22_7,
		},
		"misra_c_2012/rule_22_8": {
			Options: "-analyzer-checker=misra_c_2012.errno.SetErrnoMisra",
			Check:   CheckRule22_8,
		},
		"misra_c_2012/rule_22_9": {
			Options: "-analyzer-checker=misra_c_2012.errno.TestErrnoMisra",
			Check:   CheckRule22_9,
		},
		"misra_c_2012/rule_22_10": {
			Options: "-analyzer-checker=misra_c_2012.errno.MisusedTestErrnoMisra",
			Check:   CheckRule22_10,
		},
	}
	return ruleMap
}

// Deprecated, use cruleslib/runner/runner.go: RunCSA()
func Exec(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir string, buildActions *[]BuildAction, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	ruleMap := GetRuleMap()
	rule, exist := ruleMap[checkRule.Name]
	if !exist {
		return nil, errors.New("unrecognized rule name " + checkRule.Name)
	}
	ctuBaseDir := filepath.Join(resultsDir, "ctu-dir")
	reportJson, err := GetCSAReport(compileCommandsPath, rule.Options, ctuBaseDir, resultsDir, checkRule.JSONOptions, buildActions, config, limitMemory, timeoutNormal, timeoutOom /* disableParallelism = */, false)
	if err != nil {
		return nil, err
	}
	resultsList := rule.Check(reportJson)
	return resultsList, err
}
