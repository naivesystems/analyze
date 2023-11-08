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

package main

//go:generate ../../../out/bin/gotext -tags static -srclang=en update -out=catalog.go -lang=en,zh

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/golang/glog"
	"golang.org/x/text/message"
	pb "naive.systems/analyzer/analyzer/proto"
	autosar "naive.systems/analyzer/autosar/analyzer"
	"naive.systems/analyzer/cruleslib/baseline"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/cruleslib/filter"
	"naive.systems/analyzer/cruleslib/i18n"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/stats"
	googlecpp "naive.systems/analyzer/googlecpp/analyzer"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	misra_c_2012_crules "naive.systems/analyzer/misra_c_2012_crules/analyzer"
	misra_cpp_2008 "naive.systems/analyzer/misra_cpp_2008/analyzer"
)

var compileCommandsPath = "/src/compile_commands.json"

var linesLimitStr string = "0"
var numWorkersStr string = "0"
var lang string = "zh"

var ruleSets = []string{
	"googlecpp", "misra_cpp_2008", "autosar", "misra_c_2012"}

func isCOnly(rule string) bool {
	switch rule {
	case "misra_c_2012":
		return true
	}
	return false
}

func isCPPOnly(rule string) bool {
	switch rule {
	case "googlecpp", "misra_cpp_2008", "autosar":
		return true
	}
	return false
}

