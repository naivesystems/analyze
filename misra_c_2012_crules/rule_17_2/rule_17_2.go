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

package rule_17_2

import (
	"fmt"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	MisraResults := runner.RunMisra(srcdir, "misra_c_2012/rule_17_2", opts)

	args := []string{
		"misc-no-recursion",
	}
	ClangTidyResults, err := runner.RunClangTidy(srcdir, args, opts)
	if err != nil {
		return nil, fmt.Errorf("clangtidy err: %v", err)
	}
	for _, v := range ClangTidyResults.Results {
		v.ErrorMessage = "[C1507][misra-c2012-17.2]" + v.ErrorMessage
	}

	results := &pb.ResultsList{}
	results.Results = append(results.Results, MisraResults.Results...)
	results.Results = append(results.Results, ClangTidyResults.Results...)

	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[C1507][misra-c2012-17.2]") {
			v.ErrorMessage = "[C1507][misra-c2012-17.2]: 函数不得直接或间接调用自身"
		}
	}

	results = runner.RemoveDup(results)

	return results, nil
}
