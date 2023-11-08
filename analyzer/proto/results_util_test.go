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

package proto

import (
	"testing"
)

func TestResultsSet(t *testing.T) {
	set := NewResultsSet()
	set.Add(&Result{
		Path:         "file_a",
		LineNumber:   2,
		ErrorMessage: "error_a",
	})
	set.Add(&Result{
		Path:         "file_a",
		LineNumber:   2,
		ErrorMessage: "error_a",
	})
	set.Add(&Result{
		Path:         "file_a",
		LineNumber:   2,
		ErrorMessage: "error_b",
	})
	if len(set.Results) != 2 {
		t.Fatalf("ResultsSet is not a set, expect size: 2, actual: %d", len(set.Results))
	}
}

func TestResultsSetFromList(t *testing.T) {
	list := &ResultsList{Results: []*Result{
		{Path: "file_a", LineNumber: 2, ErrorMessage: "error_a"},
		{Path: "file_a", LineNumber: 2, ErrorMessage: "error_a"},
		{Path: "file_a", LineNumber: 2, ErrorMessage: "error_b"},
	}}
	set := NewResultsSetFromList(list)
	if len(set.Results) != 2 {
		t.Fatalf("ResultsSetFromList is not a set, expect size: 2, actual: %d", len(set.Results))
	}
}
