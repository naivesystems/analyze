//go:build linux
// +build linux

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

package csa

import (
	"os"
	"reflect"
	"testing"

	"naive.systems/analyzer/misra/checker_integration/compilecommand"
	"naive.systems/analyzer/misra/utils"
)

var CPP_VERSION = utils.GetCurrentCppVersion()
var CLANG_VERSION = utils.GetCurrentClangVersion()

func TestGetCompilerIncludes(t *testing.T) {
	for _, testCase := range [...]struct {
		name                     string
		compiler                 string
		lang                     string
		analyzerOptions          []string
		expectedCompilerIncludes []string
	}{
		{
			name:                     "use clang++ to get includes",
			compiler:                 "clang++",
			lang:                     "c++",
			analyzerOptions:          []string{"-c"},
			expectedCompilerIncludes: []string{"/usr/include/c++/" + CPP_VERSION, "/usr/include/c++/" + CPP_VERSION + "/x86_64-redhat-linux", "/usr/include/c++/" + CPP_VERSION + "/backward", "/usr/lib64/clang/" + CLANG_VERSION + "/include", "/usr/local/include", "/usr/include"},
		},
		{
			name:                     "use cc to get includes",
			compiler:                 "/usr/bin/cc",
			lang:                     "c",
			analyzerOptions:          []string{"-c"},
			expectedCompilerIncludes: []string{"/usr/lib/gcc/x86_64-redhat-linux/" + CPP_VERSION + "/include", "/usr/local/include", "/usr/include"},
		},
		{
			name:                     "use gcc to get includes",
			compiler:                 "/usr/bin/gcc",
			lang:                     "c",
			analyzerOptions:          []string{"-c"},
			expectedCompilerIncludes: []string{"/usr/lib/gcc/x86_64-redhat-linux/" + CPP_VERSION + "/include", "/usr/local/include", "/usr/include"},
		},
		{
			name:                     "use gcc with multiple stds",
			compiler:                 "/usr/bin/gcc",
			lang:                     "c",
			analyzerOptions:          []string{"-c", "-std=gnu11", "-std=gnu89"},
			expectedCompilerIncludes: []string{"/usr/lib/gcc/x86_64-redhat-linux/" + CPP_VERSION + "/include", "/usr/local/include", "/usr/include"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult, err := getCompilerIncludes(testCase.compiler, testCase.lang, testCase.analyzerOptions)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if !reflect.DeepEqual(parsedResult, testCase.expectedCompilerIncludes) {
				t.Errorf("unexpected result for parse CompilerIncludes. parsed: %v. expected: %v.", parsedResult, testCase.expectedCompilerIncludes)
			}
		})
	}
}

func TestGetCompilerStandard(t *testing.T) {
	for _, testCase := range [...]struct {
		name                     string
		compiler                 string
		lang                     string
		expectedCompilerStandard string
	}{
		{
			name:                     "use clang++ to get standard",
			compiler:                 "clang++",
			lang:                     "c++",
			expectedCompilerStandard: "-std=gnu++14",
		},
		{
			name:                     "use cc to get standard",
			compiler:                 "/usr/bin/cc",
			lang:                     "c",
			expectedCompilerStandard: "-std=gnu17",
		},
		{
			name:                     "use gcc to get standard",
			compiler:                 "/usr/bin/gcc",
			lang:                     "c++",
			expectedCompilerStandard: "-std=gnu++17",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult, err := getCompilerStandard(testCase.compiler, testCase.lang)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if parsedResult != testCase.expectedCompilerStandard {
				t.Errorf("unexpected result for parse CompilerStandard. parsed: %v. expected: %v.", parsedResult, testCase.expectedCompilerStandard)
			}
		})
	}
}

func TestGetCompilerTarget(t *testing.T) {
	for _, testCase := range [...]struct {
		name                   string
		compiler               string
		expectedCompilerTarget string
	}{
		{
			name:                   "use clang++ to get target",
			compiler:               "clang++",
			expectedCompilerTarget: "x86_64-redhat-linux-gnu",
		},
		{
			name:                   "use cc to get target",
			compiler:               "/usr/bin/cc",
			expectedCompilerTarget: "x86_64-redhat-linux",
		},
		{
			name:                   "use gcc to get target",
			compiler:               "/usr/bin/gcc",
			expectedCompilerTarget: "x86_64-redhat-linux",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult, err := getCompilerTarget(testCase.compiler)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if parsedResult != testCase.expectedCompilerTarget {
				t.Errorf("unexpected result for parse CompilerTarget. parsed: %v. expected: %v.", parsedResult, testCase.expectedCompilerTarget)
			}
		})
	}
}

// Results of the test depend not only on systems but also on the environment,
// where folders in compilerIncludes may or may not include '*intrin.h'.
func TestContainsNoIntrinsicHeadersdFilter(t *testing.T) {
	for _, testCase := range [...]struct {
		name                     string
		compilerIncludes         []string
		expectedCompilerIncludes []string
	}{
		{
			name:                     "return right filtered includes",
			compilerIncludes:         []string{"/usr/lib/gcc/x86_64-redhat-linux/" + CPP_VERSION + "/include", "/usr/local/include", "/usr/include"},
			expectedCompilerIncludes: []string{"/usr/local/include", "/usr/include"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			filteredResult := filter(testCase.compilerIncludes, containsNoIntrinsicHeaders)
			if !reflect.DeepEqual(filteredResult, testCase.expectedCompilerIncludes) {
				t.Errorf("unexpected result for filter CompilerIncludes. filtered: %v. expected: %v.", filteredResult, testCase.expectedCompilerIncludes)
			}
		})
	}
}

// Results of the test depend not only on systems but also on the environment,
// where folders in include paths of analyzerOptions may or may not include '*intrin.h'.
func TestFilterOutIntrinOptions(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		analyzerOptions         []string
		expectedAnalyzerOptions []string
	}{
		{
			name:                    "return right filtered analyzer options",
			analyzerOptions:         []string{"-isystem/usr/lib/gcc/x86_64-redhat-linux/" + CPP_VERSION + "/include", "-iprefix/usr/local/include", "-iwithprefix/usr/include"},
			expectedAnalyzerOptions: []string{"-iprefix/usr/local/include", "-iwithprefix/usr/include"},
		},
		{
			name:                    "test seperate include options",
			analyzerOptions:         []string{"-I", "simple_test", "-isysroot", "simple_test/arch/x86/kvm"},
			expectedAnalyzerOptions: []string{"-I", "simple_test", "-isysroot", "simple_test/arch/x86/kvm"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			filterOutIntrinOptions(&testCase.analyzerOptions)
			if !reflect.DeepEqual(testCase.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected result for filter analyzerOptions. filtered: %v. expected: %v.", testCase.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestGetTripleArch(t *testing.T) {
	for _, testCase := range [...]struct {
		name           string
		compileCommand compilecommand.CompileCommand
		expectedArch   string
	}{
		{
			name: "simple case",
			compileCommand: compilecommand.CompileCommand{
				Command:   "clang -c main.cpp -o main.o",
				File:      "main.cpp",
				Directory: "./simple_test"},
			expectedArch: "x86_64",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			err := os.WriteFile("./simple_test/main.cpp", []byte("int main(void) { return (0);}"), 0644)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			action := ParseOptions(&testCase.compileCommand, config, false)
			parsedResult, err := getTripleArch(&action)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if parsedResult != testCase.expectedArch {
				t.Errorf("unexpected result for parse directory %v. parsed: %v. expected: %v.", testCase.compileCommand, parsedResult, testCase.expectedArch)
			}
		})
	}
}
