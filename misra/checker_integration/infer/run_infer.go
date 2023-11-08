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

package infer

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
)

type CheckFunction func(reportJson []InferReport) *pb.ResultsList

type Rule struct {
	Plugin string
	Check  CheckFunction
}

func GetInferReport(srcDir, compileCommandsPath, options, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) ([]InferReport, error) {
	taskName := filepath.Base(resultsDir)
	reportDir := filepath.Join(resultsDir, "infer-out")
	err := os.MkdirAll(reportDir, os.ModePerm)
	if err != nil {
		return nil, err
	}
	if err != nil {
		return nil, err
	}
	cmd_arr := []string{"--no-progress-bar", "--pmd-xml", "--scheduler=restart", "--results-dir", reportDir,
		"--compilation-database", compileCommandsPath, "--report-force-relative-path", "--no-dbwriter"}
	cmd_arr = append(cmd_arr, strings.Fields(config.InferExtraOptions)...)
	cmd_arr = append(cmd_arr, fmt.Sprintf("--jobs=%d", config.InferJobs))
	options_summary := append(cmd_arr, strings.Fields(options)...)
	cmd := exec.Command(config.InferBin, options_summary...)
	glog.Infof("in %s, executing: %s", taskName, cmd.String())
	cmd.Dir = srcDir
	out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		outStr := string(out)
		if strings.Contains(outStr, "Internal Error: Non-assignable LHS expression") {
			glog.Warningf("in %s, executing: %s, a known error is detected:\n%s", taskName, cmd.String(), outStr)
			return []InferReport{}, nil
		}
		glog.Errorf("in %s, executing: %s, reported:\n%s", taskName, cmd.String(), outStr)
		if strings.Contains(outStr, "sigkill") {
			err = fmt.Errorf("signal: killed")
		}
		return nil, err
	}
	reportFile, err := os.Open(reportDir + "/report.json")
	if err != nil {
		return nil, err
	}
	defer reportFile.Close()
	reportContent, err := io.ReadAll(reportFile)
	if err != nil {
		return nil, err
	}
	var reportJsons []InferReport
	err = json.Unmarshal(reportContent, &reportJsons)
	if err != nil {
		return nil, err
	}
	for i := 0; i < len(reportJsons); i++ {
		// change to absolute path
		path, err := basic.ConvertRelativePathToAbsolute(srcDir, reportJsons[i].File)
		if err != nil {
			glog.Error(err)
			continue
		}
		reportJsons[i].File = path
	}
	return reportJsons, err
}

func CheckRule2_1(reportJson []InferReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	reg := regexp.MustCompile(`Boolean condition`)
	for _, bugMap := range reportJson {
		if bugMap.Bug_type == "UNREACHABLE_CODE" {
			match := reg.MatchString(bugMap.Qualifier)
			if match {
				result := &pb.Result{}
				result.Path = bugMap.File
				result.LineNumber = bugMap.Line
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_2_1
				result.ErrorMessage = "[C2007][misra-c2012-2.1]: violation of misra-c2012-2.1\n" + bugMap.Bug_type + "\n" + bugMap.Qualifier
				result.ExternalMessage = bugMap.Qualifier
				resultsList.Results = append(resultsList.Results, result)
				glog.Info(result.ErrorMessage)
			}
		}
	}
	return resultsList
}

func CheckRule14_3(reportJson []InferReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	reg := regexp.MustCompile(`Boolean condition`)
	for _, bugMap := range reportJson {
		if bugMap.Bug_type == "CONDITION_ALWAYS_FALSE" || bugMap.Bug_type == "CONDITION_ALWAYS_TRUE" {
			match := reg.MatchString(bugMap.Qualifier)
			if match {
				result := &pb.Result{}
				result.Path = bugMap.File
				result.LineNumber = bugMap.Line
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_14_3_EXTERNAL
				result.ErrorMessage = "[C1702][misra-c2012-14.3]: violation of misra-c2012-14.3\n" + bugMap.Bug_type + "\n" + bugMap.Qualifier
				result.ExternalMessage = bugMap.Qualifier
				resultsList.Results = append(resultsList.Results, result)
				glog.Info(result.ErrorMessage)
			}
		}
	}
	return resultsList
}

