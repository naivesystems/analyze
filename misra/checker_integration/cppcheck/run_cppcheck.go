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

package cppcheck

import (
	"bytes"
	"encoding/json"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

type CppCheckXMLLocation struct {
	File   string `xml:"file,attr"`
	Line   int32  `xml:"line,attr"`
	Column string `xml:"column,attr"`
}

type CppCheckXMLError struct {
	Id       string              `xml:"id,attr"`
	Severity string              `xml:"severity,attr"`
	Msg      string              `xml:"msg,attr"`
	Location CppCheckXMLLocation `xml:"location"`
}

type CppCheckXMLReport struct {
	Errors  []CppCheckXMLError `xml:"errors>error"`
	Version string             `xml:"cppcheck"`
}

func ExecCppcheckBinary(directory, resultsDir string, cmd_args []string, inputCppcheckBin string, limitMemory bool) ([]byte, error) {
	// If invoked by checker_integration.cmd.main, the cppcheck bin might be a relative path instead of an absolute path.
	cppcheckBin, err := filepath.Abs(inputCppcheckBin)
	if err != nil {
		glog.Errorf("cppecheck bin not found in %s", inputCppcheckBin)
		return nil, err
	}
	cmd := exec.Command(cppcheckBin, cmd_args...)
	cmd.Dir = directory
	var cmderr bytes.Buffer
	cmd.Stderr = &cmderr
	glog.Info("executing: ", cmd.String())
	if resultsDir == "" { // used for preanalyze
		err = cmd.Run()
	} else { // used for specific rule
		taskName := filepath.Base(resultsDir)
		err = basic.Run(cmd, taskName, limitMemory)
	}
	if exitError, ok := err.(*exec.ExitError); ok {
		if exitError.ExitCode() != 0 {
			glog.Error("failed to generate XML dump: " + directory)
		}
	}
	return cmderr.Bytes(), err
}

func execByFileList(filePathList []string, checkRule, resultsDir string, inputCppcheckBin, inputPythonBin string, dumpError *string, limitMemory bool, timeoutNormal, timeoutOom int) ([]*pb.Result, error) {
	taskName := filepath.Base(resultsDir)
	misraPyPath := filepath.Join(filepath.Dir(inputCppcheckBin), "addons", "misra.py")
	_, err := os.Stat(misraPyPath)
	if errors.Is(err, os.ErrNotExist) {
		return nil, errors.New("misra.py not found in: " + misraPyPath)
	}
	cmd_args := []string{misraPyPath, "--check_rules=" + checkRule, "--output_dir", resultsDir}
	fileCount := 0
	for _, filePath := range filePathList {
		if strings.HasSuffix(filePath, ".S.c99.dump") || strings.HasSuffix(filePath, ".S.c90.dump") ||
			strings.HasSuffix(filePath, ".s.c99.dump") || strings.HasSuffix(filePath, ".s.c90.dump") {
			glog.Warningf("in %s, .S or .s dump file skipped for Cppcheck: %s", taskName, filePath)
			continue
		}
		_, err = os.Stat(filePath)
		if errors.Is(err, os.ErrNotExist) {
			glog.Errorf("in %s: %v", taskName, errors.New("file not found: "+filePath))
			continue
		}
		cmd_args = append(cmd_args, filePath)
		fileCount += 1
	}
	if fileCount == 0 {
		return []*pb.Result{}, nil
	}
	if dumpError != nil {
		cmd_args = append(cmd_args, "--create-dump-error", *dumpError)
	}
	cmd := exec.Command(inputPythonBin, cmd_args...)
	glog.Infof("in %s, executing: %s", taskName, cmd.String())
	out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		outStr := string(out)
		if strings.Contains(outStr, "Traceback (most recent call last):") {
			glog.Errorf("in %s, executing: %s, reported:\n%s", taskName, cmd.String(), outStr)
			return nil, fmt.Errorf("in %s, executing: %s, reported:\n%s", taskName, cmd.String(), outStr)
		} else {
			// cppcheck will exit with 1 if found violations, it's not an error.
			// TODO: handle real crashed bug.
			glog.Infof(outStr)
		}
	}
	reportFilePath := filepath.Join(resultsDir, "cppcheck_out.json")
	reportFile, err := os.Open(reportFilePath)
	if err != nil {
		glog.Errorf("in %s: cppcheck result not found: %s, maybe run python scripts failed", taskName, reportFilePath)
		return nil, err
	}
	defer reportFile.Close()
	reportContent, err := io.ReadAll(reportFile)
	if err != nil {
		return nil, err
	}
	var currentResults []*pb.Result
	err = json.Unmarshal(reportContent, &currentResults)
	if err != nil {
		return nil, err
	}
	return currentResults, nil
}

