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

package rule_A5_2_5

import (
	"fmt"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	resultsList := &pb.ResultsList{}
	type Loc struct {
		string
		int32
	}
	results := make(map[Loc]struct{})
	report := func(path string, line int32) {
		loc := Loc{path, line}
		if _, reported := results[loc]; !reported {
			results[loc] = struct{}{}
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:         path,
				LineNumber:   line,
				ErrorMessage: "An array or container shall not be accessed beyond its range.",
			})
		}
	}
	// This clang-sema checker can check part of CWE-134.
	ds, err := runner.RunClangSema(srcdir, "-Wformat-security -Wformat", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	for _, d := range ds {
		if strings.Contains(d.Message, "format string is not a string literal") ||
			strings.Contains(d.Message, "cannot mix positional and non-positional arguments in format string") {
			report(d.Filename, int32(d.Line))
		}
	}
	// CSA checkers
	// alpha.security.taint,cwe.TaintArgv -> CWE-134
	// cwe.StackArrayBound,cwe.StackBufferOverflow -> CWE-121
	// cwe.HeapArrayBound,cwe.HeapBufferOverflow -> CWE-122
	// cwe.BufferUnderwrite -> CWE-124
	csa_rs, err := runner.RunCSA(
		srcdir,
		"-analyzer-checker=alpha.security.taint,cwe.TaintArgv,cwe.StackArrayBound,cwe.StackBufferOverflow,cwe.HeapArrayBound,cwe.HeapBufferOverflow,cwe.BufferUnderwrite",
		opts)
	if err != nil {
		return nil, err
	}
	for _, run := range csa_rs.Runs {
		for _, r := range run.Results {
			if (r.RuleId == "alpha.security.taint.TaintPropagation" && strings.Contains(r.Message.Text, "CWE-134")) ||
				r.RuleId == "cwe.StackArrayBound" || r.RuleId == "cwe.StackBufferOverflow" ||
				r.RuleId == "cwe.HeapArrayBound" || r.RuleId == "cwe.HeapBufferOverflow" ||
				r.RuleId == "cwe.BufferUnderwrite" {
				path := strings.Replace(r.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				line := r.Locations[0].PhysicalLocation.Region.StartLine
				report(path, line)
			}
		}
	}
	// cwe.BufferOverread -> CWE-126
	// cwe.BufferUnderread -> CWE-127
	csa_rs, err = runner.RunCSA(
		srcdir,
		"-analyzer-checker=cwe.BufferOverread,cwe.BufferUnderread",
		opts)
	if err != nil {
		return nil, err
	}
	for _, run := range csa_rs.Runs {
		for _, r := range run.Results {
			if r.RuleId == "cwe.BufferOverread" || r.RuleId == "cwe.BufferUnderread" {
				path := strings.Replace(r.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				line := r.Locations[0].PhysicalLocation.Region.StartLine
				report(path, line)
			}
		}
	}
	return runner.SortResult(resultsList), nil
}
