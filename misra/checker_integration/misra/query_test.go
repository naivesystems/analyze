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
package misra

import (
	"testing"
)

func TestSplitClangQueryResultsAndParseClangQueryResult(t *testing.T) {
	for _, tt := range [...]struct {
		name           string
		rawQueryResult string
		resultNumbers  int
		expectedDecls  []Decl
	}{
		{
			name: "single result",
			rawQueryResult: `0 matches.

			Match #1:

			folder1/folder2_/file_1.c:42:1: note: "root" binds here
			int func1(
			^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			1 match.
			0 matches.`,
			resultNumbers: 3,
			expectedDecls: []Decl{
				{},
				{
					File: "folder1/folder2_/file_1.c",
					Line: 42,
				},
				{},
			},
		},
		{
			name: "multiple results",
			rawQueryResult: `0 matches.
			0 matches.

			Match #1:

			folder/file1.c:1:1: note: "root" binds here
			int func1(
			^~~~~~~~~~
			1 match.
			0 matches.

			Match #1:

			folder/file2.c:2:1: note: "root" binds here
			void func2(
			^~~~~~~~~~~
			1 match.
			0 matches.
			`,
			resultNumbers: 6,
			expectedDecls: []Decl{
				{}, {},
				{
					File: "folder/file1.c",
					Line: 1,
				}, {}, {
					File: "folder/file2.c",
					Line: 2,
				},
				{},
			},
		},
	} {
		t.Run(tt.name, func(t *testing.T) {
			results := *SplitClangQueryResults(tt.rawQueryResult)
			if len(results) != tt.resultNumbers {
				t.Errorf("wrong result number")
			}
			for i, result := range results {
				decl, _ := ParseClangQueryResult(result)
				if decl != nil {
					if decl.File != tt.expectedDecls[i].File {
						t.Errorf("unexpected result, expected: %s parsed: %s", tt.expectedDecls[i].File, decl.File)
					}
					if decl.Line != tt.expectedDecls[i].Line {
						t.Errorf("unexpected result, expected: %d parsed: %d", tt.expectedDecls[i].Line, decl.Line)
					}
				}
				if decl == nil && len(tt.expectedDecls[i].File) != 0 {
					t.Errorf("failed to parse %s", result)
				}
			}
		})
	}

}
