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

package rule_15_3_6

import (
	"fmt"
	"regexp"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

var pattern = regexp.MustCompile("exception of type .* will be caught by earlier handler")

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	ds, err := runner.RunClangSema(srcdir, "-Wexceptions", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	results := &pb.ResultsList{}
	for _, d := range ds {
		if pattern.MatchString(d.Message) {
			results.Results = append(results.Results, &pb.Result{
				Path:         d.Filename,
				LineNumber:   int32(d.Line),
				ErrorMessage: "如果在单个 try-catch 语句，或派生类及其部分或全部基类的函数尝试块中，提供多个处理程序，则处理程序应按最派生到基类的顺序排列",
				ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_15_3_6,
			})
		}
	}
	return results, nil
}
