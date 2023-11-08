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

package rule_8_7

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunLibtooling(srcdir, "misra/rule_8_7", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	finalResult := &pb.ResultsList{}
	for _, v := range results.Results {
		firstDeclPath := v.OtherFilename
		// Do not report if the first declaration is not in the file under srcdir
		if !strings.HasPrefix(firstDeclPath, strings.TrimSuffix(srcdir, "/")+"/") {
			continue
		}
		if strings.HasPrefix(v.ErrorMessage, "[C0508][misra-c2012-8.7]") {
			v.ErrorMessage = "[C0508][misra-c2012-8.7]: 不应使用外部链接定义仅在一个翻译单元中被引用的函数和对象"
		}
		finalResult.Results = append(finalResult.Results, v)
	}
	return finalResult, nil
}
