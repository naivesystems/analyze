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

package rule_17_3

import (
	"fmt"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/clangsema"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	if opts.JsonOption.Standard == "c99" || opts.JsonOption.Standard == "c11" {
		return &pb.ResultsList{}, nil
	}

	// Clangsema
	diagnostics, err := runner.RunClangSema(srcdir, "-Wimplicit-function-declaration", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	results := clangsema.CheckRule17_3(diagnostics)

	// Change the error message
	errMsg := "[C1506][misra-c2012-17.3]: 不得隐式声明函数"
	for _, result := range results.Results {
		result.ErrorMessage = errMsg
	}
	return results, nil
}
