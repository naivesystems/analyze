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

package rule_15_5_3

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Reference:
	// https://clang.llvm.org/extra/clang-tidy/index.html
	// https://clang.llvm.org/extra/clang-tidy/checks/list.html
	args := []string{
		"bugprone-exception-escape,bugprone-unhandled-exception-at-new",
		"-header-filter=.*",
	}
	results, err := runner.RunClangTidy(srcdir, args, opts)
	if err != nil {
		return nil, fmt.Errorf("clangtidy err: %v", err)
	}
	for _, res := range results.Results {
		res.ErrorMessage = "不应隐式调用 terminate() 函数"
		res.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_15_5_3
	}

	return results, nil
}
