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

package rule_A13_3_1

import (
	"fmt"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"strings"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {

	AutosarResults, err := runner.RunLibtooling(srcdir, "autosar/rule_A13_3_1", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}

	args := []string{
		"bugprone-forwarding-reference-overload",
	}
	ClangTidyResults, err := runner.RunClangTidy(srcdir, args, opts)
	if err != nil {
		return nil, fmt.Errorf("clangtidy err: %v", err)
	}
	for _, v := range ClangTidyResults.Results {
		v.ErrorMessage = "[autosar-A13-3-1]" + v.ErrorMessage
	}

	results := &pb.ResultsList{}
	results.Results = append(results.Results, AutosarResults.Results...)
	results.Results = append(results.Results, ClangTidyResults.Results...)

	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[autosar-A13-3-1]") {
			v.ErrorMessage = "A function that contains \"forwarding reference\" as its argument shall not be overloaded."
		}
	}

	return runner.RemoveDup(results), err
}
