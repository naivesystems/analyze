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

package g1199

import (
	"fmt"
	"regexp"
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}
	ds, err := runner.RunClangSema(srcdir, "-Wint-conversion -Wimplicit-int-conversion -Wshorten-64-to-32 -Wimplicit-int-float-conversion -Wsign-conversion -Wconstant-conversion -Wfloat-conversion -Wimplicit-float-conversion", opts)
	if err != nil {
		return nil, fmt.Errorf("clangsema err: %v", err)
	}
	reg := regexp.MustCompile(`'[a-zA-Z0-9\s\_]+' to '[a-zA-Z0-9\s\_]+'|integer`)
	for _, d := range ds {
		match := reg.FindString(d.Message)
		if strings.Contains(match, "short") || strings.Contains(match, "int") || strings.Contains(match, "long") || strings.Contains(match, "char") {
			results.Results = append(results.Results, &pb.Result{
				Path:         d.Filename,
				LineNumber:   int32(d.Line),
				ErrorMessage: "Use care when converting integer types",
			})
		}
	}
	return runner.SortResult(runner.RemoveDup(results)), nil
}
