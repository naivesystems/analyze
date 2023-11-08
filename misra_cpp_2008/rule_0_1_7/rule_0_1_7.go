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

package rule_0_1_7

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_7"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := rule_17_7.Analyze(srcdir, opts)
	if err != nil {
		return nil, err
	}
	for _, result := range results.Results {
		result.ErrorMessage = "具有非void返回类型的函数，只要不是重载运算符，那么它返回的值必须被使用"
		result.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_0_1_7
	}
	// misra C implement will try to match the use of function, like 'f(a, b)'. So
	// the operator overload in C++ will not be match. We can reuse it for misra C++.
	return results, nil
}
