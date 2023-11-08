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

package rule_5_1

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunLibtooling(srcdir, "misra/rule_5_1", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	// rewrite error messages
	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[C1109][misra-c2012-5.1]: contain non-ASCII characters") {
			v.ErrorMessage = "[C1109][misra-c2012-5.1]: 包含非ASCII字符"
		} else if strings.HasPrefix(v.ErrorMessage, "[C1109][misra-c2012-5.1]: violation of misra-c2012-5.1") {
			v.ErrorMessage = "[C1109][misra-c2012-5.1]: 不得使用重名的外部标识符"
		}
	}
	return results, nil
}
