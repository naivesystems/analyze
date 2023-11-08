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
	"errors"
	"path/filepath"
	"strconv"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

// Delete False positive errors reported by false positive checkers
// Make sure that the results are belong to the same rule
func DeleteFalsePositiveResults(allResults *pb.ResultsList) error {
	FalsePositiveErrHashMap := make(map[string]*pb.Result)
	rtnResults := make([]*pb.Result, 0)
	// add false positive cases found by the false positive checker to FalsePositiveErrHashMap
	// false positive field for false positive cases is true
	for _, currentResult := range allResults.Results {
		if currentResult.FalsePositive {
			absPath, err := filepath.Abs(currentResult.Path)
			if err != nil {
				return err
			}
			currentStr := absPath + strconv.Itoa(int(currentResult.LineNumber))
			// add the prefix str containing Path and LineNumber into FalsePositiveErrHashMap
			FalsePositiveErrHashMap[currentStr] = currentResult
		}
	}
	for _, currentResult := range allResults.Results {
		if !matchFalsePositiveResult(&FalsePositiveErrHashMap, currentResult) {
			rtnResults = append(rtnResults, currentResult)
		}
	}
	allResults.Results = rtnResults
	return nil
}

// Check if currentResult matches any false positive result with the same path and line
func matchFalsePositiveResult(errHashMap *map[string]*pb.Result, currentResult *pb.Result) bool {
	absPath, _ := filepath.Abs(currentResult.Path)
	currentStr := absPath + strconv.Itoa(int(currentResult.LineNumber))
	if _, ok := (*errHashMap)[currentStr]; ok {
		return true
	}
	return false
}

type ruleChecker struct {
	Check func(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir, logDir, misraCheckerPath string,
		buildActions *[]csa.BuildAction, sourceFiles []string, checkerConfig *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, []error)
}

func check14_3(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir, logDir, misraCheckerPath string,
	buildActions *[]csa.BuildAction, sourceFiles []string, checkerConfig *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, []error) {
	errs := []error{}
	resultsList := pb.ResultsList{}
	resultsPath := filepath.Join(resultsDir, "results")
	err := RunCmdWithRuleName(compileCommandsPath, checkRule.Name, resultsPath, resultsDir, logDir, checkerConfig.GetMisraCheckerPath(), checkRule.JSONOptions, buildActions, sourceFiles, checkerConfig, &resultsList, checker_integration.Libtooling_CTU, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		errs = append(errs, err)
	}
	err = checker_integration.ExecInfer(checkRule, compileCommandsPath, resultsDir, checkerConfig, &resultsList, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		errs = append(errs, err)
	}
	err = checker_integration.ExecClangsema(compileCommandsPath, checkRule.Name, resultsDir, buildActions, checkerConfig, &resultsList, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		errs = append(errs, err)
	}
	err = DeleteFalsePositiveResults(&resultsList)
	if err != nil {
		errs = append(errs, err)
	}
	if len(errs) == 0 {
		errs = nil
	}
	return &resultsList, errs
}

var ruleMap = map[string]ruleChecker{
	"misra_c_2012/rule_14_3": {
		Check: check14_3,
	},
}

func Exec(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir, logDir, misraCheckerPath string,
	buildActions *[]csa.BuildAction, sourceFiles []string, checkerConfig *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, []error) {
	ruleChecker, exist := ruleMap[checkRule.Name]
	if !exist {
		return nil, []error{errors.New("unrecognized rule name " + checkRule.Name)}
	}
	resultsList, errs := ruleChecker.Check(checkRule, compileCommandsPath, resultsDir, logDir, misraCheckerPath,
		buildActions, sourceFiles, checkerConfig, limitMemory, timeoutNormal, timeoutOom)
	return resultsList, errs
}
