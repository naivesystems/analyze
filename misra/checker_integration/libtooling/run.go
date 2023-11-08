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

package libtooling

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
	"google.golang.org/protobuf/proto"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func hasDifferentTarget(buildActions *[]csa.BuildAction, target string) (string, bool) {
	for _, action := range *buildActions {
		if target != "" && action.Target != target {
			return action.Target, true
		}
	}
	return "", false
}

func execLibtoolingCmd(binaryName, taskName string, args []string, limitMemory bool, timeoutNormal, timeoutOom int) error {
	cmd := exec.Command(binaryName, args...)

	glog.Info("executing: ", cmd.String())
	out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		outStr := string(out)
		glog.Errorf("in %s, executing: %s, reported:\n%s\n%v\n", binaryName, cmd.String(), outStr, err)
		return err
	}
	return nil
}

func readResultsFromFile(resultsPath string) (*pb.ResultsList, error) {
	content, err := os.ReadFile(resultsPath)
	if err != nil {
		glog.Errorf("failed to read %s: %v", resultsPath, err)
		return nil, err
	}
	fileResults := &pb.ResultsList{}
	err = proto.Unmarshal(content, fileResults)
	if err != nil {
		glog.Errorf("failed to parse %s: %v", resultsPath, err)
		return nil, err
	}
	return fileResults, nil
}

func touchFile(name string) error {
	file, err := os.OpenFile(name, os.O_RDONLY|os.O_CREATE, os.ModePerm)
	if err != nil {
		return err
	}
	return file.Close()
}

func skipAssembly(sourceFiles []string) []string {
	filtered := []string{}
	for _, sourceFile := range sourceFiles {
		if strings.HasSuffix(sourceFile, ".S") || strings.HasSuffix(sourceFile, ".s") {
			glog.Warningf(".S or .s file skipped for Libtooling: %s", sourceFile)
			continue
		}
		filtered = append(filtered, sourceFile)
	}
	return filtered
}

func generateSourceFilesPath(sourceFiles []string, resultDir string) (string, error) {
	fp, err := os.Create(filepath.Join(resultDir, "sourceFile"))
	if err != nil {
		return resultDir, err
	}
	defer fp.Close() // tmp sourceFile will automatically remove in scanner.go cleanWorkingDir() func
	_, err = fp.WriteString(strings.Join(sourceFiles, "\n"))
	if err != nil {
		return fp.Name(), err
	}
	return fp.Name(), nil
}

func generateCTURuns(args []string, sourceFiles []string, resultDir string) [][]string {
	runs := [][]string{}
	ctuArgs := []string{}

	ctuArgs = append(ctuArgs, args...)
	filePath, err := generateSourceFilesPath(sourceFiles, resultDir)
	if err != nil {
		glog.Errorf("libtooling_ctu execution sourcefile path %s generate failed: %v", filePath, err)
		return runs
	}
	ctuArgs = append(ctuArgs, filePath)

	runs = append(runs, ctuArgs)
	return runs
}

func generateSTURuns(args []string, sourceFiles []string) [][]string {
	runs := [][]string{}
	for _, sourceFile := range sourceFiles {
		stuArgs := []string{}
		stuArgs = append(stuArgs, args...)
		stuArgs = append(stuArgs, sourceFile)

		runs = append(runs, stuArgs)
	}
	return runs
}

func ExecLibtooling(
	progName,
	resultsPath,
	resultDir,
	logDir string,
	extraArgs []string,
	buildActions *[]csa.BuildAction,
	sourceFiles []string,
	checkerConfig *pb.CheckerConfiguration,
	checkerType checker_integration.Checker,
	limitMemory bool,
	timeoutNormal,
	timeoutOom int,
	compileCommandsPath string,
) (*pb.ResultsList, error) {
	taskName := filepath.Base(resultDir)
	allArgs := make([]string, 0)
	if logDir != "" {
		allArgs = append(allArgs, fmt.Sprintf("--log-dir=%s", logDir))
	}
	allArgs = append(allArgs, "-@@@")
	allArgs = append(allArgs, fmt.Sprintf("--results-path=%s", resultsPath))
	allArgs = append(allArgs, fmt.Sprintf("--p=%s", filepath.Dir(compileCommandsPath)))

	target := ""
	if len(*buildActions) != 0 {
		target = (*buildActions)[0].Target
		if diffTarget, ok := hasDifferentTarget(buildActions, target); ok {
			glog.Warning("Dectect different target: ", target, diffTarget)
		}
	}

	if target != "" {
		allArgs = append(allArgs, "--extra-arg", "-target", "--extra-arg", target)
	}

	for _, field := range strings.Fields(checkerConfig.CsaSystemLibOptions) {
		allArgs = append(allArgs, "--extra-arg", field)
	}

	if _, err := os.Stat(resultsPath); err != nil {
		glog.Info("creating result file")
		err = touchFile(resultsPath)
		if err != nil {
			glog.Errorf("failed to create result file: %v", err)
			return nil, err
		}
	}

	allArgs = append(allArgs, csa.ParseGccPredefinedMacros(checkerConfig.GetGccPredefinedMacros(), true)...)
	allArgs = append(allArgs, extraArgs...)

	var err error
	allResults := &pb.ResultsList{}
	sourceFiles = skipAssembly(sourceFiles)
	runs := [][]string{}

	if checkerType == checker_integration.Libtooling_CTU {
		runs = generateCTURuns(allArgs, sourceFiles, resultDir)
	} else if checkerType == checker_integration.Libtooling_STU {
		runs = generateSTURuns(allArgs, sourceFiles)
	}

	for _, runArgs := range runs {
		err = execLibtoolingCmd(progName, taskName, runArgs, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			argStr := strings.Join(runArgs, " ")
			glog.Errorf("libtooling execution failed: %v,\nexecution args: %s", err, argStr)
			return nil, err
		}
		readResults, err := readResultsFromFile(resultsPath)
		if err != nil {
			argStr := strings.Join(runArgs, " ")
			glog.Errorf("failed to read libtootling results: %v,\nfilepath: %s,\nexecution args: %s", err, resultsPath, argStr)
			return nil, err
		}
		allResults.Results = append(allResults.Results, readResults.Results...)
	}

	return allResults, nil
}
