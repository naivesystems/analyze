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

package rule_8_1

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/gcc_diagnostics"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	if opts.JsonOption.Standard == "c99" || opts.JsonOption.Standard == "c11" {
		return &pb.ResultsList{}, nil
	}
	// GCC
	gccDiagnostics, err := runner.RunGCC(opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	gccResults := gcc_diagnostics.CheckRule8_1(gccDiagnostics)

	// Misra
	misraResults := runner.RunMisra(srcdir, "misra_c_2012/rule_8_1", opts)

	// Merge different checker's results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, gccResults.Results...)
	results.Results = append(results.Results, misraResults.Results...)

	// Change the error message
	errMsg := "[C0514][misra-c2012-8.1]: 必须明确指定类型"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}
	// Remove duplicate
	results = runner.RemoveDup(results)

	return results, nil
}
