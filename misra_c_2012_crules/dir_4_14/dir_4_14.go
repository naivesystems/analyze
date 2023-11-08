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

package dir_4_14

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// Clang Static Analyzer
	csaReports, err := runner.RunCSA(srcdir, "-analyzer-checker=misra_c_2012.TaintMisra", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	results := csa.CheckDir4_14(*csaReports)

	// Change the error message
	errMsg := "[C2317][misra-c2012-dir-4.14]: 必须检查从外部来源获取的值的有效性"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}

	return results, nil
}
