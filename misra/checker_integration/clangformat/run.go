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

package clangformat

import (
	"os/exec"
	"regexp"
	"strconv"

	"github.com/golang/glog"
	"naive.systems/analyzer/cruleslib/basic"
)

type Report struct {
	Filename     string
	LineNumber   int
	ColumnNumber int
	Message      string
	Category     string
}

func ParseReports(out []byte) (reports []Report, err error) {
	// example output of clang-format:
	// foo.cpp:129:23: error: code should be clang-formatted [-Wclang-format-violations]
	re := regexp.MustCompile(`(.*):(\d*):(\d*): (.*) \[(.*)\]`)
	matches := re.FindAllStringSubmatch(string(out), -1)
	for _, match := range matches {
		linenum, err := strconv.Atoi(match[2])
		if err != nil {
			glog.Errorf("clang-format output cannot be parsed: %s", string(out))
			return reports, err
		}
		colnum, err := strconv.Atoi(match[3])
		if err != nil {
			glog.Errorf("clang-format output cannot be parsed: %s", string(out))
			return reports, err
		}
		reports = append(reports, Report{
			Filename:     match[1],
			LineNumber:   linenum,
			ColumnNumber: colnum,
			Message:      match[4],
			Category:     match[5],
		})
	}
	return reports, nil
}

type Runner struct {
	TaskName      string
	LimitMemory   bool
	TimeoutNormal int
	TimeoutOom    int
}

func (r *Runner) Run(srcs []string, style string) (reports []Report, err error) {
	cmd := exec.Command("clang-format", "-Werror", "--style="+style, "--dry-run")
	cmd.Args = append(cmd.Args, srcs...)
	glog.Info("executing: ", cmd.String())
	out, err := basic.CombinedOutput(cmd, r.TaskName, r.LimitMemory, r.TimeoutNormal, r.TimeoutOom)
	// err == nil ==> no report
	if err == nil {
		return
	}
	if serr, ok := err.(*exec.ExitError); !ok || serr.ExitCode() != 1 {
		glog.Errorf("clang-format execution error: executing %s, reported:\n%s", cmd.String(), string(out))
		return
	}
	// ExitError 1 ==> normal report
	reports, err = ParseReports(out)
	if err != nil {
		glog.Errorf("parse clang-format output error: %v", err)
	}
	return
}
