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

package analyzerinterface

import (
	"errors"
	"testing"
)

func TestMatchIgnoreDirPatterns(t *testing.T) {
	for _, testCase := range [...]struct {
		name           string
		ignorePatterns []string
		filePath       string
		expectedResult bool
		expectedErr    error
	}{
		{
			name:           "match file in the same folder",
			ignorePatterns: []string{"/src/test/**/*"},
			filePath:       "/src/test/test1.c",
			expectedResult: true,
			expectedErr:    nil,
		},
		{
			name:           "match file in the recursive folder",
			ignorePatterns: []string{"/src/test/**/*"},
			filePath:       "/src/test/test1.c",
			expectedResult: true,
			expectedErr:    nil,
		},
		{
			name:           "no matched file",
			ignorePatterns: []string{"/src/core/**/*"},
			filePath:       "/src/test/test1.c",
			expectedResult: false,
			expectedErr:    nil,
		},
		{
			name:           "invalid pattern",
			ignorePatterns: []string{"/src/[**/"},
			filePath:       "/src/test/test1.c",
			expectedResult: false,
			expectedErr:    errors.New("malformed ignore_dir pattern /src/[**/"),
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			matched, err := MatchIgnoreDirPatterns(testCase.ignorePatterns, testCase.filePath)
			if err != nil || testCase.expectedErr != nil {
				if err.Error() != testCase.expectedErr.Error() {
					t.Errorf("unexpected result for test %v. error: %v. expected: %v.", testCase.name, err, testCase.expectedErr)
				}
			} else if matched != testCase.expectedResult {
				t.Errorf("unexpected result for test %v. result: %v. expected: %v.", testCase.name, matched, testCase.expectedResult)
			}
		})
	}
}
