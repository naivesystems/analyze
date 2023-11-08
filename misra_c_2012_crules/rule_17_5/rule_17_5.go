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

package rule_17_5

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/gcc_diagnostics"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// GCC
	diagnostics, err := runner.RunGCC(opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	gccResults := gcc_diagnostics.CheckRule17_5(diagnostics)

	// LibTooling
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_17_5", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	// Merge the different checker's results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, gccResults.Results...)
	results.Results = append(results.Results, libtoolingResults.Results...)

	// Change the error message
	for _, result := range results.Results {
		result.ErrorMessage = "[C1504][misra-c2012-17.5]: 如果函数形参被声明为数组类型，那么对应的实参必须具有适当数量的元素"
	}

	results = runner.RemoveDup(results)
	return results, nil
}
