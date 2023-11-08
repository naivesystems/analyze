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

package rule_2_1

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/checker_integration/infer"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// CSA
	csaReport, err := runner.RunCSA(srcdir, "-analyzer-checker=alpha.deadcode.UnreachableCode", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	csaResults := csa.CheckRule2_1(*csaReport)

	// Infer
	inferReports, err := runner.RunInfer(srcdir, "--bufferoverrun-only --debug-exceptions", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	inferResults := infer.CheckRule2_1(*inferReports)

	// Merge
	results := &pb.ResultsList{}
	results.Results = append(results.Results, csaResults.Results...)
	results.Results = append(results.Results, inferResults.Results...)
	errMsg := "[C2007][misra-c2012-2.1]: 项目不得含有不可达代码"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}

	// Remove Duplicate
	results = runner.RemoveDup(results)

	return results, nil
}
