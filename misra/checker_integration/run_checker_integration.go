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

package checker_integration

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/clangsema"
	"naive.systems/analyzer/misra/checker_integration/clangtidy"
	"naive.systems/analyzer/misra/checker_integration/cppcheck"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/checker_integration/gcc_diagnostics"
	"naive.systems/analyzer/misra/checker_integration/infer"
	"naive.systems/analyzer/misra/checker_integration/misra"
)

type Checker int

const (
	Infer Checker = iota
	CSA
	Cppcheck_Binary
	Cppcheck_STU
	Cppcheck_CTU
	Clangtidy
	Misra
	Clangsema
	GCC
	Libtooling_STU
	Libtooling_CTU
	Mixture
	EmptyChecker // A placeholder for unimplemented checker.
)

func (e Checker) String() string {
	switch e {
	case Infer:
		return "Infer"
	case CSA:
		return "CSA"
	case Cppcheck_STU:
		return "Cppcheck_STU"
	case Cppcheck_CTU:
		return "Cppcheck_CTU"
	case Clangtidy:
		return "Clangtidy"
	case Misra:
		return "Misra"
	case Clangsema:
		return "Clangsema"
	case GCC:
		return "GCC"
	case Libtooling_STU:
		return "Libtooling_STU"
	case Libtooling_CTU:
		return "Libtooling_CTU"
	case Mixture:
		return "Mixture"
	case EmptyChecker:
		return "EmptyChecker"
	default:
		return fmt.Sprintf("%d", e)
	}
}

