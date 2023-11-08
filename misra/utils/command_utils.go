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

package utils

import (
	"bufio"
	"fmt"
	"os/exec"
	"regexp"
	"strings"

	"github.com/golang/glog"
)

// Returns the stdout of a command as string.
func GetCommandStdout(commands []string, workingDir string) ([]string, error) {
	var stdLogs []string
	cmd := exec.Command(commands[0], commands[1:]...)
	if workingDir != "" {
		cmd.Dir = workingDir
	}
	stdoutStderr, err := cmd.CombinedOutput()
	if err != nil {
		return nil, fmt.Errorf("cmd.CombinedOutput: %v", err)
	}
	stdLogs = strings.Fields(string(stdoutStderr))
	return stdLogs, nil
}

// Returns the lines of stdout of a command as string.
func GetCommandStdoutLines(cmd *exec.Cmd, workingDir string) ([]string, error) {
	var stdLogs []string
	if workingDir != "" {
		cmd.Dir = workingDir
	}
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return nil, fmt.Errorf("cmd.StdoutPipe: %v", err)
	}
	if err := cmd.Start(); err != nil {
		return nil, fmt.Errorf("cmd.Start: %v", err)
	}
	in := bufio.NewScanner(stdout)
	for in.Scan() {
		stdLogs = append(stdLogs, string(in.Text()))
	}
	if err := in.Err(); err != nil {
		return stdLogs, fmt.Errorf("bufio.NewScanner: %v", err)
	}
	return stdLogs, nil
}

func GetCurrentClangVersion() string {
	stdoutLines, err := GetCommandStdoutLines(exec.Command("clang", "--version"), ".")
	if err != nil {
		glog.Errorf("getCommandStdoutLines: %v", err)
		return ""
	}
	clangVersionPattern := "(?P<vendor>clang|Apple LLVM) version (?P<clang_version>[^\\s]*)"
	reg := regexp.MustCompile(clangVersionPattern)
	result := reg.FindAllStringSubmatch(strings.Join(stdoutLines, " "), -1)
	return result[0][2]
}

func GetCurrentCppVersion() string {
	stdoutLines, err := GetCommandStdoutLines(exec.Command("c++", "--version"), ".")
	if err != nil {
		glog.Errorf("getCommandStdoutLines: %v", err)
		return ""
	}
	cppVersionPattern := "c\\+\\+ \\((?P<vendor>clang|GCC)\\) (?P<cpp_version>[^\\s]*)"
	reg := regexp.MustCompile(cppVersionPattern)
	result := reg.FindAllStringSubmatch(strings.Join(stdoutLines, " "), -1)
	return strings.Split(result[0][2], ".")[0]
}
