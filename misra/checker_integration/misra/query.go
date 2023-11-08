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
	"fmt"
	"os/exec"
	"regexp"
	"strconv"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func QueryAllWithAction(queryCommands []string, taskName, compileCommandsPath string, action *csa.BuildAction, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*[]string, error) {
	cmdArgs := append(queryCommands, action.Command.File)
	for _, field := range strings.Fields(config.CsaSystemLibOptions) {
		cmdArgs = append(cmdArgs, "--extra-arg", field)
	}
	cmdArgs = append(cmdArgs, "-p", compileCommandsPath)
	cmd := exec.Command(config.ClangqueryBin, cmdArgs...)
	glog.Info("executing: ", cmd.String())
	out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		outStr := string(out)
		glog.Errorf("clang-query execution error: executing: %s, reported:\n%s", cmd.String(), outStr)
		return nil, err
	}
	results := SplitClangQueryResults(string(out))
	if len(*results) != len(queryCommands) { // Sanity check
		glog.Errorf("length of clang-query result != lenght of query commands")
	}
	return results, nil
}

func SplitClangQueryResults(clangQueryOutput string) *[]string {
	results := []string{}
	currentResult := []string{}
	re := regexp.MustCompile(`\d+ match(es)?\.`) // `0 matches.` or `1 match.`
	lines := strings.Split(clangQueryOutput, "\n")
	for _, line := range lines {
		currentResult = append(currentResult, line)
		if re.MatchString(line) {
			results = append(results, strings.Join(currentResult, "\n"))
			currentResult = []string{}
		}
	}
	return &results
}

func ParseClangQueryResult(rawQueryResult string) (*Decl, error) {
	regExpr := regexp.MustCompile(`([\/a-zA-Z-_\d]+\.c):(\d+):(\d+)`) // for `xx/xx.c:1:2`
	match := regExpr.FindStringSubmatch(rawQueryResult)
	if len(match) > 0 {
		linenum, err := strconv.Atoi(match[2])
		if err != nil {
			return nil, fmt.Errorf("failed to parse line number: %s", match[1])
		}
		return &Decl{match[1], linenum}, nil
	} else { // If the query result is `0 matches.`, just return nil with no error
		return nil, nil
	}
}
