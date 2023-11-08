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

package rule_7_5_1

import (
	"fmt"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}

	ds, err := runner.RunClangSema(srcdir, "-Wreturn-stack-address", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	for _, d := range ds {
		// To cover four types of warning messages reported by clangsema:
		// 1) warning: address of stack memory associated with local variable 'x' returned
		// 2) warning: reference to stack memory associated with local variable 'x' returned
		// 3) warning: address of stack memory associated with parameter 'x' returned
		// 4) warning: reference to stack memory associated with parameter 'x' returned
		if strings.Contains(d.Message, "stack memory associated with") {
			results.Results = append(results.Results, &pb.Result{
				Path:         d.Filename,
				LineNumber:   int32(d.Line),
				ErrorMessage: "函数不应返回在该函数中定义的自动变量（包括形参）的引用或指针",
				ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_7_5_1,
			})
		}
	}
	return results, nil
}
