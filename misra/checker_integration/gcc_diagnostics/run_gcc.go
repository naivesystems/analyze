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

package gcc_diagnostics

import (
	"errors"
	"fmt"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func CheckRule8_1(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.Option == "-Wimplicit-int" {
			result := &pb.Result{}
			result.Path = diagnostic.Locations[0].Caret.File
			result.LineNumber = int32(diagnostic.Locations[0].Caret.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_8_1
			result.ErrorMessage = "[C0514][misra-c2012-8.1]: " + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

func CheckRule17_5(diagnostics []Diagnostic) *pb.ResultsList {
	resultList := &pb.ResultsList{}
	for _, diagnostic := range diagnostics {
		if diagnostic.Option == "-Wstringop-overflow=" {
			result := &pb.Result{}
			result.Path = diagnostic.Locations[0].Caret.File
			result.LineNumber = int32(diagnostic.Locations[0].Caret.Line)
			result.ErrorKind = pb.Result_MISRA_C_2012_RULE_17_5_EXTERNAL
			result.ErrorMessage = "[C1504][misra-c2012-17.5]: " + diagnostic.Message
			result.ExternalMessage = diagnostic.Message
			resultList.Results = append(resultList.Results, result)
			glog.Info(result.ErrorMessage)
		}
	}
	return resultList
}

var clangSpecificOptions = []string{
	"-mstack-alignment=",
	"-mretpoline-external-thunk",
}

func FilterClangSpecificOptions(args []string) []string {
	filteredArgs := []string{}
	for _, arg := range args {
		keep := true
		for _, clangSpecificOption := range clangSpecificOptions {
			if strings.HasPrefix(arg, clangSpecificOption) {
				keep = false
				break
			}
		}
		if keep {
			filteredArgs = append(filteredArgs, arg)
		}
	}
	return filteredArgs
}

func TrimTeminationMark(out *[]byte) {
	outStr := string(*out)
	if outStr == "" {
		return
	}
	outStr = strings.Trim(outStr, " ")
	outStr = strings.Trim(outStr, "\n")
	if !strings.HasSuffix(outStr, "]") {
		// The format of the termination mark may be '\ncompilation terminated.'.
		glog.Warningf("The end of the output is not ']'. It may have a termination mark: %s.", outStr)
		outStr = outStr[:strings.LastIndex(outStr, "]")+1]
	}
	*out = []byte(outStr)
}

func GetActionDiagnostics(action csa.BuildAction, resultsDir, csaSystemLibOptions string, limitMemory bool, timeoutNormal, timeoutOom int) ([]Diagnostic, error) {
	args := action.Command.Arguments[1:]
	// run gcc is STU type, only 1 current analyzed file path will exist on the final args position of Command.Argument
	// current action.Command.File is the abs filepath of this source file path
	args[len(args)-1] = action.Command.File // replace the relative filepath with the abs one
	args = append(args, "-fdiagnostics-format=json")
	args = append(args, strings.Fields(csaSystemLibOptions)...)
	gccBin := "gcc"
	if len(action.Command.Arguments) > 0 && strings.HasSuffix(action.Command.Arguments[0], "arm-none-eabi-gcc") {
		gccBin = "arm-none-eabi-gcc"
	}
	args = FilterClangSpecificOptions(args)
	cmd := exec.Command(gccBin, args...)
	cmd.Dir = action.Command.Directory
	taskName := filepath.Base(resultsDir)
	glog.Infof("in %s, cd to :%s", taskName, cmd.Dir)
	glog.Infof("in %s, executing: %s", taskName, cmd.String())
	out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		glog.Warningf("in %s, executing: %s, reported:\n%s", taskName, cmd.String(), string(out))
		if strings.Contains(string(out), "unrecognized command line option") &&
			strings.Contains(string(out), "-fdiagnostics-format=json") {
			return nil, fmt.Errorf("%s does not support -fdiagnostics-format=json", gccBin)
		}
	}
	TrimTeminationMark(&out)
	rs, err := ParseDiagnosticsJson(out)
	if err != nil {
		glog.Errorf("failed to parse JSON: %s", string(out))
	}
	return rs, err
}

func GetGccDiagnostics(compileCommandsPath, resultsDir, csaSystemLibOptions string, buildActions *[]csa.BuildAction, limitMemory bool, timeoutNormal, timeoutOom int) ([]Diagnostic, error) {
	diagnostics := []Diagnostic{}
	for _, action := range *buildActions {
		actionDiagnostics, err := GetActionDiagnostics(action, resultsDir, csaSystemLibOptions, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			return nil, fmt.Errorf("GetActionDiagnostics: %v", err)
		}
		diagnostics = append(diagnostics, actionDiagnostics...)
	}
	return diagnostics, nil
}

func Exec(compileCommandsPath, checkRule, resultsDir, csaSystemLibOptions string, buildActions *[]csa.BuildAction, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	ruleMap := map[string]func([]Diagnostic) *pb.ResultsList{
		"misra_c_2012/rule_8_1":  CheckRule8_1,
		"misra_c_2012/rule_17_5": CheckRule17_5}
	check, exist := ruleMap[checkRule]
	if !exist {
		return nil, errors.New("unrecognized rule name " + checkRule)
	}
	reportJson, err := GetGccDiagnostics(compileCommandsPath, resultsDir, csaSystemLibOptions, buildActions, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	resultsList := check(reportJson)
	return resultsList, err
}
