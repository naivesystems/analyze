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

package rule_13_5

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	MisraResults := runner.RunMisra(srcdir, "misra_c_2012/rule_13_5", opts)

	LibtoolingResults, err := runner.RunLibtooling(srcdir, "misra/rule_13_5", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	results := &pb.ResultsList{}
	results.Results = append(results.Results, MisraResults.Results...)
	results.Results = append(results.Results, LibtoolingResults.Results...)

	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[C1602][misra-c2012-13.5]") {
			v.ErrorMessage = "[C1602][misra-c2012-13.5]: 逻辑与（\u0026\u0026）和逻辑或（||）运算符的右操作数不得含有持续的副作用"
		}
	}

	results = runner.RemoveDup(results)

	return results, nil
}
