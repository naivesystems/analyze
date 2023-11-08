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

package rule_A2_13_2

import (
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := pb.ResultsList{}
	resultFromClang, err := runner.RunClangForErrorsOrWarnings(srcdir, true, "", opts)
	if err != nil {
		return nil, err
	}
	for _, clangErr := range resultFromClang.Results {
		if strings.Contains(clangErr.ErrorMessage, "unsupported non-standard concatenation of string literals") {
			clangErr.ErrorMessage = "String literals with different encoding prefixes shall not be concatenated."
			results.Results = append(results.Results, clangErr)
		}
	}
	return runner.RemoveDup(&results), err
}
