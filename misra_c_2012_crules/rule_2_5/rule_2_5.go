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

package rule_2_5

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunCppcheck(srcdir, "misra_c_2012/rule_2_5", checker_integration.Cppcheck_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	filteredResults := []*pb.Result{}
	for _, result := range results.Results {
		// The flag is not specified, with the false default value.
		if strings.HasSuffix(result.ErrorMessage, "(optional)") &&
			(opts.JsonOption.CheckIncludeGuards == nil ||
				!*opts.JsonOption.CheckIncludeGuards) {
			continue
		}
		// The error is not in include guards, reported anyway.
		// Or the flag is specified as true,
		// and results with an 'optional' mark should be reported.
		filteredResults = append(filteredResults, result)
	}
	results.Results = filteredResults
	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[C2003][misra-c2012-2.5]") {
			v.ErrorMessage = "[C2003][misra-c2012-2.5]: 项目不应含有未使用的宏声明"
		}
	}
	return results, nil
}
