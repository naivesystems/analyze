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

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/golang/glog"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cpumem"
	"naive.systems/analyzer/misra/analyzer"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/utils"
)

func main() {
	compileCommandsPath := flag.String("compile_commands_path", "", "Absolute path to the compile_commands file")
	checkRules := flag.String("check_rules", "", "Rule numbers to be analyzed, like misra_c_2012/rule_22_1, misra_c_2012/rule_22_2")
	resultsDir := flag.String("results_dir", "/output/", "Absolute path to the directory of results files")
	checkerConfig := flag.String("checker_config",
		`{"csa_system_lib_options":"-isystem /usr/lib/gcc/x86_64-redhat-linux/11/include/",
		  "infer_bin":"infer",
		  "clang_bin":"bazel-bin/external/llvm-project/clang/clang",
		  "code_checker_bin":"CodeChecker",
		  "cppcheck_bin":"third_party/cppcheck/cppcheck",
		  "python_bin":"python3",
		  "clangtidy_bin":"clang-tidy",
		  "clangquery_bin":"clang-query",
		  "infer_extra_options":"--max-nesting=3 --bo-field-depth-limit=6",
		  "clangmapping_bin":"bazel-bin/external/llvm-project/clang/clang-extdef-mapping",
		  "infer_jobs":8}`,
		"Checker configuration in JSON format")
	jsonOptions := flag.String("json_options", "{}", "Json options for the analyzer.")
	checkProgress := flag.Bool("check_progress", true, "Show the checking progress")
	enableCodeChecker := flag.Bool("enable_codechecker", false, "Use CodeChecker to perform CTU pre-analysis for CSA")
	var ignoreDirPatterns analyzerinterface.ArrayFlags
	flag.Var(&ignoreDirPatterns, "ignore_dir", "Shell file name pattern to a directory that will be ignored")
	flag.Parse()
	defer glog.Flush()
	err := os.MkdirAll(*resultsDir, os.ModePerm)
	if err != nil {
		glog.Error(err)
	}
	parsedCheckerConfig := &pb.CheckerConfiguration{}
	err = protojson.Unmarshal([]byte(*checkerConfig), parsedCheckerConfig)
	if err != nil {
		glog.Fatal("parsing checker config: ", err)
	}
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(*compileCommandsPath)
	if err != nil {
		glog.Fatalf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)
	buildActions, err := csa.ParseCompileCommands(
		*compileCommandsPath,
		ignoreDirPatterns,
		parsedCheckerConfig,
		true,
		/*onlyCppcheck=*/ false,
		/*isDev*/ true,
	)
	if err != nil {
		glog.Fatalf("csa.ParseCompileCommands: %v", err)
	}
	if len(*buildActions) == 0 {
		glog.Infof("Nothing to check, exit.")
		os.Exit(0)
	}

	numWorkers := int32(runtime.NumCPU())
	cpumem.Init(int(numWorkers), 0)

	parsedCheckRules := make([]checkrule.CheckRule, 0)
	for _, ruleName := range strings.Split(*checkRules, ",") {
		checkRule, err := checkrule.MakeCheckRule(ruleName, *jsonOptions)
		if err != nil {
			glog.Errorf("invalid json options: %v", *jsonOptions)
		}
		parsedCheckRules = append(parsedCheckRules, *checkRule)
	}

	err = analyzerinterface.CleanResultDir(*resultsDir)
	if err != nil {
		glog.Errorf("failed to clean log and result dir: %v", err)
	}

	dumpErrors, err := analyzer.PreAnalyze(
		*compileCommandsPath,
		buildActions,
		parsedCheckRules,
		*resultsDir,
		parsedCheckerConfig,
		*checkProgress,
		*enableCodeChecker,
		numWorkers,
		/*ignoreDirPatterns=*/ []string{},
		/*lang=*/ "zh",
		/*onlyCppcheck=*/ false,
	)
	if err != nil {
		glog.Error(fmt.Errorf("analyzer.PreAnalyze: %v", err))
	}
	allResults := analyzer.Analyze(
		parsedCheckRules,
		*resultsDir,
		*compileCommandsPath,
		buildActions,
		ignoreDirPatterns,
		parsedCheckerConfig,
		dumpErrors,
		*checkProgress,
		numWorkers,
		false,
		90,
		30,
		"zh",
		filteredCompileCommandsFolder,
		/*onlyCppcheck=*/ false,
	)
	err = checker_integration.DeleteRepeatedResults(allResults)
	if err != nil {
		glog.Fatal(err)
	}
	out, err := proto.Marshal(allResults)
	if err != nil {
		glog.Fatal(err)
	}
	err = os.WriteFile(filepath.Join(*resultsDir, "results"), out, 0644)
	if err != nil {
		glog.Fatal(err)
	}
	err = utils.CleanCache(*resultsDir, []string{"results"})
	if err != nil {
		glog.Fatal(err)
	}
}
