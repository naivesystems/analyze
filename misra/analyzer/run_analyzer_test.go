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

package analyzer

import (
	"testing"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
)

func TestResolveCheckerBinariesPath(t *testing.T) {
	for _, testCase := range [...]struct {
		name              string
		binPath           string
		checkRules        []checkrule.CheckRule
		enableCodeChecker bool
		expectedErr       error
	}{
		{
			name:    "skip CodeChecker",
			binPath: "CodeChecker",
			checkRules: []checkrule.CheckRule{
				{
					Name:        "misra_c_2012/dir_4_14",
					JSONOptions: checkrule.JSONOption{},
				},
			},
			enableCodeChecker: false,
			expectedErr:       nil,
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			config := &pb.CheckerConfiguration{}
			config.InferBin = testCase.binPath
			config.ClangBin = testCase.binPath
			config.CodeCheckerBin = testCase.binPath
			config.CppcheckBin = testCase.binPath
			config.PythonBin = testCase.binPath
			config.ClangtidyBin = testCase.binPath
			config.ClangqueryBin = testCase.binPath
			config.ClangmappingBin = testCase.binPath
			err := resolveCheckerBinariesPath(testCase.checkRules, config, testCase.enableCodeChecker)
			if err != testCase.expectedErr {
				t.Errorf("unexpected error for resolving binary binaries. error: %v. expected: %v.", err, testCase.checkRules)
			}
		})
	}
}