var RuleCheckerMap = map[string][]Checker{
	// dir 1
	"misra_c_2012/dir_1_1": {},
	// dir 2
	"misra_c_2012/dir_2_1": {},
	// dir 3
	"misra_c_2012/dir_3_1": {},
	// dir 4
	"misra_c_2012/dir_4_1":  {},
	"misra_c_2012/dir_4_2":  {},
	"misra_c_2012/dir_4_3":  {Libtooling_CTU},
	"misra_c_2012/dir_4_4":  {},
	"misra_c_2012/dir_4_5":  {},
	"misra_c_2012/dir_4_6":  {},
	"misra_c_2012/dir_4_7":  {Libtooling_CTU},
	"misra_c_2012/dir_4_8":  {},
	"misra_c_2012/dir_4_9":  {},
	"misra_c_2012/dir_4_10": {Libtooling_CTU},
	"misra_c_2012/dir_4_11": {CSA, Libtooling_CTU},
	"misra_c_2012/dir_4_12": {Libtooling_CTU},
	"misra_c_2012/dir_4_13": {},
	"misra_c_2012/dir_4_14": {CSA},
	// rule 1
	"misra_c_2012/rule_1_1": {EmptyChecker},
	"misra_c_2012/rule_1_2": {EmptyChecker},
	"misra_c_2012/rule_1_3": {CSA},
	"misra_c_2012/rule_1_4": {Cppcheck_STU},
	// rule 2
	"misra_c_2012/rule_2_1": {Infer, CSA},
	"misra_c_2012/rule_2_2": {Infer, Libtooling_CTU, Clangsema},
	"misra_c_2012/rule_2_3": {Libtooling_CTU},
	"misra_c_2012/rule_2_4": {Cppcheck_CTU},
	"misra_c_2012/rule_2_5": {Cppcheck_CTU},
	"misra_c_2012/rule_2_6": {Cppcheck_STU},
	"misra_c_2012/rule_2_7": {Cppcheck_STU},
	// rule 3
	"misra_c_2012/rule_3_1": {Cppcheck_STU},
	"misra_c_2012/rule_3_2": {Cppcheck_STU},
	// rule 4
	"misra_c_2012/rule_4_1": {Cppcheck_STU},
	"misra_c_2012/rule_4_2": {Misra},
	// rule 5
	"misra_c_2012/rule_5_1": {Libtooling_CTU},
	"misra_c_2012/rule_5_2": {Cppcheck_STU},
	"misra_c_2012/rule_5_3": {Cppcheck_STU},
	"misra_c_2012/rule_5_4": {Cppcheck_STU},
	"misra_c_2012/rule_5_5": {Cppcheck_STU},
	"misra_c_2012/rule_5_6": {Libtooling_CTU},
	"misra_c_2012/rule_5_7": {Libtooling_CTU},
	"misra_c_2012/rule_5_8": {Libtooling_CTU},
	"misra_c_2012/rule_5_9": {Libtooling_CTU},
	// rule 6
	"misra_c_2012/rule_6_1": {Cppcheck_STU},
	"misra_c_2012/rule_6_2": {Cppcheck_STU},
	// rule 7
	"misra_c_2012/rule_7_1": {Cppcheck_STU},
	"misra_c_2012/rule_7_2": {Cppcheck_STU},
	"misra_c_2012/rule_7_3": {Cppcheck_STU},
	"misra_c_2012/rule_7_4": {Libtooling_STU, Misra},
	// rule 8
	"misra_c_2012/rule_8_1":  {GCC, Misra},
	"misra_c_2012/rule_8_2":  {Libtooling_STU},
	"misra_c_2012/rule_8_3":  {Libtooling_CTU},
	"misra_c_2012/rule_8_4":  {Libtooling_STU},
	"misra_c_2012/rule_8_5":  {Libtooling_CTU},
	"misra_c_2012/rule_8_6":  {Libtooling_CTU},
	"misra_c_2012/rule_8_7":  {Libtooling_CTU},
	"misra_c_2012/rule_8_8":  {Cppcheck_STU},
	"misra_c_2012/rule_8_9":  {Cppcheck_CTU},
	"misra_c_2012/rule_8_10": {Cppcheck_STU},
	"misra_c_2012/rule_8_11": {Cppcheck_STU},
	"misra_c_2012/rule_8_12": {Cppcheck_STU},
	"misra_c_2012/rule_8_13": {},
	"misra_c_2012/rule_8_14": {Cppcheck_STU},
	// rule 9
	"misra_c_2012/rule_9_1": {CSA},
	"misra_c_2012/rule_9_2": {Cppcheck_STU},
	"misra_c_2012/rule_9_3": {Cppcheck_STU},
	"misra_c_2012/rule_9_4": {Cppcheck_STU},
	"misra_c_2012/rule_9_5": {Cppcheck_STU},
	// rule 10
	"misra_c_2012/rule_10_1": {Cppcheck_STU},
	"misra_c_2012/rule_10_2": {Cppcheck_STU},
	"misra_c_2012/rule_10_3": {Cppcheck_STU},
	"misra_c_2012/rule_10_4": {Cppcheck_STU},
	"misra_c_2012/rule_10_5": {Cppcheck_STU},
	"misra_c_2012/rule_10_6": {Cppcheck_STU},
	"misra_c_2012/rule_10_7": {Cppcheck_STU},
	"misra_c_2012/rule_10_8": {Cppcheck_STU},
	// rule 11
	"misra_c_2012/rule_11_1": {Misra, Libtooling_STU},
	"misra_c_2012/rule_11_2": {Libtooling_STU},
	"misra_c_2012/rule_11_3": {Libtooling_STU},
	"misra_c_2012/rule_11_4": {Libtooling_STU},
	"misra_c_2012/rule_11_5": {Libtooling_STU, CSA},
	"misra_c_2012/rule_11_6": {Misra, Libtooling_STU},
	"misra_c_2012/rule_11_7": {Misra, Libtooling_STU},
	"misra_c_2012/rule_11_8": {Libtooling_STU},
	"misra_c_2012/rule_11_9": {Cppcheck_STU},
	// rule 12
	"misra_c_2012/rule_12_1": {Cppcheck_STU},
	"misra_c_2012/rule_12_2": {CSA},
	"misra_c_2012/rule_12_3": {Libtooling_STU},
	"misra_c_2012/rule_12_4": {Cppcheck_STU},
	"misra_c_2012/rule_12_5": {Cppcheck_STU},
	// rule 13
	"misra_c_2012/rule_13_1": {Libtooling_CTU},
	"misra_c_2012/rule_13_2": {CSA, Clangsema, Libtooling_CTU},
	"misra_c_2012/rule_13_3": {Libtooling_STU},
	"misra_c_2012/rule_13_4": {Libtooling_STU, Clangsema},
	"misra_c_2012/rule_13_5": {Misra, Libtooling_CTU},
	"misra_c_2012/rule_13_6": {Cppcheck_STU},
	// rule 14
	"misra_c_2012/rule_14_1": {Libtooling_CTU},
	"misra_c_2012/rule_14_2": {Libtooling_CTU},
	"misra_c_2012/rule_14_3": {Mixture}, // use Libtooling_CTU, Infer and Clangsema
	"misra_c_2012/rule_14_4": {Cppcheck_STU},
	// rule 15
	"misra_c_2012/rule_15_1": {Cppcheck_STU},
	"misra_c_2012/rule_15_2": {Cppcheck_STU},
	"misra_c_2012/rule_15_3": {Cppcheck_STU},
	"misra_c_2012/rule_15_4": {Cppcheck_STU},
	"misra_c_2012/rule_15_5": {Cppcheck_STU},
	"misra_c_2012/rule_15_6": {Cppcheck_STU},
	"misra_c_2012/rule_15_7": {Cppcheck_STU, Libtooling_STU},
	// rule 16
	"misra_c_2012/rule_16_1": {Cppcheck_STU},
	"misra_c_2012/rule_16_2": {Cppcheck_STU},
	"misra_c_2012/rule_16_3": {Cppcheck_STU},
	"misra_c_2012/rule_16_4": {Cppcheck_STU},
	"misra_c_2012/rule_16_5": {Cppcheck_STU},
	"misra_c_2012/rule_16_6": {Cppcheck_STU},
	"misra_c_2012/rule_16_7": {Cppcheck_STU},
	// rule 17
	"misra_c_2012/rule_17_1": {Cppcheck_STU},
	"misra_c_2012/rule_17_2": {Misra, Clangtidy},
	"misra_c_2012/rule_17_3": {Clangsema},
	"misra_c_2012/rule_17_4": {Clangsema},
	"misra_c_2012/rule_17_5": {GCC, Libtooling_CTU},
	"misra_c_2012/rule_17_6": {Cppcheck_STU},
	"misra_c_2012/rule_17_7": {Cppcheck_STU},
	"misra_c_2012/rule_17_8": {Cppcheck_CTU},
	// rule 18
	"misra_c_2012/rule_18_1": {CSA},
	"misra_c_2012/rule_18_2": {CSA},
	"misra_c_2012/rule_18_3": {CSA},
	"misra_c_2012/rule_18_4": {Cppcheck_STU},
	"misra_c_2012/rule_18_5": {Cppcheck_STU, Libtooling_STU},
	"misra_c_2012/rule_18_6": {CSA},
	"misra_c_2012/rule_18_7": {Cppcheck_STU},
	"misra_c_2012/rule_18_8": {Cppcheck_STU},
	// rule 19
	"misra_c_2012/rule_19_1": {CSA, Libtooling_STU},
	"misra_c_2012/rule_19_2": {Cppcheck_STU},
	// rule 20
	"misra_c_2012/rule_20_1":  {Cppcheck_STU},
	"misra_c_2012/rule_20_2":  {Cppcheck_STU},
	"misra_c_2012/rule_20_3":  {Cppcheck_STU},
	"misra_c_2012/rule_20_4":  {Cppcheck_STU},
	"misra_c_2012/rule_20_5":  {Cppcheck_STU},
	"misra_c_2012/rule_20_6":  {Cppcheck_STU},
	"misra_c_2012/rule_20_7":  {Cppcheck_STU},
	"misra_c_2012/rule_20_8":  {Cppcheck_STU},
	"misra_c_2012/rule_20_9":  {Cppcheck_STU},
	"misra_c_2012/rule_20_10": {Cppcheck_STU},
	"misra_c_2012/rule_20_11": {Cppcheck_STU},
	"misra_c_2012/rule_20_12": {Cppcheck_STU},
	"misra_c_2012/rule_20_13": {Cppcheck_STU},
	"misra_c_2012/rule_20_14": {Cppcheck_STU},
	// rule 21
	"misra_c_2012/rule_21_1":  {Cppcheck_STU},
	"misra_c_2012/rule_21_2":  {Cppcheck_STU},
	"misra_c_2012/rule_21_3":  {Cppcheck_STU},
	"misra_c_2012/rule_21_4":  {Cppcheck_STU},
	"misra_c_2012/rule_21_5":  {Cppcheck_STU},
	"misra_c_2012/rule_21_6":  {Cppcheck_STU},
	"misra_c_2012/rule_21_7":  {Cppcheck_STU},
	"misra_c_2012/rule_21_8":  {Cppcheck_STU},
	"misra_c_2012/rule_21_9":  {Cppcheck_STU},
	"misra_c_2012/rule_21_10": {Cppcheck_STU},
	"misra_c_2012/rule_21_11": {Cppcheck_STU},
	"misra_c_2012/rule_21_12": {Cppcheck_STU},
	"misra_c_2012/rule_21_13": {CSA},
	"misra_c_2012/rule_21_14": {CSA},
	"misra_c_2012/rule_21_15": {Cppcheck_STU},
	"misra_c_2012/rule_21_16": {Cppcheck_STU},
	"misra_c_2012/rule_21_17": {CSA},
	"misra_c_2012/rule_21_18": {Infer, CSA, Libtooling_STU},
	"misra_c_2012/rule_21_19": {CSA, Libtooling_CTU},
	"misra_c_2012/rule_21_20": {CSA},
	"misra_c_2012/rule_21_21": {Cppcheck_STU},
	// rule 22
	"misra_c_2012/rule_22_1":  {Infer, CSA},
	"misra_c_2012/rule_22_2":  {CSA},
	"misra_c_2012/rule_22_3":  {CSA},
	"misra_c_2012/rule_22_4":  {CSA},
	"misra_c_2012/rule_22_5":  {CSA},
	"misra_c_2012/rule_22_6":  {CSA},
	"misra_c_2012/rule_22_7":  {CSA},
	"misra_c_2012/rule_22_8":  {CSA},
	"misra_c_2012/rule_22_9":  {CSA},
	"misra_c_2012/rule_22_10": {CSA},
	"naivesystems/C7966":      {Libtooling_STU, Cppcheck_STU},
}

