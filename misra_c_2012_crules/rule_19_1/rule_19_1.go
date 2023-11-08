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

package rule_19_1

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Clang Static Analyzer
	csaReports, err := runner.RunCSA(srcdir, "-analyzer-checker=alpha.unix.cstring.BufferOverlap", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	csaResults := csa.CheckRule19_1(*csaReports)

	// LibTooling
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_19_1", checker_integration.Libtooling_STU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	// Merge different checker's results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, csaResults.Results...)
	results.Results = append(results.Results, libtoolingResults.Results...)

	// Change the error messages
	errMsg := "[C0302][misra-c2012-19.1]: 不得将对象赋值或复制给与其重叠的对象"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}

	// Remove duplicate
	results = runner.RemoveDup(results)

	return results, nil
}
