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

package filter

import (
	"path/filepath"
	"testing"

	pb "naive.systems/analyzer/analyzer/proto"
)

func TestDeleteResultsWithCertainSuffixs(t *testing.T) {
	var testCase pb.ResultsList
	var suffixs []string
	suffixs = append(suffixs, ".c")
	notDeleteResult := pb.Result{Path: "notDeleteResult.cpp"}
	toDeleteResult := pb.Result{Path: "toDeleteResult.c"}
	testCase.Results = append(testCase.Results, &notDeleteResult)
	testCase.Results = append(testCase.Results, &toDeleteResult)

	t.Run("TestDeleteResultsWithCertainSuffixs", func(t *testing.T) {
		rule := DeleteResultsWithCertainSuffixs(&testCase, suffixs)
		suffix := make(map[string]struct{})
		for _, str := range suffixs {
			suffix[str] = struct{}{}
		}
		for _, rtn := range rule.Results {
			if _, ok := suffix[filepath.Ext(rtn.Path)]; ok {
				t.Errorf("found %v in returned ResultsLust, which is expected to be deleted", rtn.Path)
			}
		}
	})

}
