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

package misra

import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

type RuleChecker struct {
	Check func(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error)
}

var ruleOptionMap = map[string][]string{
	"misra_c_2012/rule_7_4":  {"-Wincompatible-pointer-types"},
	"misra_c_2012/rule_8_1":  {"-Wimplicit-int"},
	"misra_c_2012/rule_11_1": {"-Wno-all"},
	"misra_c_2012/rule_11_6": {"-Wno-all"},
	"misra_c_2012/rule_11_7": {"-Wno-all"},
}

func Check4_2(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := &pb.ResultsList{}
	or := "|"
	trigraphs := []string{`??=`, `??(`, `??/`, `??)`, `??'`, `??<`, `??>`, `??!`, `??-`}
	regexStr := ""
	for _, trigraph := range trigraphs {
		regexStr = regexStr + regexp.QuoteMeta(trigraph) + or
	}
	regexStr = strings.TrimSuffix(regexStr, or)
	trigraphsRegex := regexp.MustCompile(regexStr)

	for _, action := range *buildActions {
		path := action.Command.File
		file, err := os.Open(path)
		if err != nil {
			glog.Errorf("failed to open source file: %s, error: %v", path, err)
			return nil, err
		}

		scanner := bufio.NewScanner(file)
		lineno := 0

		for scanner.Scan() {
			line := scanner.Text()
			lineno++

			matchings := trigraphsRegex.FindAllStringIndex(line, -1)
			for range matchings {
				resultsList.Results = append(resultsList.Results, &pb.Result{
					Path:         path,
					LineNumber:   int32(lineno),
					ErrorKind:    pb.Result_MISRA_C_2012_RULE_4_2,
					ErrorMessage: "[C2101][misra-c2012-4.2]: should not use trigraphs",
				})
			}
		}
		err = file.Close()
		if err != nil {
			return nil, err
		}
	}

	return resultsList, nil
}

func CheckMisraCpp2_5_1(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := &pb.ResultsList{}
	or := "|"
	digraphs := []string{`<%`, `%>`, `<:`, `:>`, `%:`, "%:%:"}
	regexStr := ""
	for _, digraph := range digraphs {
		regexStr = regexStr + regexp.QuoteMeta(digraph) + or
	}
	regexStr = strings.TrimSuffix(regexStr, or)
	digraphsRegex := regexp.MustCompile(regexStr)

	for _, action := range *buildActions {
		path := action.Command.File
		file, err := os.Open(path)
		if err != nil {
			glog.Errorf("failed to open source file: %s, error: %v", path, err)
			return nil, err
		}

		scanner := bufio.NewScanner(file)
		lineno := 0

		for scanner.Scan() {
			line := scanner.Text()
			lineno++

			matchings := digraphsRegex.FindAllStringIndex(line, -1)
			for range matchings {
				resultsList.Results = append(resultsList.Results, &pb.Result{
					Path:         path,
					LineNumber:   int32(lineno),
					ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_2_5_1,
					ErrorMessage: "[misra-cpp2008-2.5.1]: Digraphs should not be used",
				})
			}
		}
		err = file.Close()
		if err != nil {
			return nil, err
		}
	}

	return resultsList, nil
}

