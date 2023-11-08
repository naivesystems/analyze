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

package rule_17_0_3

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	rs, err := runner.RunCppcheck(srcdir, "misra_cpp_2008/rule_17_0_3", checker_integration.Cppcheck_STU, opts)
	if err != nil {
		return nil, err
	}
	for _, r := range rs.Results {
		r.ErrorMessage = "标准库函数的名称不应被覆盖"
		r.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_17_0_3
	}
	return rs, nil
}