func CheckRule2_2(reportJson []InferReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, bugMap := range reportJson {
		if bugMap.Bug_type == "DEAD_STORE" {
			result := &pb.Result{}
			result.Path = bugMap.File
			result.LineNumber = bugMap.Line
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_2_2_EXTERNAL
			result.ErrorMessage = "[C2006][misra-c2012-2.2]: violation of misra-c2012-2.2\n" + bugMap.Bug_type + "\n" + bugMap.Qualifier
			result.ExternalMessage = bugMap.Qualifier
			resultsList.Results = append(resultsList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultsList
}

func CheckRule21_18(reportJson []InferReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	reg := regexp.MustCompile(`memcmp()|memcpy()|memmove()|memset()|strncat()|strncmp()|strncpy()`)
	for _, bugMap := range reportJson {
		if bugMap.Bug_type == "PRECONDITION_NOT_MET" {
			match := reg.MatchString(bugMap.Qualifier)
			if match {
				result := &pb.Result{}
				result.Path = bugMap.File
				result.LineNumber = bugMap.Line
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_21_18_EXTERNAL
				result.ErrorMessage = "[C0403][misra-c2012-21.18]: violation of misra-c2012-21.18\n" + bugMap.Qualifier
				result.ExternalMessage = bugMap.Qualifier
				resultsList.Results = append(resultsList.Results, result)
				glog.Info(result.ErrorMessage)
			}
		}
	}
	return resultsList
}

func CheckRule22_1(reportJson []InferReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	reg := regexp.MustCompile(`malloc()|calloc()|realloc()|fopen()`)
	for _, bugMap := range reportJson {
		if bugMap.Bug_type == "RESOURCE_LEAK" || bugMap.Bug_type == "MEMORY_LEAK" {
			match := reg.MatchString(bugMap.Qualifier)
			if match {
				result := &pb.Result{}
				result.Path = bugMap.File
				result.LineNumber = bugMap.Line
				result.ErrorKind = pb.Result_MISRA_C_2012_RULE_22_1
				result.ErrorMessage = "[C0210][misra-c2012-22.1]: violation of misra-c2012-22.1\n" + bugMap.Qualifier
				result.ExternalMessage = bugMap.Qualifier
				resultsList.Results = append(resultsList.Results, result)
				glog.Info(result.ErrorMessage)
			}
		}
	}
	return resultsList
}

func GetRuleMap() map[string]Rule {
	ruleMap := map[string]Rule{
		"misra_c_2012/rule_2_1": {
			Plugin: "--bufferoverrun-only --debug-exceptions",
			Check:  CheckRule2_1,
		},
		"misra_c_2012/rule_2_2": {
			Plugin: "--liveness",
			Check:  CheckRule2_2,
		},
		"misra_c_2012/rule_14_3": {
			Plugin: "--bufferoverrun-only --debug-exceptions",
			Check:  CheckRule14_3,
		},
		"misra_c_2012/rule_21_18": {
			Plugin: "--bufferoverrun --debug-exceptions",
			Check:  CheckRule21_18,
		},
		"misra_c_2012/rule_22_1": {
			Plugin: "--pulse",
			Check:  CheckRule22_1,
		},
	}
	return ruleMap
}

// Deprecated, use cruleslib/runner/runner.go:RunInfer()
func Exec(checkRule checkrule.CheckRule, compileCommandsPath, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	ruleMap := GetRuleMap()
	rule, exist := ruleMap[checkRule.Name]
	if !exist {
		return nil, errors.New("unrecognized rule name " + checkRule.Name)
	}
	reportJson, err := GetInferReport("/src", compileCommandsPath, rule.Plugin, resultsDir, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	resultsList := rule.Check(reportJson)
	// Infer outputs results with path relative to project root, we convert it to abspath by adding prefix `/src`
	for idx := range resultsList.Results {
		path, err := basic.ConvertRelativePathToAbsolute("/src", resultsList.Results[idx].Path)
		if err != nil {
			glog.Errorf("run_infer.Exec: %v", err)
		}
		resultsList.Results[idx].Path = path
		for idxLoc := range resultsList.Results[idx].Locations {
			path, err := basic.ConvertRelativePathToAbsolute("/src", resultsList.Results[idx].Locations[idxLoc].Path)
			if err != nil {
				glog.Errorf("run_infer.Exec: %v", err)
			}
			resultsList.Results[idx].Locations[idxLoc].Path = path
		}
	}
	return resultsList, err
}
