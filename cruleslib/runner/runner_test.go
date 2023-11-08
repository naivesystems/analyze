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

package runner

import (
	"testing"

	pb "naive.systems/analyzer/analyzer/proto"
)

func TestModifyResult(t *testing.T) {
	for _, testCase := range [...]struct {
		name                 string
		resultRule           string
		errorMessage         string
		expectedErrorMessage string
	}{
		{
			name:                 "misra cpp",
			resultRule:           "misra_cpp_2008/rule_2_10_2",
			errorMessage:         "在内部作用域声明的标识符不得隐藏在外部作用域声明的标识符",
			expectedErrorMessage: "[CXX5651][misra-cpp2008-2.10.2]: 在内部作用域声明的标识符不得隐藏在外部作用域声明的标识符",
		},
		{
			name:                 "gjb5369",
			resultRule:           "gjb5369/rule_1_1_19",
			errorMessage:         "array size should be specified in declaration",
			expectedErrorMessage: "[C3752][gjb-5369-1.1.19]: array size should be specified in declaration",
		},
		{
			name:                 "gjb8114 R",
			resultRule:           "gjb8114/rule_1_6_4",
			errorMessage:         "bit operation should not apply to logical expression",
			expectedErrorMessage: "[C3938][gjb-8114-1.6.4]: bit operation should not apply to logical expression",
		},
		{
			name:                 "gjb8114 A",
			resultRule:           "gjb8114/rule_A_1_13_2",
			errorMessage:         "bit operation should not apply to logical expression",
			expectedErrorMessage: "[C4036][gjb-8114-A.1.13.2]: bit operation should not apply to logical expression",
		},
		{
			name:                 "no available issue code",
			resultRule:           "gjb8114/rule_2_9_2",
			errorMessage:         "bit operation should not apply to logical expression",
			expectedErrorMessage: "[-][gjb-8114-2.9.2]: bit operation should not apply to logical expression",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			analyzerResult := &analyzerResult{}
			analyzerResult.rule = testCase.resultRule
			analyzerResult.resultsList = &pb.ResultsList{}
			results := &pb.Result{}
			results.ErrorMessage = testCase.errorMessage
			analyzerResult.resultsList.Results = []*pb.Result{results}
			modifyResult(analyzerResult)
			if analyzerResult.resultsList.Results[0].ErrorMessage != testCase.expectedErrorMessage {
				t.Errorf("unexpected error message for %v. parsed: %v. expected: %v.", analyzerResult.rule, analyzerResult.resultsList.Results[0].ErrorMessage, testCase.expectedErrorMessage)
			}
		})
	}

}
