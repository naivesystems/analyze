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

package rule_0_3_2

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_7"
)

func generateMisraResult(resultsOfMisra *pb.ResultsList) *pb.ResultsList {
	results := &pb.ResultsList{}
	for _, rs := range resultsOfMisra.Results {
		results.Results = append(results.Results, &pb.Result{
			Path:         rs.Path,
			LineNumber:   rs.LineNumber,
			ErrorMessage: "如果函数返回了错误信息，那么必须检测该错误信息",
			ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_0_3_2,
		})
	}
	return results
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	misraResults, err := dir_4_7.Analyze(srcdir, opts)
	if err != nil {
		return nil, err
	}
	results := generateMisraResult(misraResults)

	return results, nil
}
