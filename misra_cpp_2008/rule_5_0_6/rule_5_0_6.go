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

package rule_5_0_6

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunLibtooling(srcdir, "misra_cpp_2008/rule_5_0_6", checker_integration.Libtooling_STU, opts)
	// merge two types of cast error messages
	newResults := &pb.ResultsList{}
	for _, result := range results.Results {
		if runner.HasTargetErrorMessageFragment(result, "隐式的浮点转换不应使底层类型的大小变小") || runner.HasTargetErrorMessageFragment(result, "隐式的整数转换不应使底层类型的大小变小") {
			result.ErrorMessage = "隐式的整数或浮点转换不应使底层类型的大小变小"
			newResults.Results = append(newResults.Results, result)
		}
	}
	return newResults, err
}
