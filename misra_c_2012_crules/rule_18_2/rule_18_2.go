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

package rule_18_2

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	csaReports, err := runner.RunCSA(srcdir, "-analyzer-checker=misra_c_2012.PointerSubMisra", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	results := csa.CheckRule18_2(*csaReports)

	// Change the error messages
	errMsg := "[C1307][misra-c2012-18.2]: 只有寻址同一数组元素的指针之间才能进行减法运算"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}
	return results, nil
}
