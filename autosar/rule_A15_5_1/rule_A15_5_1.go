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

package rule_A15_5_1

import (
	"fmt"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Since clang-tidy would find the main function throwing an exception,
	// which is not what we want. Although this is bad programming practice,
	// it should not be checked by rule_A15_5_1.
	//
	// To solve this, we first find all decls and defs of the main function and
	// consider them as errors, and then we remove these errors from the results
	// of clang-tidy checks (if they do exist).

	AutosarResults, err := runner.RunLibtooling(srcdir, "autosar/rule_A15_5_1", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}
	args := []string{
		"bugprone-exception-escape",
	}

	ClangTidyResults, err := runner.RunClangTidy(srcdir, args, opts)
	if err != nil {
		return nil, fmt.Errorf("clangtidy err: %v", err)
	}

	// Process ClangTidyResults
	for _, v := range ClangTidyResults.Results {
		v.ErrorMessage = "All user-provided class destructors, deallocation functions, move constructors, move assignment operators and swap functions shall not exit with an exception. A noexcept exception specification shall be added to these functions as appropriate."
	}
	results := &pb.ResultsList{}
	for _, j := range ClangTidyResults.Results {
		found := false
		for _, i := range AutosarResults.Results {
			if j.LineNumber == i.LineNumber && j.Path == i.Path {
				found = true
				break
			}
		}
		if !found {
			results.Results = append(results.Results, j)
		}
	}

	return runner.RemoveDup(results), nil
}