func Check7_4(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	clangWarnings, err := GetClangErrorsOrWarnings(compileCommandsPath, resultsDir, false, ruleOptionMap[checkRule], buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	for _, clangWarning := range clangWarnings {
		if strings.Contains(clangWarning.message, "incompatible pointer types initializing") &&
			!strings.Contains(clangWarning.message, "incompatible pointer types initializing 'const") {
			errMessage := "[C0901][misra-c2012-7.4]: " + clangWarning.message
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:            clangWarning.file,
				LineNumber:      int32(clangWarning.line),
				ErrorKind:       pb.Result_MISRA_C_2012_RULE_7_4_EXTERNAL,
				ErrorMessage:    errMessage,
				ExternalMessage: clangWarning.message,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func Check8_1(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	clangWarnings, err := GetClangErrorsOrWarnings(compileCommandsPath, resultsDir, false, ruleOptionMap[checkRule], buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	for _, clangWarning := range clangWarnings {
		if strings.Contains(clangWarning.message, "-Wimplicit-int") {
			errMessage := "[C0514][misra-c2012-8.1]: " + clangWarning.message
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:            clangWarning.file,
				LineNumber:      int32(clangWarning.line),
				ErrorKind:       pb.Result_MISRA_C_2012_RULE_8_1,
				ErrorMessage:    errMessage,
				ExternalMessage: clangWarning.message,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func Check11_1(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	clangErrors, err := GetClangErrorsOrWarnings(compileCommandsPath, resultsDir, true, ruleOptionMap[checkRule], buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	// for all impure functions, check whether it appears in && or || operator's right operand.
	for _, clangErr := range clangErrors {
		if strings.Contains(clangErr.message, "cannot be cast to a pointer type") {
			errMessage := "[C1409][misra-c2012-11.1]: " + clangErr.message
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:            clangErr.file,
				LineNumber:      int32(clangErr.line),
				ErrorKind:       pb.Result_MISRA_C_2012_RULE_11_1_EXTERNAL,
				ErrorMessage:    errMessage,
				ExternalMessage: clangErr.message,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func Check11_6(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	clangErrors, err := GetClangErrorsOrWarnings(compileCommandsPath, resultsDir, true, ruleOptionMap[checkRule], buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	// for all impure functions, check whether it appears in && or || operator's right operand.
	for _, clangErr := range clangErrors {
		if strings.Contains(clangErr.message, "cannot be cast to a pointer type") {
			errMessage := "[C1404][misra-c2012-11.6]: " + clangErr.message
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:            clangErr.file,
				LineNumber:      int32(clangErr.line),
				ErrorKind:       pb.Result_MISRA_C_2012_RULE_11_6_EXTERNAL,
				ErrorMessage:    errMessage,
				ExternalMessage: clangErr.message,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func Check11_7(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	clangErrors, err := GetClangErrorsOrWarnings(compileCommandsPath, resultsDir, true, ruleOptionMap[checkRule], buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	// for all impure functions, check whether it appears in && or || operator's right operand.
	for _, clangErr := range clangErrors {
		if strings.Contains(clangErr.message, "cannot be cast to a pointer type") {
			errMessage := "[C1403][misra-c2012-11.7]: " + clangErr.message
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:            clangErr.file,
				LineNumber:      int32(clangErr.line),
				ErrorKind:       pb.Result_MISRA_C_2012_RULE_11_7_EXTERNAL,
				ErrorMessage:    errMessage,
				ExternalMessage: clangErr.message,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func Check13_5(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	taskName := filepath.Base(resultsDir)
	impureFunctions, err := GetImpureFunctions(srcDir, compileCommandsPath, resultsDir, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	rhsFunctions, err := GetLogicalAndOrRHSFunctionCalls(compileCommandsPath, buildActions, taskName, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	// for all impure functions, check whether it appears in && or || operator's right operand.
	for _, function := range impureFunctions {
		decl, exist := rhsFunctions[function]
		if exist {
			errMessage := "[C1602][misra-c2012-13.5]: impure function " + function
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:         decl.File,
				LineNumber:   int32(decl.Line),
				ErrorKind:    pb.Result_MISRA_C_2012_RULE_13_5_IMPURE_FUNCTION,
				ErrorMessage: errMessage,
				Name:         function,
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}

func Check17_2(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	taskName := filepath.Base(resultsDir)
	resultsList := pb.ResultsList{}
	callgraph, err := GetCallGraph(compileCommandsPath, checkRule, resultsDir, buildActions, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	QueryCommands := []string{}
	circles := callgraph.GetCircles()
	// TODO: GetCircles not always return the same result, Bug-Id: 12501
	for _, circle := range circles {
		head := circle[0]
		for _, node := range circle {
			if head > node {
				head = node
			}
		}
		glog.Info(head)
		QueryCommands = append(QueryCommands, fmt.Sprintf(`-c=m functionDecl(allOf(isDefinition(), hasName("%s")))`, head))
	}
	for _, action := range *buildActions {
		actionResults, err := QueryAllWithAction(QueryCommands, taskName, compileCommandsPath, &action, config, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("QueryAllWithAction: %v", err)
		}
		for i, result := range *actionResults {
			decl, err := ParseClangQueryResult(result)
			if err != nil {
				glog.Errorf("ParseClangQueryResult: %v", err)
			}
			if decl != nil {
				circleChain := strings.Join(circles[i], "->")
				errMessage := fmt.Sprintf(
					"[C1507][misra-c2012-17.2]: found a recursive chain (%s)",
					circleChain,
				)
				path := decl.File
				if !filepath.IsAbs(path) {
					path = filepath.Join(action.Command.Directory, decl.File)
				}
				resultsList.Results = append(resultsList.Results, &pb.Result{
					Path:            path,
					LineNumber:      int32(decl.Line),
					ErrorKind:       pb.Result_MISRA_C_2012_RULE_17_2,
					ErrorMessage:    errMessage,
					ExternalMessage: circleChain,
				})
			}

		}
	}
	return &resultsList, nil
}

var ruleMap = map[string]RuleChecker{
	"misra_c_2012/rule_4_2": {
		Check: Check4_2,
	},
	"misra_c_2012/rule_7_4": {
		Check: Check7_4,
	},
	"misra_c_2012/rule_8_1": {
		Check: Check8_1,
	},
	"misra_c_2012/rule_11_1": {
		Check: Check11_1,
	},
	"misra_c_2012/rule_11_6": {
		Check: Check11_6,
	},
	"misra_c_2012/rule_11_7": {
		Check: Check11_7,
	},
	"misra_c_2012/rule_13_5": {
		Check: Check13_5,
	},
	"misra_c_2012/rule_17_2": {
		Check: Check17_2,
	},
	"misra_cpp_2008/rule_2_5_1": {
		Check: CheckMisraCpp2_5_1,
	},
}

func Exec(srcDir, compileCommandsPath string, buildActions *[]csa.BuildAction, checkRule, resultsDir string,
	config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	ruleChecker, exist := ruleMap[checkRule]
	if !exist {
		return nil, errors.New("unrecognized rule name " + checkRule)
	}
	resultsList, err := ruleChecker.Check(srcDir, compileCommandsPath, buildActions, checkRule, resultsDir, config, limitMemory, timeoutNormal, timeoutOom)
	return resultsList, err
}
