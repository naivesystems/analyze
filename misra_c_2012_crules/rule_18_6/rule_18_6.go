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

package rule_18_6

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	csaReports, err := runner.RunCSA(srcdir, "-analyzer-checker=core.StackAddressEscape", opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	results := csa.CheckRule18_6(*csaReports)

	// Change the error messages
	errMsg := "[C1303][misra-c2012-18.6]: 不得将自动存储对象的地址复制给在该对象不复存在后仍然存在的另一个对象"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}
	return results, nil
}
