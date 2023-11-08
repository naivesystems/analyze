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

package rule_5_0_13

import (
	"fmt"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	ds, err := runner.RunClangSema(srcdir, "", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	sema_results := &pb.ResultsList{}
	for _, d := range ds {
		if strings.Contains(d.Message, "is not contextually convertible to 'bool'") {
			sema_results.Results = append(sema_results.Results, &pb.Result{
				Path:         d.Filename,
				LineNumber:   int32(d.Line),
				ErrorMessage: "if语句的条件和迭代语句的条件必须具有bool类型",
			})
		}
	}

	libtooling_results, err := runner.RunLibtooling(srcdir, "misra_cpp_2008/rule_5_0_13", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return sema_results, err
	}

	final_results := &pb.ResultsList{}
	final_results.Results = append(libtooling_results.Results, sema_results.Results...)
	return runner.RemoveDup(final_results), nil
}
