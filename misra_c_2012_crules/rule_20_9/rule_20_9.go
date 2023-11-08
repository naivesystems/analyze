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

package rule_20_9

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunCppcheck(srcdir, "misra_c_2012/rule_20_9", checker_integration.Cppcheck_STU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	for _, v := range results.Results {
		if strings.HasPrefix(v.ErrorMessage, "[C0106][misra-c2012-20.9]") {
			v.ErrorMessage = "[C0106][misra-c2012-20.9]: 预处理指令 #if 或 #elif 的控制表达式中的所有标识符在求值之前必须被定义（#define）"
		}
	}
	return results, nil
}
