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

package clangtidy

import (
	"errors"
	"fmt"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

var checkMap = map[string]string{
	"misra_c_2012/rule_17_2": "misc-no-recursion",
}

var messageMap = map[string]string{
	"misra_c_2012/rule_17_2": "[C1507][misra-c2012-17.2]: ",
}

var errorKindMap = map[string]pb.Result_ErrorKind{
	"misra_c_2012/rule_17_2": pb.Result_MISRA_C_2012_RULE_17_2_EXTERNAL,
}

func Exec(
	compileCommandsPath string,
	buildActions *[]csa.BuildAction,
	checkRule, resultsDir,
	csaSystemLibOptions,
	clangtidyBin string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int,
) (*pb.ResultsList, error) {
	taskName := filepath.Base(resultsDir)
	resultsList := pb.ResultsList{}
	for _, action := range *buildActions {
		if action.Command.ContainsCC1() {
			continue
		}
		sourceFile := action.Command.File
		// TODO: pre-exec checks
		check := checkMap[checkRule]
		cmdArgs := []string{"--checks=-*," + check, sourceFile}
		cmdArgs = append(cmdArgs, fmt.Sprintf("--p=%s", filepath.Dir(compileCommandsPath)))
		for _, field := range strings.Fields(csaSystemLibOptions) {
			cmdArgs = append(cmdArgs, "--extra-arg", field)
		}

		// Hack: add "--" for clang-tidy on CentOS 7.x
		osType, osVersion, err := basic.GetOperatingSystemType()
		if err != nil {
			glog.Errorf("getOperatingSystemType: %v", err)
		}
		if osType == "centos" && osVersion == "7" {
			cmdArgs = append(cmdArgs, "--")
		}

		cmd := exec.Command(clangtidyBin, cmdArgs...)
		cmd.Dir = action.Command.Directory
		glog.Info("executing: ", cmd.String())
		// TODO: Output is based on []byte, maybe exceed buffer size
		out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			glog.Errorf("clang-query execution error: executing: %s, reported:\n%s", cmd.String(), string(out))
			return &resultsList, err
		}
		re := regexp.MustCompile(fmt.Sprintf(`(.*):(\d*):(\d*): (.*) \[%s\]\n`, check))
		matches := re.FindAllStringSubmatch(string(out), -1)
		for _, match := range matches {
			path := filepath.Join(action.Command.Directory, match[1])
			linenum, err := strconv.Atoi(match[2])
			errMessage := messageMap[checkRule] + match[4]
			if err != nil {
				glog.Error(errors.New("ClangTidy output cannot be parsed"))
				return &resultsList, err
			}
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:            path,
				LineNumber:      int32(linenum),
				ErrorKind:       errorKindMap[checkRule],
				ErrorMessage:    errMessage,
				ExternalMessage: match[4],
			})
			glog.Info(errMessage)
		}
	}
	return &resultsList, nil
}
