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

package rule_11_1

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	MisraResults := runner.RunMisra(srcdir, "misra_c_2012/rule_11_1", opts)

	LibtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_11_1", checker_integration.Libtooling_STU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	results := &pb.ResultsList{}
	results.Results = append(results.Results, MisraResults.Results...)
	results.Results = append(results.Results, LibtoolingResults.Results...)

	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[C1409][misra-c2012-11.1]") {
			v.ErrorMessage = "[C1409][misra-c2012-11.1]: 指向函数的指针只能与指向函数的指针相互转换，且指针与函数的类型必须兼容"
		}
	}

	results = runner.RemoveDup(results)

	return runner.RemoveDup(results), nil
}
