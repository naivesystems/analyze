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

package rule_3_9_1

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Refer to the checker of MISRA_C_2012 rule 8.3: All declarations of an object or function
	// shall use the same names and type qualifiers.
	results, err := runner.RunLibtooling(srcdir, "misra_cpp_2008/rule_3_9_1", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}
	for _, r := range results.Results {
		r.ErrorMessage = "在所有声明和重新声明中，用于对象、函数返回类型或函数参数的类型的每个词符必须对应相同"
		r.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_3_9_1
	}
	return results, nil
}
