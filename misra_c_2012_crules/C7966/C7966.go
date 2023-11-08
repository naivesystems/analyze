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

package C7966

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func alterErrorMsg(results *pb.ResultsList) *pb.ResultsList {
	for _, result := range results.Results {
		result.ErrorMessage = "malloc分配的大小应为4的倍数"
	}
	return results
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results_libtooling, err := runner.RunLibtooling(srcdir, "misra/C7966", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}

	results_cppcheck, err := runner.RunCppcheck(srcdir, "naivesystems/C7966", checker_integration.Cppcheck_STU, opts)
	if err != nil {
		glog.Errorf("error when executing cppcheck: %v", err)
		return nil, err
	}
	results_libtooling.Results = append(results_libtooling.Results, results_cppcheck.Results...)

	final := &pb.ResultsList{}
	allKeys := make(map[string]bool)
	for _, res := range results_libtooling.Results {
		key := (res.Filename + string(res.LineNumber))
		if _, value := allKeys[key]; !value {
			allKeys[key] = true
			final.Results = append(final.Results, res)
		}
	}
	return alterErrorMsg(final), nil
}
