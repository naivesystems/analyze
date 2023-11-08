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

package rule_6_4_4

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_2"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := rule_16_2.Analyze(srcdir, opts)
	if err != nil {
		return nil, err
	}
	for _, result := range results.Results {
		result.ErrorMessage = "switch标记只得出现在构成switch语句主体的复合语句的最外层"
		result.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_6_4_4
	}
	return results, nil
}
