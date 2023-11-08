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

package rule_3_1_2

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func formatReportMessage(results *pb.ResultsList) *pb.ResultsList {
	for _, result := range results.Results {
		result.ErrorMessage = "不得在块作用域内声明函数"
		result.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_3_1_2
	}

	return results
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	result, err := runner.RunLibtooling(srcdir, "misra_cpp_2008/rule_3_1_2", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}
	return formatReportMessage(result), nil
}