// Get the cppCheck binary generated XML dump file results. Don't run misra.py.
func GetCppCheckCoreXMLErrors(
	ruleName string,
	jsonOptions checkrule.JSONOption,
	compileCommandsPath string,
	resultsDir string,
	dumpDir string,
	buildActions *[]csa.BuildAction,
	inputCppcheckBin string,
	dumpErrors map[string]string,
	limitMemory bool) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	standard := jsonOptions.Standard
	for _, action := range *buildActions {
		sourceFile := action.Command.File
		// read cppcheck xml output
		var currentResults []*pb.Result
		cmd_args := []string{"--abspath", ruleName, "--xml", "--quiet", "--max-configs=1", "--std=" + standard, action.Command.File}
		xmlData, err := ExecCppcheckBinary(action.Command.Directory, resultsDir, cmd_args, inputCppcheckBin, limitMemory)
		if err != nil {
			glog.Errorf("get xml output file for cppcheck %v failed, %v ", sourceFile, err)
			return nil, err
		}
		cppCheckResult := CppCheckXMLReport{}
		err = xml.Unmarshal(xmlData, &cppCheckResult)
		if err != nil {
			glog.Errorf("unmarshal cppcheck errors xml: %v", err)
			return nil, err
		}
		for _, e := range cppCheckResult.Errors {
			currentResults = append(currentResults, &pb.Result{
				Path:         e.Location.File,
				LineNumber:   e.Location.Line,
				ErrorMessage: e.Msg,
				Name:         e.Id,
			})
		}
		resultsList.Results = append(resultsList.Results, currentResults...)
	}
	return &resultsList, nil
}

func ExecSTU(
	ruleName string,
	jsonOptions checkrule.JSONOption,
	compileCommandsPath string,
	resultsDir string,
	dumpDir string,
	buildActions *[]csa.BuildAction,
	inputCppcheckBin,
	inputPythonBin string,
	dumpErrors map[string]string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	standard := jsonOptions.Standard
	for _, action := range *buildActions {
		sourceFile := action.Command.File
		dumpFilePath := filepath.Join(dumpDir, sourceFile) + "." + standard + ".dump"
		var dumpErrorPointer *string = nil
		if dumpError, ok := dumpErrors[dumpFilePath]; ok {
			dumpErrorPointer = &dumpError
		}
		currentResults, err := execByFileList(
			[]string{dumpFilePath},
			ruleName,
			resultsDir,
			inputCppcheckBin,
			inputPythonBin,
			dumpErrorPointer,
			limitMemory,
			timeoutNormal,
			timeoutOom)
		if err != nil {
			return &resultsList, err
		}
		resultsList.Results = append(resultsList.Results, currentResults...)
	}
	return &resultsList, nil
}

func ExecCTU(
	ruleName string,
	jsonOptions checkrule.JSONOption,
	compileCommandsPath string,
	resultsDir string,
	dumpDir string,
	buildActions *[]csa.BuildAction,
	inputCppcheckBin,
	inputPythonBin string,
	dumpErrors map[string]string,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int) (*pb.ResultsList, error) {
	resultsList := pb.ResultsList{}
	standard := jsonOptions.Standard

	filePaths := []string{}
	for _, action := range *buildActions {
		sourceFile := action.Command.File
		dumpFilePath := filepath.Join(dumpDir, sourceFile) + "." + standard + ".dump"
		ctuInfoFilePath := filepath.Join(dumpDir, sourceFile) + "." + standard + ".ctu-info"
		filePaths = append(filePaths, dumpFilePath, ctuInfoFilePath)
	}
	currentResults, err := execByFileList(filePaths, ruleName, resultsDir, inputCppcheckBin, inputPythonBin, nil, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return &resultsList, err
	}
	resultsList.Results = append(resultsList.Results, currentResults...)
	return &resultsList, nil
}
