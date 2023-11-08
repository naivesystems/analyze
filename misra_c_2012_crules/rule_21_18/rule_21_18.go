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

package rule_21_18

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/checker_integration/infer"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// CSA
	csaReports, err := runner.RunCSA(srcdir, "-analyzer-checker=core,alpha.unix.cstring.OutOfBounds", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	csaResults := csa.CheckRule21_18(*csaReports)

	// Infer
	inferReports, err := runner.RunInfer(srcdir, "--bufferoverrun --debug-exceptions", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	inferResults := infer.CheckRule21_18(*inferReports)

	// libtooling
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_21_18", checker_integration.Libtooling_STU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	// Merge different checker's results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, csaResults.Results...)
	results.Results = append(results.Results, inferResults.Results...)
	results.Results = append(results.Results, libtoolingResults.Results...)

	// Change the error message
	errMsg := "[C0403][misra-c2012-21.18]: 如果要将 size_t 实参传递给 <cstring.h> 中的任意函数，则 size_t 实参必须具有恰当的值"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}

	// Remove duplicate
	results = runner.RemoveDup(results)

	return results, nil
}
