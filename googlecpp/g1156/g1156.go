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

package g1156

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

var style = `{
	Language: Cpp,
	IncludeBlocks: Regroup,
	IncludeCategories: [
		{
			Regex: "^<ext/.*\\.h>",
			Priority: 2,
			SortPriority: 0,
			CaseSensitive: false,
		},
		{
			Regex: "^<.*\\.h>",
			Priority: 1,
			SortPriority: 0,
			CaseSensitive: false,
		},
		{
			Regex: "^<.*",
			Priority: 2,
			SortPriority: 0,
			CaseSensitive: false,
		},
		{
			Regex: ".*",
			Priority: 3,
			SortPriority: 0,
			CaseSensitive: false,
		}
	],
	SortIncludes: CaseSensitive,
}`

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	compileCommandsPath := runner.GetCompileCommandsPath(srcdir, opts)
	reports, err := runner.RunClangFormat(srcdir, style, compileCommandsPath, opts)
	if err != nil {
		return nil, err
	}
	type Loc struct {
		string
		int32
	}
	results := make(map[Loc]struct{})
	resultsList := &pb.ResultsList{}
	report := func(path string, line int32) {
		loc := Loc{path, line}
		if _, reported := results[loc]; !reported {
			results[loc] = struct{}{}
			resultsList.Results = append(resultsList.Results, &pb.Result{
				Path:         path,
				LineNumber:   line,
				ErrorMessage: "Include headers in the following order: Related header, C system headers, C++ standard library headers, other libraries' headers, your project's headers.",
			})
		}
	}
	for _, r := range reports {
		report(r.Filename, int32(r.LineNumber))
	}
	return resultsList, nil
}
