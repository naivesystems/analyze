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

package rule_2_2

import (
	"fmt"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/clangsema"
	"naive.systems/analyzer/misra/checker_integration/infer"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Infer
	inferReports, err := runner.RunInfer(srcdir, "--liveness", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	inferResults := infer.CheckRule2_2(*inferReports)

	// Clangsema
	diagnostics, err := runner.RunClangSema(srcdir, "-Wunused-value", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	clangsemaResults := clangsema.CheckRule2_2(diagnostics)

	// LibTooling
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_2_2", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	// merge differenct checker results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, inferResults.Results...)
	results.Results = append(results.Results, clangsemaResults.Results...)
	results.Results = append(results.Results, libtoolingResults.Results...)

	// change the error message
	errMsg := "[C2006][misra-c2012-2.2]: 不得有死代码"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}

	// remove duplicate
	results = runner.RemoveDup(results)

	return results, nil
}
