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

package rule_13_4

import (
	"fmt"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/clangsema"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Clangsema
	clangsemaDiagnostics, err := runner.RunClangSema(srcdir, "-Wparentheses", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	clangsemaResults := clangsema.CheckRule13_4(clangsemaDiagnostics)

	// LibTooling
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_13_4", checker_integration.Libtooling_STU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	// Merge differenct checker's results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, clangsemaResults.Results...)
	results.Results = append(results.Results, libtoolingResults.Results...)

	// Change the error message
	for _, result := range results.Results {
		result.ErrorMessage = "[C1603][misra-c2012-13.4]: 不得使用赋值运算符的结果"
	}
	return results, nil
}
