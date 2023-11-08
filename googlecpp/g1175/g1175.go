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

package g1175

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	reports, err := runner.RunCpplint(srcdir, "-,+readability/inheritance", opts)
	if err != nil {
		return nil, err
	}
	type Loc struct {
		string
		int32
	}
	results := make(map[Loc]struct{})
	resultsList := &pb.ResultsList{}
	report := func(path string, line int32) {
		loc := Loc{path, line}
		if _, reported := results[loc]; !reported {
			results[loc] = struct{}{}
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:         path,
				LineNumber:   line,
				ErrorMessage: "Do not use virtual when declaring an override",
			})
		}
	}
	for _, r := range reports {
		report(r.Path, int32(r.LineNumber))
	}
	return resultsList, nil
}
