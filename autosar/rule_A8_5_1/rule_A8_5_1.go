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

package rule_A8_5_1

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	diagnostics, err := runner.RunClangSema(srcdir, "-Wreorder-ctor", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	results := &pb.ResultsList{}
	for _, d := range diagnostics {
		results.Results = append(results.Results, &pb.Result{
			Path:         d.Filename,
			LineNumber:   int32(d.Line),
			ErrorMessage: "In an initialization list, the order of initialization shall be following: (1) virtual base classes in depth and left to right order of the inheritance graph, (2) direct base classes in left to right order of inheritance list, (3) non-static data members in the order they were declared in the class definition.",
		})
	}
	return results, nil
}
