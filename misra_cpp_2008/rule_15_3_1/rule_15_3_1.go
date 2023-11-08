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

package rule_15_3_1

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}
	ds, err := runner.RunGCC(opts)
	if err != nil {
		return results, fmt.Errorf("gcc executing err: %v", err)
	}
	for _, d := range ds {
		if d.Option == "-Wterminate" && d.Message == "‘throw’ will always call ‘terminate’" {
			results.Results = append(results.Results, &pb.Result{
				Path:         d.Locations[0].Caret.File,
				LineNumber:   int32(d.Locations[0].Caret.Line),
				ErrorMessage: "只有在程序启动后和程序终止前才应引发异常",
				ErrorKind:    pb.Result_MISRA_CPP_2008_RULE_15_3_1,
			})
		}
	}
	return results, nil
}
