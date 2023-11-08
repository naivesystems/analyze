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

package rule_6_3_1

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func generateMisraResult(results *pb.ResultsList) *pb.ResultsList {
	for _, result := range results.Results {
		result.ErrorMessage = "构成switch、while、do...while或for语句主体的语句必须为复合语句"
		result.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_6_3_1
	}
	return results
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	cppcheckResults, err := runner.RunCppcheck(srcdir, "misra_cpp_2008/rule_6_3_1", checker_integration.Cppcheck_STU, opts)
	if err != nil {
		glog.Errorf("misra_cpp_2008/rule_6_3_1 Analyze: %v", err)
		return nil, err
	}
	results := generateMisraResult(cppcheckResults)
	return results, nil
}