func main() {
	sharedOptions := options.NewSharedOptions()
	flag.Parse()
	defer glog.Flush()

	// Do not call any logging functions of glog before this part.
	sharedOptions.SetLang(lang)
	printer := i18n.GetPrinter(sharedOptions.GetLang())

	logDir := flag.Lookup("log_dir")
	if logDir.Value.String() == "" {
		err := flag.Set("log_dir", filepath.Join(sharedOptions.GetResultsDir(), "logs"))
		if err != nil {
			glog.Fatalf("failed to set default log_dir: %v", err)
		}
	}
	err := analyzerinterface.CreateLogDir(logDir.Value.String())
	if err != nil {
		glog.Fatalf("failed to create log dir: %v", err)
	}

	if !sharedOptions.GetDebugMode() {
		err := flag.Set("stderrthreshold", "FATAL")
		if err != nil {
			glog.Fatalf("failed to set default stderrthreshold: %v", err)
		}
	}

	fmt.Println("(c) 2023 Naive Systems Ltd.")

	if sharedOptions.GetCheckProgress() {
		basic.PrintfWithTimeStamp(printer.Sprintf("Start to generate compilation database"))
		stats.WriteProgress(sharedOptions.GetResultsDir(), stats.CC, "0%", time.Now())
	}

	numWorkers, err := options.ParseLimitMemory(sharedOptions, numWorkersStr)
	if err != nil {
		glog.Fatalf("options.ParseLimitMemory: %v", err)
	}

	glog.Info("numWorkers: ", numWorkers)
	glog.Info("configDir: ", sharedOptions.GetConfigDir())
	glog.Info("checkerConfig: ", sharedOptions.GetCheckerConfig())

	err = analyzerinterface.CreateResultDir(sharedOptions.GetResultsDir())
	if err != nil {
		glog.Fatalf("failed to create result dir: %v", err)
	}

	resultsPath := filepath.Join(sharedOptions.GetResultsDir(), "results")
	resultsWithSuffixPath := filepath.Join(sharedOptions.GetResultsDir(), "results.nsa_results")
	resultsJsonPath := filepath.Join(sharedOptions.GetResultsDir(), "nsa_results.json")
	if !filepath.IsAbs(sharedOptions.GetConfigDir()) {
		glog.Fatal("configDir must be an absolute path")
	}

	if sharedOptions.GetSrcDir() != "/src" {
		srcDir := filepath.Join("/src", sharedOptions.GetSrcDir())
		compileCommandsPath = filepath.Join(srcDir, analyzerinterface.CCJson)
		sharedOptions.SetSrcDir(srcDir)
	}

	projType, err := options.ParseProjectType(sharedOptions)
	if err != nil {
		glog.Fatalf("options.ParseProjectType: %v", err)
	}
	if projType == analyzerinterface.Keil {
		options.ProcessKeilProject(sharedOptions)
	}

	shouldCreateNew := (!sharedOptions.GetSkipBearMake() && (projType == analyzerinterface.Make || projType == analyzerinterface.CMake || projType == analyzerinterface.QMake || projType == analyzerinterface.Script))
	onlyCppcheck, err := analyzerinterface.CheckCompilationDatabase(
		compileCommandsPath,
		shouldCreateNew,
		projType,
		sharedOptions.GetSrcDir(),
		sharedOptions.GetQmakeBin(),
		sharedOptions.GetQtProPath(),
		sharedOptions.GetScriptContents(),
		sharedOptions.GetAllowBuildFail(),
		/*isDev=*/ false,
	)
	if err != nil {
		if os.IsNotExist(err) {
			glog.Fatal("Compilation database not found, consider removing `-skip_bear_make`?")
		} else {
			glog.Fatal(err)
		}
	}
	err = filter.DeleteCompileCommandsFromCCJson(compileCommandsPath, sharedOptions.GetKeepBuildActionPercent())
	if err != nil {
		glog.Errorf("DeleteCompileCommandsFromCCJson failed")
	}

	// It will report a fatal error, when:
	// 1) if there is neither clines nor cpplines.
	// 2) if the number of code lines exceed the maximum limit.
	clines, cpplines, err := options.CheckCodeLines(compileCommandsPath, sharedOptions, linesLimitStr)
	if err != nil {
		glog.Fatalf("options.CheckCodeLines: %v", err)
	}

	// TODO: deprecated checker config
	parsedCheckerConfig := options.ParseCheckerConfig(sharedOptions, numWorkers, cpplines, projType)
	glog.Info("parsedCheckerConfig: ", parsedCheckerConfig)

	start := time.Now()

	if sharedOptions.GetCheckProgress() {
		basic.PrintfWithTimeStamp(printer.Sprintf("Start parsing compilation database"))
		stats.WriteProgress(sharedOptions.GetResultsDir(), stats.PP, "0%", start)
	}
	// TODO: there still some duplicated action which remove ignore files twice.
	compileCommandsPath, err := analyzerinterface.GenerateActualCCJsonByRemoveIgnoreFiles(compileCommandsPath, sharedOptions.GetIgnoreDirPatterns())
	if err != nil {
		glog.Fatalf("analyzerinterface.GenerateActualCCJsonByRemoveIgnoreFiles(: %v", err)
	}
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath)
	if err != nil {
		glog.Fatalf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)

	// TODO: analyze the build process to get a list of "systems"
	checkRulesPath := filepath.Join(sharedOptions.GetConfigDir(), "check_rules")
	checkRules, err := analyzerinterface.ReadCheckRules(checkRulesPath)
	if err != nil {
		glog.Errorf("failed to read check rules from %s: %v", checkRulesPath, err)
	}

	var allResults *pb.ResultsList
	standardSet, err := checkrule.ConstructStandardSet(checkRules)
	if err != nil {
		glog.Fatalf("failed to construct standard set: %v", err)
	}

	err = analyzerinterface.CleanResultDir(sharedOptions.GetResultsDir())
	if err != nil {
		glog.Errorf("failed to clean log and result dir: %v", err)
	}

	csaSystemLibOptionsCopy := parsedCheckerConfig.CsaSystemLibOptions

	checkerPathsMap := map[string]string{}
	for _, ruleSet := range ruleSets {
		if ruleSet == "misra_c_2012" {
			checkerPathsMap["misra"] = filepath.Join(sharedOptions.GetCheckerPathRoot(), "misra")
			continue
		}
		checkerPathsMap[ruleSet] = filepath.Join(sharedOptions.GetCheckerPathRoot(), ruleSet)
	}

	for _, ruleSet := range ruleSets {
		if isCOnly(ruleSet) && projType == analyzerinterface.Keil {
			parsedCheckerConfig.CsaSystemLibOptions = *options.ConcatStringField("-isystem /usr/include", &parsedCheckerConfig.CsaSystemLibOptions)
		} else {
			parsedCheckerConfig.CsaSystemLibOptions = csaSystemLibOptionsCopy
		}

		if (isCOnly(ruleSet) && clines == 0) || (isCPPOnly(ruleSet) && cpplines == 0) {
			continue
		}

		filteredCheckRules := analyzerinterface.FilterCheckRules(checkRules, ruleSet)

		if len(filteredCheckRules) > 0 {
			start := time.Now()
			basic.PrintfWithTimeStamp(printer.Sprintf("Start preprocessing C/C++ files"))
			envOptions := options.NewEnvOptionsFromShared(
				standardSet,
				logDir.Value.String(),
				checkerPathsMap,
				sharedOptions,
				parsedCheckerConfig,
				/*ignoreCpp=*/ isCOnly(ruleSet),
				numWorkers,
				onlyCppcheck,
			)
			elapsed := time.Since(start)
			timeUsed := basic.FormatTimeDuration(elapsed)
			basic.PrintfWithTimeStamp(printer.Sprintf("Preprocessing C/C++ files completed [%s]", timeUsed))

			results := checkRuleSet(sharedOptions.GetSrcDir(), ruleSet, filteredCheckRules, envOptions, printer)

			if results == nil {
				continue
			}

			if ruleSet == "misra_cpp_2008" {
				checker_integration.DeleteCResults(results)
			}

			results = analyzerinterface.FormatResultPath(results, sharedOptions.GetSrcDir())

			if ruleSet == "misra_c_2012" {
				results = filter.DeleteCppResults(results)
			}

			if ruleSet == "misra_cpp_2008" || ruleSet == "misra_c_2012" {
				i18n.LocalizeErrorMessages(results, sharedOptions.GetLang())
			}

			err = checker_integration.DeleteRepeatedResults(results)
			if err != nil {
				glog.Fatal(err)
			}

			results = analyzerinterface.ProcessIgnoreDir(results, &sharedOptions.IgnoreDirPatterns)
			results = filter.DeleteExceedResults(results, filteredCheckRules)

			if allResults == nil {
				allResults = results
			} else {
				allResults.Results = append(allResults.Results, results.Results...)
			}
		}
	}

	if allResults == nil {
		glog.Warning("No code can be analyzed with the chosen rules.")
		allResults = &pb.ResultsList{}
	}
	allResults = baseline.RemoveDuplicatedResults(
		allResults,
		sharedOptions.GetSrcDir(),
		sharedOptions.GetConfigDir(),
		sharedOptions.GetResultsDir())
	baseline.SortResults(allResults)

	suppressionDir := filepath.Join(sharedOptions.GetConfigDir(), "suppression")
	stat, err := os.Stat(suppressionDir)
	if err == nil && stat.IsDir() {
		allResults, err = analyzerinterface.ProcessSuppression(allResults, suppressionDir)
		if err != nil {
			glog.Errorf("ProcessSuppression: %v", err)
		}
	}

	if sharedOptions.GetAddLineHash() {
		analyzerinterface.AddCodeLineHash(allResults)
	}

	analyzerinterface.AddID(allResults)

	// write results
	err = analyzerinterface.WriteResults(allResults, resultsPath)
	if err != nil {
		glog.Fatal(err)
	}
	err = analyzerinterface.WriteResults(allResults, resultsWithSuffixPath)
	if err != nil {
		glog.Fatal(err)
	}
	if sharedOptions.GetShowJsonResults() {
		err = analyzerinterface.WriteJsonResults(allResults, resultsJsonPath)
		if err != nil {
			glog.Fatal(err)
		}
	}

	// count results by severity and save stats to severity_stats.nsa_metadata
	stats.CountSeverityAndWrite(allResults, sharedOptions.GetResultsDir())

	err = analyzerinterface.GenerateReport(allResults, sharedOptions.GetSrcDir(), filepath.Join(sharedOptions.GetResultsDir(), "report.json"), sharedOptions.GetLang())
	if err != nil {
		glog.Errorf("failed to generate report: %v", err)
	}

	glog.Infof("All results have been written to %s and %s (%d in total), exit. ", resultsPath, resultsWithSuffixPath, len(allResults.Results))
	if sharedOptions.GetShowResults() {
		analyzerinterface.PrintResults(allResults, sharedOptions.GetShowResultsCount())
	}

	elapsed := time.Since(start)
	if sharedOptions.GetCheckProgress() {
		timeUsed := basic.FormatTimeDuration(elapsed)
		basic.PrintfWithTimeStamp(printer.Sprintf("Total time for analysis: %s", timeUsed))
	}

	// clean generated file
	files, err := os.ReadDir(".")
	if err != nil {
		glog.Warning(err)
	}
	for _, file := range files {
		filename := file.Name()
		if strings.HasSuffix(filename, "compile_commands.json") {
			cmd := exec.Command("mv", []string{filename, fmt.Sprintf("/output/%s", filename)}...)
			err = cmd.Run()
			if err != nil {
				glog.Warning(err)
			}
		}
	}

	// tar logs folder
	err = basic.TarFile(logDir.Value.String(), filepath.Join(sharedOptions.GetResultsDir(), "logs.tar.gz"))
	if err != nil {
		glog.Errorf("failed to compress log files: %v", err)
	}
	glog.Flush()
}

