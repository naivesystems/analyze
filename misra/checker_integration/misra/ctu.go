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
	"errors"
	"fmt"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

type Decl struct {
	File string
	Line int
}

type QueryResult struct {
	Directory   string
	MatchResult []string
}

// Run clang-query with given commands and build actions, then find submatch with regex in output
func QueryWithRegex(queryCommands []string, buildActions *[]csa.BuildAction, config *pb.CheckerConfiguration, regex string) ([]QueryResult, error) {
	allQueryResults := []QueryResult{}
	for _, action := range *buildActions {
		cmdArgs := append(queryCommands, action.Command.File)
		cmd := exec.Command(config.ClangqueryBin, cmdArgs...)
		glog.Info("executing: ", cmd.String())
		out, err := cmd.CombinedOutput()
		if err != nil {
			outStr := string(out)
			glog.Errorf("clang-query execution error: executing: %s, reported:\n%s", cmd.String(), outStr)
			return nil, err
		}
		re := regexp.MustCompile(regex)
		queryResults := re.FindStringSubmatch(string(out))
		if len(queryResults) > 0 {
			allQueryResults = append(allQueryResults, QueryResult{action.Command.Directory, queryResults})
		}
	}
	return allQueryResults, nil
}

type MultipleQueryResult struct {
	Directory           string
	MultipleMatchResult [][]string
}

func QueryAllWithRegex(queryCommands []string, compileCommandsPath string, buildActions *[]csa.BuildAction, taskName string, config *pb.CheckerConfiguration, regex string, limitMemory bool, timeoutNormal, timeoutOom int) ([]MultipleQueryResult, error) {
	allQueryResults := []MultipleQueryResult{}
	for _, action := range *buildActions {
		cmdArgs := append(queryCommands, action.Command.File)
		cmdArgs = append(cmdArgs, "-p", compileCommandsPath)
		cmd := exec.Command(config.ClangqueryBin, cmdArgs...)
		glog.Info("executing: ", cmd.String())
		out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			outStr := string(out)
			glog.Errorf("clang-query execution error: executing: %s, reported:\n%s", cmd.String(), outStr)
			return nil, err
		}
		re := regexp.MustCompile(regex)
		queryResults := re.FindAllStringSubmatch(string(out), -1)
		allQueryResults = append(allQueryResults, MultipleQueryResult{action.Command.Directory, queryResults})
	}
	return allQueryResults, nil
}

// Get function definitions' filename and line number, in CTU files.
func GetFunctionDefinition(buildActions *[]csa.BuildAction, function string, config *pb.CheckerConfiguration) (*Decl, error) {
	queryCommands := []string{
		fmt.Sprintf(`-c=m functionDecl(allOf(isDefinition(), hasName("%s")))`, function),
	}
	// regexp for `somepath/something.c:1:29`
	regex := `([\/a-zA-Z\d]+\.c):(\d+):(\d+)`
	matches, err := QueryWithRegex(queryCommands, buildActions, config, regex)
	if err != nil {
		return nil, err
	}
	if len(matches) > 0 {
		match := matches[0].MatchResult
		linenum, err := strconv.Atoi(match[2])
		if err != nil {
			glog.Error(errors.New("cannot parse clangquery output"))
			return nil, err
		}
		decl := Decl{
			File: filepath.Join(matches[0].Directory, match[1]),
			Line: linenum,
		}
		return &decl, nil
	}
	return nil, fmt.Errorf("GetFunctionDefinition: No query result")
}

func GetLogicalAndOrRHSFunctionCalls(compileCommandsPath string, buildActions *[]csa.BuildAction, taskName string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (map[string]*Decl, error) {
	queryCommands := []string{
		`-c=set bind-root false`, // these three commands control the output format, run `clang-query --help` for more details
		`-c=set output diag`,     // we need function's location (output diag)
		`-c=enable output print`, // we need function's name (output print)
		`-c=l call expr(forEachDescendant(callExpr().bind("call")))`,
		`-c=l rhs hasRHS(call)`,
		`-c=l andor anyOf(hasOperatorName("&&"), hasOperatorName("||"))`,
		`-c=m binaryOperation(andor, rhs)`, // this is the matcher for functions appearing in && or || operator's right operand
	}
	regex := `Match #(\d+):\n\n(.*):(\d*):(\d*):.*\n.*\n.*\nBinding for "call":\n([a-zA-Z_\d]+)\(`
	res := make(map[string]*Decl, 0)
	matches, err := QueryAllWithRegex(queryCommands, compileCommandsPath, buildActions, taskName, config, regex, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	for _, matchResult := range matches {
		multipleMatchResult := matchResult.MultipleMatchResult
		for _, match := range multipleMatchResult {
			linenum, err := strconv.Atoi(match[3])
			if err != nil {
				glog.Error(errors.New("cannot parse clangquery output"))
				return nil, err
			}
			decl := Decl{
				File: filepath.Join(matchResult.Directory, match[2]),
				Line: linenum,
			}
			res[match[5]] = &decl
		}
	}
	return res, nil
}
