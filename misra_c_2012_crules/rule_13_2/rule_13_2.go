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

package rule_13_2

import (
	"fmt"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/clangsema"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Clang Static Analyzer
	csaReports, err := runner.RunCSA(srcdir, "-analyzer-checker=misra_c_2012.VarUnsequencedAccess", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	csaResults := csa.CheckRule13_2(*csaReports)

	// Clangsema
	clangsemaDiagnostics, err := runner.RunClangSema(srcdir, "-Wunsequenced -Wsequence-point", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	clangsemaResults := clangsema.CheckRule13_2(clangsemaDiagnostics)

	// LibTooling: the order of calling functions may affect the value of the expression
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_13_2", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	// Merge differenct checker's results
	results := &pb.ResultsList{}
	results.Results = append(results.Results, csaResults.Results...)
	results.Results = append(results.Results, clangsemaResults.Results...)
	results.Results = append(results.Results, libtoolingResults.Results...)

	// Change the error message
	for _, result := range results.Results {
		result.ErrorMessage = "[C1605][misra-c2012-13.2]: 采用任意允许采用的求值顺序时，表达式的值和表达式的持续的副作用不得发生改变"
	}
	return results, nil
}