type runFuncType = func([]checkrule.CheckRule, string, *options.EnvOptions) (*pb.ResultsList, []error)

func selectRun(rulePrefix string) (runFuncType, error) {
	switch rulePrefix {
	case "googlecpp":
		return googlecpp.Run, nil
	case "autosar":
		return autosar.Run, nil
	case "misra_c_2012", "naivesystems":
		return misra_c_2012_crules.Run, nil
	case "misra_cpp_2008":
		return misra_cpp_2008.Run, nil
	default:
		return nil, fmt.Errorf("No such rule runner found: %s", rulePrefix)
	}
}

func checkRuleSet(
	srcdir string,
	rulePrefix string,
	checkRules []checkrule.CheckRule,
	envOptions *options.EnvOptions,
	printer *message.Printer) *pb.ResultsList {
	if len(checkRules) > 0 {
		if envOptions == nil {
			glog.Infof("nothing to check for %s rules", strings.ToUpper(rulePrefix))
		} else {
			start := time.Now()
			if envOptions.CheckProgress {
				basic.PrintfWithTimeStamp(printer.Sprintf("Start analyzing C/C++ files"))
				stats.WriteProgress(envOptions.ResultsDir, stats.AC, "0%", start)
			}
			runner, error := selectRun(rulePrefix)
			if error != nil {
				glog.Fatal(error)
			}
			// TODO: handle onlyCppcheck
			results, errors := runner(checkRules, srcdir, envOptions)
			for _, error := range errors {
				if error != nil {
					glog.Errorf("errors occur while analyzing: %v", errors)
				}
			}
			elapsed := time.Since(start)
			if envOptions.CheckProgress {
				timeUsed := basic.FormatTimeDuration(elapsed)
				basic.PrintfWithTimeStamp(printer.Sprintf("Analyzing C/C++ files completed [%s]", timeUsed))
			}
			return results
		}
	}
	return nil
}