func ExecCSA(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir string, buildActions *[]csa.BuildAction, config *pb.CheckerConfiguration, allResults *pb.ResultsList, limitMemory bool, timeoutNormal, timeoutOom int) error {
	resultsList, err := csa.Exec(checkRule, compileCommandsPath, resultsDir, buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecInfer(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir string, config *pb.CheckerConfiguration, allResults *pb.ResultsList, limitMemory bool, timeoutNormal, timeoutOom int) error {
	taskName := filepath.Base(resultsDir)
	_, err := os.Stat(compileCommandsPath)
	if err != nil {
		if os.IsNotExist(err) {
			glog.Infof("in %s: %s does not exist", taskName, compileCommandsPath)
		}
		return err
	}
	resultsList, err := infer.Exec(checkRule, compileCommandsPath, resultsDir, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecCppcheckSTU(
	checkRule checkrule.CheckRule,
	compileCommandsPath,
	resultsDir string,
	buildActions *[]csa.BuildAction,
	config *pb.CheckerConfiguration,
	allResults *pb.ResultsList,
	dumpErrors map[string]string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) error {
	resultsList, err := cppcheck.ExecSTU(
		checkRule.Name,
		checkRule.JSONOptions,
		compileCommandsPath,
		resultsDir,
		filepath.Dir(resultsDir),
		buildActions,
		config.CppcheckBin,
		config.PythonBin,
		dumpErrors,
		limitMemory,
		timeoutNormal,
		timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecCppcheckCTU(
	checkRule checkrule.CheckRule,
	compileCommandsPath,
	resultsDir string,
	buildActions *[]csa.BuildAction,
	config *pb.CheckerConfiguration,
	allResults *pb.ResultsList,
	dumpErrors map[string]string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) error {
	resultsList, err := cppcheck.ExecCTU(
		checkRule.Name,
		checkRule.JSONOptions,
		compileCommandsPath,
		resultsDir,
		filepath.Dir(resultsDir),
		buildActions,
		config.CppcheckBin,
		config.PythonBin,
		dumpErrors,
		limitMemory,
		timeoutNormal,
		timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecClangtidy(
	compileCommandsPath,
	checkRule,
	resultsDir string,
	buildActions *[]csa.BuildAction,
	csaSystemLibOptions,
	clangtidyBin string,
	allResults *pb.ResultsList,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) error {
	resultsList, err := clangtidy.Exec(
		compileCommandsPath,
		buildActions,
		checkRule,
		resultsDir,
		csaSystemLibOptions,
		clangtidyBin,
		limitMemory,
		timeoutNormal,
		timeoutOom,
	)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecMisra(compileCommandsPath string, checkRule string, resultsDir string, buildActions *[]csa.BuildAction, config *pb.CheckerConfiguration, allResults *pb.ResultsList, limitMemory bool, timeoutNormal, timeoutOom int) error {
	resultsList, err := misra.Exec("/src", compileCommandsPath, buildActions, checkRule, resultsDir, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecGCC(compileCommandsPath, checkRule, resultsDir, csaSystemLibOptions string, buildActions *[]csa.BuildAction, allResults *pb.ResultsList, limitMemory bool, timeoutNormal, timeoutOom int) error {
	resultsList, err := gcc_diagnostics.Exec(compileCommandsPath, checkRule, resultsDir, csaSystemLibOptions, buildActions, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

func ExecClangsema(
	compileCommandsPath,
	checkRule,
	resultsDir string,
	buildActions *[]csa.BuildAction,
	config *pb.CheckerConfiguration,
	allResults *pb.ResultsList,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) error {
	resultsList, err := clangsema.Exec(
		compileCommandsPath,
		checkRule,
		resultsDir,
		buildActions,
		config.GetCsaSystemLibOptions(),
		config.GetClangBin(),
		config.GetGccPredefinedMacros(),
		limitMemory,
		timeoutNormal,
		timeoutOom)
	if err != nil {
		return err
	}
	allResults.Results = append(allResults.Results, resultsList.Results...)
	return nil
}

// Delete repeated errors in results and return a cutted results
func DeleteRepeatedResults(allResults *pb.ResultsList) error {
	errHashMap := make(map[string]*pb.Result)
	rtnResults := make([]*pb.Result, 0)
	for _, currentResult := range allResults.Results {
		if !CheckAndCreateErrHashMap(&errHashMap, currentResult) {
			rtnResults = append(rtnResults, currentResult)
		}
	}
	allResults.Results = rtnResults
	return nil
}

// TODO: deprecated this, filter in build actions
func DeleteCCResults(allResults *pb.ResultsList) {
	rtnResults := make([]*pb.Result, 0)
	for _, currentResult := range allResults.Results {
		if strings.HasSuffix(currentResult.Path, ".cc") ||
			strings.HasSuffix(currentResult.Path, ".cpp") {
			continue
		}
		rtnResults = append(rtnResults, currentResult)
	}
	allResults.Results = rtnResults
}

// TODO: deprecated this, filter in build actions
func DeleteCResults(allResults *pb.ResultsList) {
	rtnResults := make([]*pb.Result, 0)
	for _, currentResult := range allResults.Results {
		if strings.HasSuffix(currentResult.Path, ".c") {
			continue
		}
		rtnResults = append(rtnResults, currentResult)
	}
	allResults.Results = rtnResults
}

// Check if an error message is in the Hash map, if not add it
func CheckAndCreateErrHashMap(errHashMap *map[string]*pb.Result, currentResult *pb.Result) bool {
	currentStr := currentResult.Path + strconv.Itoa(int(currentResult.LineNumber)) + currentResult.ErrorMessage
	if _, ok := (*errHashMap)[currentStr]; !ok {
		(*errHashMap)[currentStr] = currentResult
		return false
	}
	return true
}
