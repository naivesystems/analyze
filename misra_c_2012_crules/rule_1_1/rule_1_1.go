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

package rule_1_1

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_1"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err1 := runner.RunLibtooling(srcdir, "misra/rule_1_1", checker_integration.Libtooling_STU, opts)
	if err1 != nil {
		glog.Error(err1)
		return nil, err1
	}
	rule_5_1_results, err2 := rule_5_1.Analyze(srcdir, opts)
	if err2 != nil {
		glog.Error(err2)
		return nil, err2
	}
	for _, result := range rule_5_1_results.Results {
		result.ErrorMessage = "[C2201][misra-c2012-1.1]: The program shall contain no violations of the standard C syntax and constraints, and shall not exceed the implementation's translation limits"
		result.ErrorKind = pb.Result_MISRA_C_2012_RULE_1_1_EXTERN_ID_CHAR
	}
	results.Results = append(results.Results, rule_5_1_results.Results...)
	return runner.RemoveDup(results), nil
}
