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

package rule_7_4

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// libtooling
	libtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_7_4", checker_integration.Libtooling_STU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	// misra
	misraResults := runner.RunMisra(srcdir, "misra_c_2012/rule_7_4", opts)
	// merge
	errMsg := "[C0901][misra-c2012-7.4]: 不得将字符串字面量赋值给对象，除非对象类型为“指向const修饰的char的指针”"
	results := &pb.ResultsList{}
	results.Results = append(results.Results, libtoolingResults.Results...)
	results.Results = append(results.Results, misraResults.Results...)
	for _, result := range results.Results {
		if strings.HasPrefix(result.ErrorMessage, "[C0901][misra-c2012-7.4]") {
			result.ErrorMessage = errMsg
		}
	}
	// remove duplicate
	results = runner.RemoveDup(results)

	return results, nil
}
