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

package rule_A18_0_2

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// atof, atoi and atol shall not be used
	args := []string{
		"cert-err34-c",
	}
	results, err := runner.RunClangTidy(srcdir, args, opts)
	if err != nil {
		return nil, fmt.Errorf("clangtidy err: %v", err)
	}
	for _, results := range results.Results {
		results.ErrorMessage = "The error state of a conversion from string to a numeric value shall be checked."
	}

	// stream related conversion
	libtoolingResults, err := runner.RunLibtooling(srcdir, "autosar/rule_A18_0_2", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}
	results.Results = append(results.Results, libtoolingResults.Results...)

	return results, nil
}
