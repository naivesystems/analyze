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

package rule_A15_4_3

import (
	"fmt"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}
	ds, err := runner.RunClangSema(srcdir, "", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	for _, d := range ds {
		if strings.Contains(d.Message, "exception specification in declaration does not match previous declaration") ||
			strings.Contains(d.Message, "is missing exception specification") ||
			strings.Contains(d.Message, "exception specification of overriding function is more lax than base version") {
			results.Results = append(results.Results, &pb.Result{
				Path:         d.Filename,
				LineNumber:   int32(d.Line),
				ErrorMessage: "The noexcept specification of a function shall either be identical across all translation units, or identical or more restrictive between a virtual member function and an overrider.",
			})
		}
	}
	return runner.SortResult(results), nil
}
