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

package rule_0_1_3

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}
	// It seems that cppcheck cannot specify enable only one rule id.
	cppcheck_results, err := runner.RunCppcheck(srcdir, "--enable=style", checker_integration.Cppcheck_Binary, opts)
	if err != nil {
		return nil, err
	}
	for _, r := range cppcheck_results.Results {
		if r.Name != "unusedStructMember" {
			continue
		}
		r.ErrorMessage = "shall not contain unused variables"
		results.Results = append(results.Results, r)
	}
	ds, err := runner.RunClangSema(srcdir, "-Wunused -Wunused-but-set-parameter -Wunused-parameter", opts)
	if err != nil {
		return nil, err
	}
	for _, d := range ds {
		results.Results = append(results.Results, &pb.Result{
			Path:         d.Filename,
			LineNumber:   int32(d.Line),
			ErrorMessage: "shall not contain unused variables",
		})
	}
	for _, result := range results.Results {
		result.ErrorMessage = "项目不得含有未使用的变量"
		result.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_0_1_3
	}
	return results, nil
}
