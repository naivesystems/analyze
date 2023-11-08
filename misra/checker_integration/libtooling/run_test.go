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

package libtooling

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"
)

func TestLibtoolingSTU(t *testing.T) {
	args := []string{
		"--log-dir=/log",
		"-@@@",
		"--results-path=/output/results",
	}
	sourceFiles := []string{
		"sourcefile1.c",
		"sourcefile2.c",
		"sourcefile3.c",
	}
	expected := [][]string{
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			"sourcefile1.c",
		},
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			"sourcefile2.c",
		},
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			"sourcefile3.c",
		},
	}
	actual := generateSTURuns(args, sourceFiles)
	if !reflect.DeepEqual(expected, actual) {
		t.Errorf("unexpected stu results: %v, expected :%v", actual, expected)
	}
}

func TestLibtoolingCTU(t *testing.T) {
	dir, _ := os.Getwd()
	args := []string{
		"--log-dir=/log",
		"-@@@",
		"--results-path=/output/results",
	}
	sourceFiles := []string{
		"sourcefile1.c",
		"sourcefile2.c",
		"sourcefile3.c",
	}
	expected := [][]string{
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			filepath.Join(dir, "sourceFile"),
		},
	}
	actual := generateCTURuns(args, sourceFiles, dir)
	tmpFilePath := actual[0][len(actual[0])-1]
	defer os.Remove(tmpFilePath)
	if !reflect.DeepEqual(expected, actual) {
		t.Errorf("unexpected stu results: %v, expected :%v", actual, expected)
	}
}

func TestRunsAppend(t *testing.T) {
	dir, _ := os.Getwd()
	args := make([]string, 3, 5)
	args[0] = "--log-dir=/log"
	args[1] = "-@@@"
	args[2] = "--results-path=/output/results"

	sourceFiles := []string{
		"sourcefile1.c",
		"sourcefile2.c",
		"sourcefile3.c",
	}

	expectedCTU := [][]string{
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			filepath.Join(dir, "sourceFile"),
		},
	}

	expectedSTU := [][]string{
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			"sourcefile1.c",
		},
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			"sourcefile2.c",
		},
		{
			"--log-dir=/log",
			"-@@@",
			"--results-path=/output/results",
			"sourcefile3.c",
		},
	}

	actualCTURuns := generateCTURuns(args, sourceFiles, dir)
	tmpFilePath := actualCTURuns[0][len(actualCTURuns[0])-1]
	defer os.Remove(tmpFilePath)

	if !reflect.DeepEqual(expectedCTU, actualCTURuns) {
		t.Errorf("unexpected stu results: %v, expected :%v", actualCTURuns, expectedCTU)
	}

	actualSTURuns := generateSTURuns(args, sourceFiles)
	if !reflect.DeepEqual(expectedSTU, actualSTURuns) {
		t.Errorf("unexpected stu results: %v, expected :%v", actualSTURuns, expectedSTU)
	}
}

func TestSkipAssembly(t *testing.T) {
	sourceFiles := []string{
		"sourcefile1.c",
		"sourcefile2.s",
		"sourcefile3.c",
		"sourcefile4.S",
		"sourcefile5.c",
	}

	expected := []string{
		"sourcefile1.c",
		"sourcefile3.c",
		"sourcefile5.c",
	}

	actual := skipAssembly(sourceFiles)
	if !reflect.DeepEqual(expected, actual) {
		t.Errorf("unexpected skip results: %v, expected :%v", actual, expected)
	}
}
