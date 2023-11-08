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

package rule_A16_2_2

import (
	"bufio"
	"io"
	"os"
	"os/exec"
	"path"
	"regexp"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

type TempFileInfo struct {
	file           *os.File
	sourceFileName string
}

var tempFileNamePattern = "compile_test-*"

func copyToTempFile(sourceFile string, dir string) (*os.File, error) {
	src, err := os.Open(sourceFile)
	if err != nil {
		glog.Errorf("failed to open source files: %v", err)
		return nil, err
	}
	defer src.Close()

	tempFile, err := os.CreateTemp(dir, tempFileNamePattern+"-"+path.Base(sourceFile))
	if err != nil {
		glog.Errorf("failed to create temporary file: %v", err)
		return tempFile, err
	}

	_, err = io.Copy(tempFile, src)
	if err != nil {
		glog.Errorf("failed to copy source files: %v", err)
		return tempFile, err
	}
	return tempFile, nil
}

func runCompileTest(tempFileInfoArray []TempFileInfo, compileCmdName string, compileCmdArgs []string, results *pb.ResultsList) error {
	for _, tempFileInfo := range tempFileInfoArray {
		file := tempFileInfo.file
		_, err := file.Seek(0, io.SeekStart)
		if err != nil {
			return err
		}
		reader := bufio.NewReader(file)
		pos := int64(0)
		lineNumber := 0
		for {
			lineNumber++
			line, readerErr := reader.ReadString('\n')
			// match "#include xxx"
			rInclude := regexp.MustCompile(`^\s*?#\s*?include`)
			matchResult := rInclude.FindString(line)
			if matchResult != "" {
				// replace "#include xxx" with space
				bytes := []byte{}
				for i := 0; i < len(line); i++ {
					bytes = append(bytes, byte(' '))
				}
				_, err := file.WriteAt(bytes, pos)
				if err != nil {
					return err
				}
				// try to compile the modified file
				cmd := exec.Command(compileCmdName, compileCmdArgs...)
				_, err = cmd.CombinedOutput()
				if err != nil {
					// if this line cannot be deleted, then write it back
					bytes := []byte(line)
					_, err := file.WriteAt(bytes, pos)
					if err != nil {
						return err
					}
				} else {
					// we have found a line of '#include' which can be deleted without compilation error, so it is unused
					// report error on this line
					results.Results = append(results.Results, &pb.Result{
						Path:         tempFileInfo.sourceFileName,
						LineNumber:   int32(lineNumber),
						ErrorMessage: "There shall be no unused include directives.",
					})
				}
			}
			pos += int64(len(line))
			if readerErr != nil {
				break
			}
		}
	}
	return nil
}

func removeTempFiles(tempFileInfoArray []TempFileInfo) {
	for _, tempFileInfo := range tempFileInfoArray {
		os.Remove(tempFileInfo.file.Name())
	}
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}

	sourceFiles, err := runner.GetSourceFiles(srcdir, opts)
	if err != nil {
		glog.Errorf("failed to get source files: %v", err)
	}

	cmdArgs := []string{"-x", "c++-header"}
	cmdArgs = append(cmdArgs, "-I"+srcdir)
	cmdArgs = append(cmdArgs, strings.Fields(opts.EnvOption.CheckerConfig.CsaSystemLibOptions)...)

	resultDir := opts.RuleSpecificOption.RuleSpecificResultDir

	tempFileInfoArray := []TempFileInfo{}
	// copy content of cc files into temp files
	for _, sourceFile := range sourceFiles {
		tempFile, err := copyToTempFile(sourceFile, resultDir)
		if err != nil {
			return results, err
		}

		tempFileInfoArray = append(tempFileInfoArray, TempFileInfo{file: tempFile, sourceFileName: sourceFile})
		cmdArgs = append(cmdArgs, tempFile.Name())
	}
	defer removeTempFiles(tempFileInfoArray)
	// try to compile the unmodified file copies
	// if there is error, return
	cmd := exec.Command(opts.EnvOption.CheckerConfig.ClangBin, cmdArgs...)
	_, err = cmd.CombinedOutput()
	if err != nil {
		return results, err
	}
	err = runCompileTest(tempFileInfoArray, opts.EnvOption.CheckerConfig.ClangBin, cmdArgs, results)
	if err != nil {
		return results, err
	}
	return results, nil
}
