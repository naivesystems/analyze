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

package clangsema

import (
	"errors"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

type CheckFunction func(diagnostics []Diagnostic) *pb.ResultsList

type Rule struct {
	Options string
	Check   CheckFunction
}

func GetClangSemaDiagnostics(
	compileCommandsPath,
	options,
	resultsDir string,
	buildActions *[]csa.BuildAction,
	csaSystemLibOptions,
	clangBin,
	gccPredefinedMacros string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) ([]Diagnostic, error) {
	taskName := filepath.Base(resultsDir)
	diagnostics := []Diagnostic{}
	systemLibOptions := csaSystemLibOptions // TODO: remove this config path hack
	reportDir := filepath.Join(resultsDir, "clangsema-out")
	err := os.MkdirAll(reportDir, os.ModePerm)
	if err != nil {
		return nil, err
	}
	reportPath := filepath.Join(reportDir, "outxml")
	for _, action := range *buildActions {
		cmd_arr := []string{"-fsyntax-only"}
		if action.Target != "" {
			cmd_arr = append(cmd_arr, "--target="+action.Target)
		}
		cmd_arr = append(cmd_arr, strings.Fields(systemLibOptions)...)
		cmd_arr = append(cmd_arr, strings.Fields(options)...)
		cmd_arr = append(cmd_arr, csa.ParsePreprocessorSymbols(action, true, false)...)
		cmd_arr = append(cmd_arr, action.Command.File)
		cmd_arr = append(cmd_arr, csa.ParseGccPredefinedMacros(gccPredefinedMacros, false)...)
		cmd := exec.Command(clangBin, cmd_arr...)
		cmd.Env = append(cmd.Env, "CC_LOG_DIAGNOSTICS=1", "CC_LOG_DIAGNOSTICS_FILE="+reportPath)
		cmd.Dir = action.Command.Directory
		glog.Info("executing: ", cmd.String())
		out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Warningf("in %s, executing: %s, reported:\n%s", filepath.Base(resultsDir), cmd.String(), string(out))
			if exitErr, ok := err.(*exec.ExitError); ok && exitErr.ProcessState.ExitCode() == 1 {
				// ignore exit code 1, because some "error" level diagnostics can only be generated with it
				glog.Warningf("clang reported compilation errors and exited code 1")
			} else {
				return nil, err
			}
		}
		diagnosticFile, err := os.Open(reportPath)
		if err != nil {
			return nil, err
		}
		diagnosticContent, err := io.ReadAll(diagnosticFile)
		if err != nil {
			return nil, err
		}
		err = diagnosticFile.Close()
		if err != nil {
			return nil, err
		}
		err = os.Remove(reportPath)
		if err != nil {
			return nil, err
		}
		if len(diagnosticContent) == 0 {
			continue
		}
		diagnostic, err := Parse(diagnosticContent)
		if err != nil {
			return nil, err
		}
		diagnostics = append(diagnostics, diagnostic...)
	}
	return diagnostics, err
}

func CheckRule2_2(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.WarningOption == "unused-value" {
			result := &pb.Result{}
			result.Path = diagnostic.Filename
			result.LineNumber = int32(diagnostic.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_2_2_EXTERNAL
			result.ErrorMessage = "[C2006][misra-c2012-2.2]: violation of rule_2_2\n" + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func CheckRule13_2(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.WarningOption == "unsequenced" {
			result := &pb.Result{}
			result.Path = diagnostic.Filename
			result.LineNumber = int32(diagnostic.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_13_2_EXTERNAL
			result.ErrorMessage = "[C1605][misra-c2012-13.2]: violation of rule_13_2\n" + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func CheckRule14_3(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.WarningOption == "tautological-unsigned-zero-compare" ||
			diagnostic.WarningOption == "tautological-constant-out-of-range-compare" ||
			diagnostic.WarningOption == "tautological-type-limit-compare" ||
			diagnostic.WarningOption == "tautological-overlap-compare" ||
			diagnostic.WarningOption == "tautological-constant-compare" {
			result := &pb.Result{}
			result.Path = diagnostic.Filename
			result.LineNumber = int32(diagnostic.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_14_3_EXTERNAL
			result.ErrorMessage = "[C1702][misra-c2012-14.3]: violation of misra-c2012-14.3\n" + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func CheckRule13_4(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.WarningOption == "parentheses" &&
			diagnostic.Message == "using the result of an assignment as a condition without parentheses" {
			result := &pb.Result{}
			result.Path = diagnostic.Filename
			result.LineNumber = int32(diagnostic.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_13_4_EXTERNAL
			result.ErrorMessage = "[C1603][misra-c2012-13.4]: violation of rule_13_4\n" + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func CheckRule17_3(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.WarningOption == "implicit-function-declaration" {
			result := &pb.Result{}
			result.Path = diagnostic.Filename
			result.LineNumber = int32(diagnostic.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_17_3
			result.ErrorMessage = "[C1506][misra-c2012-17.3]: violation of rule_17_3\n" + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func CheckRule17_4(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.WarningOption == "return-type" {
			result := &pb.Result{}
			result.Path = diagnostic.Filename
			result.LineNumber = int32(diagnostic.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_17_4
			result.ErrorMessage = "[C1505][misra-c2012-17.4]: violation of rule_17_4\n" + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func GetRuleMap() map[string]Rule {
	ruleMap := map[string]Rule{
		"misra_c_2012/rule_2_2": {
			Options: "-Wunused-value",
			Check:   CheckRule2_2,
		},
		"misra_c_2012/rule_13_2": {
			Options: "-Wunsequenced -Wsequence-point",
			Check:   CheckRule13_2,
		},
		"misra_c_2012/rule_13_4": {
			Options: "-Wparentheses",
			Check:   CheckRule13_4,
		},
		"misra_c_2012/rule_14_3": {
			Options: "-Wtautological-constant-out-of-range-compare -Wtautological-unsigned-zero-compare -Wtautological-type-limit-compare -Wtautological-overlap-compare",
			Check:   CheckRule14_3,
		},
		"misra_c_2012/rule_17_3": {
			Options: "-Wimplicit-function-declaration",
			Check:   CheckRule17_3,
		},
		"misra_c_2012/rule_17_4": {
			Options: "-Wreturn-type",
			Check:   CheckRule17_4,
		},
	}
	return ruleMap
}

func Exec(
	compileCommandsPath,
	checkRule,
	resultsDir string,
	buildActions *[]csa.BuildAction,
	csaSystemLibOptions,
	clangBin,
	gccPredefinedMacros string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) (*pb.ResultsList, error) {
	ruleMap := GetRuleMap()
	rule, exist := ruleMap[checkRule]
	if !exist {
		return nil, errors.New("unrecognized rule name " + checkRule)
	}
	reportJson, err := GetClangSemaDiagnostics(
		compileCommandsPath,
		rule.Options,
		resultsDir,
		buildActions,
		csaSystemLibOptions,
		clangBin,
		gccPredefinedMacros,
		limitMemory,
		timeoutNormal,
		timeoutOom)
	if err != nil {
		return nil, err
	}
	resultsList := rule.Check(reportJson)
	return resultsList, err
}
