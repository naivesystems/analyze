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

package rule_A14_7_2

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	misra_cpp_2008_rule_14_7_3 "naive.systems/analyzer/misra_cpp_2008/rule_14_7_3"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := misra_cpp_2008_rule_14_7_3.Analyze(srcdir, opts)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	for _, result := range results.Results {
		result.ErrorMessage = "Template specialization shall be declared in the same file (1) as the primary template (2) as a user-defined type, for which the specialization is declared."
	}
	return results, nil
}
