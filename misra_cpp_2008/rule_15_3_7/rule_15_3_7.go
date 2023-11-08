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

package rule_15_3_7

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	ds, err := runner.RunClangSema(srcdir, "", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	results := &pb.ResultsList{}
	for _, d := range ds {
		if d.Message != "catch-all handler must come last" {
			continue
		}
		results.Results = append(results.Results, &pb.Result{
			Path:         d.Filename,
			LineNumber:   int32(d.Line),
			ErrorMessage: "如果在单个 try-catch 语句或函数尝试块中提供了多个处理程序，则任何省略号（catch-all) 处理程序应最后出现",
			ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_15_3_7,
		})
	}
	return results, nil
}
