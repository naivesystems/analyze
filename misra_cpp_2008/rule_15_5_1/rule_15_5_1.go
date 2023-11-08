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

package rule_15_5_1

import (
	"fmt"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	final_res := &pb.ResultsList{}
	results, err := runner.RunClangSema(srcdir, "-fcxx-exceptions -Wexceptions", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	for _, res := range results {
		if strings.Contains(res.Message, "destructor has a implicit non-throwing exception specification") {
			final_res.Results = append(final_res.Results, &pb.Result{
				Path:         res.Filename,
				LineNumber:   int32(res.Line),
				ErrorMessage: "一个类析构函数不应该随着异常退出",
				ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_15_5_1,
			})
		}
	}

	return final_res, nil
}
