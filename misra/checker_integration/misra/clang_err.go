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

type ClangErrorOrWarning struct {
	file    string
	line    int
	column  int
	message string
}

// Get Clang Stderrors or warnings of source files
func GetClangErrorsOrWarnings(compileCommandsPath, resultsDir string, getErrors bool, extraOption []string, buildActions *[]csa.BuildAction, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) ([]ClangErrorOrWarning, error) {
	clangErrsOrWarnings := make([]ClangErrorOrWarning, 0)
	systemLibOptions := config.CsaSystemLibOptions // TODO: remove this config path hack
	taskName := filepath.Base(resultsDir)
	// regexp for somefile.c:12:23 error/warning: some message
	var re *regexp.Regexp
	if getErrors {
		re = regexp.MustCompile(`([a-zA-Z\/_\-\d.]*):(\d*):(\d*): error: (.*)\n`)
	} else {
		re = regexp.MustCompile(`([a-zA-Z\/_\-\d.]*):(\d*):(\d*): warning: (.*)\n`)
	}
	for _, action := range *buildActions {
		cmd_args := []string{"-cc1", "-fsyntax-only"}
		cmd_args = append(cmd_args, extraOption...)
		cmd_args = append(cmd_args, action.Command.File)
		if action.Arch != "" {
			cmd_args = append(cmd_args, "-triple", action.Arch)
		}
		preprocessorSymbols := csa.ParsePreprocessorSymbols(action, true, true)
		cmd_args = append(cmd_args, preprocessorSymbols...)
		cmd_args = append(cmd_args, strings.Fields(systemLibOptions)...)
		cmd_args = append(cmd_args, csa.ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), false)...)
		cmd := exec.Command(config.ClangBin, cmd_args...)
		cmd.Dir = action.Command.Directory
		glog.Infof("in %s, executing: %s", taskName, cmd.String())
		out, _ := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
		matches := re.FindAllStringSubmatch(string(out), -1)
		for _, match := range matches {
			line, err := strconv.Atoi(match[2])
			if err != nil {
				glog.Error(errors.New("cannot parse clang_err/warning line number"))
				return nil, err
			}
			column, err := strconv.Atoi(match[3])
			if err != nil {
				glog.Error(errors.New("cannot parse clang_err/warning column number"))
				return nil, err
			}
			clangErrsOrWarnings = append(clangErrsOrWarnings, ClangErrorOrWarning{
				match[1],
				line,
				column,
				match[4],
			})

		}
	}
	return clangErrsOrWarnings, nil
}

func GetClangErrorsOrWarningsWithProtoReturn(compileCommandsPath, resultsDir string, getErrors bool, extraOption []string, buildActions *[]csa.BuildAction, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (*pb.ResultsList, error) {
	clangErr, err := GetClangErrorsOrWarnings(compileCommandsPath, resultsDir, getErrors, extraOption, buildActions, config, limitMemory, timeoutNormal, timeoutOom)

	if err != nil {
		return nil, err
	}

	resultList := pb.ResultsList{}
	for _, clangErr := range clangErr {
		resultList.Results = append(resultList.Results, &pb.Result{
			Path:       clangErr.file,
			LineNumber: int32(clangErr.line),
			Locations: []*pb.ErrorLocation{
				{
					Path:       clangErr.file,
					LineNumber: int32(clangErr.line),
				},
			},
			ErrorMessage: clangErr.message,
		})
	}
	return &resultList, nil
}
