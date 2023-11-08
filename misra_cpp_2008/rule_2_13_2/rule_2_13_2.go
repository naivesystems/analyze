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

package rule_2_13_2

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra_c_2012_crules/rule_7_1"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunLibtooling(srcdir, "misra_cpp_2008/rule_2_13_2", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return results, fmt.Errorf("run libtooling err: %v", err)
	}
	rs, err := rule_7_1.Analyze(srcdir, opts)
	if err != nil {
		return nil, err
	}
	for _, r := range rs.Results {
		results.Results = append(results.Results, &pb.Result{
			Path:         r.Path,
			LineNumber:   r.LineNumber,
			ErrorMessage: "不得使用八进制常量（除零以外）和八进制转义序列（除“\\0”以外）",
			ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_2_13_2,
		})
	}
	return results, nil
}
