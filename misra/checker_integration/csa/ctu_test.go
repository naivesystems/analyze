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
	"encoding/json"
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"

	"gopkg.in/yaml.v2"

	"github.com/golang/glog"
	"golang.org/x/exp/slices"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/checker_integration/compilecommand"
	"naive.systems/analyzer/misra/utils"
)

var config *pb.CheckerConfiguration = &pb.CheckerConfiguration{}

var testDir = "./simple_test"

var srcFilePath = filepath.Join(testDir, "main.cpp")
var rspFilePath = filepath.Join(testDir, "main.rsp")

func setUp() error {
	// set up clang path in config
	clangBin, err := exec.LookPath("clang")
	if err != nil {
		return err
	}
	config.ClangBin = clangBin
	// set up clang-extdef-mapping bin in config
	config.ClangmappingBin = "../../../../out/bin/clang-extdef-mapping"
	// create simple case project dir
	err = os.MkdirAll(testDir, os.ModePerm)
	if err != nil {
		return err
	}
	return nil
}

func TestMain(m *testing.M) {
	err := setUp()
	if err != nil {
		glog.Errorf("test setUp: %v", err)
	}
	code := m.Run()
	// clear created dirs and files in testDir after tesing
	err = os.RemoveAll(testDir)
	if err != nil {
		glog.Errorf("test tearDown: %v", err)
	}
	os.Exit(code)
}

func TestParseGccPredefinedMacros(t *testing.T) {
	for _, testCase := range [...]struct {
		name                string
		gccPredefinedMacros string
		needExtraArg        bool
		expectedResult      []string
	}{
		{
			name:                "parse -DMARCO=VALUE",
			gccPredefinedMacros: "-DMARCO=VALUE",
			needExtraArg:        false,
			expectedResult:      []string{"-DMARCO=VALUE"},
		}, {
			name:                "parse -DMARCO=VALUE with needExtraArg",
			gccPredefinedMacros: "-DMARCO=VALUE",
			needExtraArg:        true,
			expectedResult:      []string{"--extra-arg", "-DMARCO=VALUE"},
		},
		{
			name:                "parse -D MARCO=VALUE",
			gccPredefinedMacros: "-D MARCO=VALUE",
			needExtraArg:        false,
			// Leave it as splitted format unless needExtraArg is True
			expectedResult: []string{"-D", "MARCO=VALUE"},
		},
		{
			name:                "parse -D MARCO=VALUE with needExtraArg",
			gccPredefinedMacros: "-D MARCO=VALUE",
			needExtraArg:        true,
			expectedResult:      []string{"--extra-arg", "-DMARCO=VALUE"},
		},
		{
			name:                "parse more than one -D",
			gccPredefinedMacros: "-D MARCO1=VALUE1 -DMARCO2=VALUE2 -D MARCO3=VALUE3",
			needExtraArg:        false,
			expectedResult:      []string{"-D", "MARCO1=VALUE1", "-DMARCO2=VALUE2", "-D", "MARCO3=VALUE3"},
		},
		{
			name:                "parse more than one -D with needExtraArg",
			gccPredefinedMacros: "-D MARCO1=VALUE1 -DMARCO2=VALUE2 -D MARCO3=VALUE3",
			needExtraArg:        true,
			expectedResult:      []string{"--extra-arg", "-DMARCO1=VALUE1", "--extra-arg", "-DMARCO2=VALUE2", "--extra-arg", "-DMARCO3=VALUE3"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			config.GccPredefinedMacros = testCase.gccPredefinedMacros
			parsedResult := ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), testCase.needExtraArg)
			if !reflect.DeepEqual(parsedResult, testCase.expectedResult) {
				t.Errorf("unexpected result for %v. parsed: %v. expected: %v.", testCase.gccPredefinedMacros, parsedResult, testCase.expectedResult)
			}
		})
	}

}

func TestGenerateCTUExtraArguments(t *testing.T) {
	for _, testCase := range [...]struct {
		name                string
		compileCommands     []compilecommand.CompileCommand
		compileCommandsPath string
		resultsDir          string
		expectedErr         error
	}{
		{
			name: "a simple case to test the workflow",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "clang++ -c foo.cpp -o foo.o",
				File:      "foo.cpp",
				Directory: "./simple_test"}},
			compileCommandsPath: "./simple_test/compile_commands.json",
			resultsDir:          "./simple_test/reports",
			expectedErr:         nil,
		},
		{
			name:                "empty struct of compile commands",
			compileCommands:     []compilecommand.CompileCommand{},
			compileCommandsPath: "./simple_test/compile_commands.json",
			resultsDir:          "./simple_test/reports",
			expectedErr:         errors.New("empty compile database"),
		},
		{
			name:                "empty file of compile commands",
			compileCommands:     nil,
			compileCommandsPath: "./simple_test/compile_commands.json",
			resultsDir:          "./simple_test/reports",
			expectedErr:         errors.New("compilecommand.ReadCompileCommandsFromFiles: unexpected end of JSON input"),
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			file := "./simple_test/foo.cpp"
			file_str :=
				`int foo() {
				return 0;
			}`
			err := os.WriteFile(file, []byte(file_str), os.ModePerm)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if testCase.compileCommands == nil {
				_, err := os.Create(testCase.compileCommandsPath)
				if err != nil {
					t.Errorf("unexpected error: %v", err)
				}
			} else {
				strCommands, err := json.Marshal(testCase.compileCommands)
				if err != nil {
					t.Errorf("unexpected error: %v", err)
				}
				err = os.WriteFile(testCase.compileCommandsPath, strCommands, os.ModePerm)
				if err != nil {
					t.Errorf("unexpected error: %v", err)
				}
			}
			parsedResult := GenerateCTUExtraArguments(testCase.compileCommandsPath, testCase.resultsDir, config, false, "zh", int32(runtime.NumCPU()))
			if parsedResult != nil {
				if parsedResult.Error() != testCase.expectedErr.Error() {
					t.Errorf("unexpected result for running the function to the end %v. parsed: %v. expected: %v.", testCase.compileCommandsPath, parsedResult, testCase.expectedErr)
				}
			} else if parsedResult != testCase.expectedErr {
				t.Errorf("unexpected result for running the function to the end %v. parsed: %v. expected: %v.", testCase.compileCommandsPath, parsedResult, testCase.expectedErr)
			}
		})
	}
}

func TestGetBuildActionsFromCompileCommands(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		compileCommands         []compilecommand.CompileCommand
		expectedDirectory       string
		expectedSource          string
		expectedAnalyzerOptions []string
	}{
		{
			name: "simple case",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "clang++ -c foo.cpp -o foo.o",
				File:      "foo.cpp",
				Directory: "./simple_test"}},
			expectedDirectory:       "./simple_test",
			expectedSource:          "foo.cpp",
			expectedAnalyzerOptions: []string{},
		},
		{
			name: "filtering flags for clang compiler",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "clang++ -c -fshort-wchar -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang -Wp,-MMD,arch/x86/kernel/cpu/.perfctr-watchdog.o.d foo.cpp -o foo.o",
				File:      "foo.cpp",
				Directory: "./simple_test"}},
			expectedDirectory:       "./simple_test",
			expectedSource:          "foo.cpp",
			expectedAnalyzerOptions: []string{"-fshort-wchar", "-enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang", "-Wp,-MMD,arch/x86/kernel/cpu/.perfctr-watchdog.o.d"},
		},
		{
			name: "filtering flags for gcc compiler",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "gcc -c -Wbad-function-cast scripts/sign-file -Wp,-MMD,scripts/.sign-file.d -o scripts/sign-file.c foo.cpp -o foo.o",
				File:      "foo.cpp",
				Directory: "./simple_test"}},
			expectedDirectory:       "./simple_test",
			expectedSource:          "foo.cpp",
			expectedAnalyzerOptions: []string{},
		},
		{
			// Flags with double quotes have been safely treated by shlex.Split.
			name: "flags with double quotes",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "clang++ -c -DKBUILD_MODNAME=\"string\" -DKBUILD_MODNAME=\"str ing\" '-DKBUILD_MODNAME=\"string\"' '-DKBUILD_MODNAME=\"str ing\"' foo.cpp -o foo.o",
				File:      "foo.cpp",
				Directory: "./simple_test"}},
			expectedDirectory:       "./simple_test",
			expectedSource:          "foo.cpp",
			expectedAnalyzerOptions: []string{"-DKBUILD_MODNAME=string", "-DKBUILD_MODNAME=str ing", "-DKBUILD_MODNAME=\"string\"", "-DKBUILD_MODNAME=\"str ing\""},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult := GetBuildActionsFromCompileCommands(testDir, &testCase.compileCommands, config, false, "zh")
			if (*parsedResult)[0].directory != testCase.expectedDirectory {
				t.Errorf("unexpected result for parse build actions from compile commands %v. parsed: %v. expected: %v.", testCase.compileCommands, (*parsedResult)[0].directory, testCase.expectedDirectory)
			} else if (*parsedResult)[0].source != testCase.expectedSource {
				t.Errorf("unexpected result for parse source from compile commands %v. parsed: %v. expected: %v.", testCase.compileCommands, (*parsedResult)[0].source, testCase.expectedSource)
			} else if !reflect.DeepEqual((*parsedResult)[0].analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected result for parse analyze options from compile commands %v. parsed: %v. expected: %v.", testCase.compileCommands, (*parsedResult)[0].analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestSortBuildActionsFromCompileCommands(t *testing.T) {
	for _, testCase := range [...]struct {
		name                  string
		compileCommands       []compilecommand.CompileCommand
		expectedDirectory     string
		expectedSource        string
		expectedSortedActions []string
	}{
		{
			name: "sort commands by source file name",
			compileCommands: []compilecommand.CompileCommand{
				{
					Command:   "clang++ -c foo.cpp -o foo.o",
					File:      "foo.cpp",
					Directory: "./simple_test"},
				{
					Command:   "clang++ -c aoo.cpp -o aoo.o",
					File:      "aoo.cpp",
					Directory: "./simple_test"},
			},
			expectedSortedActions: []string{"aoo.cpp", "foo.cpp"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			file := "./simple_test/aoo.cpp"
			file_str :=
				`int aoo() {
			return 0;
			}`
			err := os.WriteFile(file, []byte(file_str), os.ModePerm)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			parsedResult := GetBuildActionsFromCompileCommands(testDir, &testCase.compileCommands, config, false, "zh")
			var sortedActions []string
			for _, action := range *parsedResult {
				sortedActions = append(sortedActions, action.source)
			}
			if !reflect.DeepEqual(sortedActions, testCase.expectedSortedActions) {
				t.Errorf("unexpected result for sort compile commands. parsed: %v. expected: %v.", sortedActions, testCase.expectedSortedActions)
			}
		})
	}
}

func TestParseOptions(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		compileCommand          compilecommand.CompileCommand
		expectedCompiler        string
		expectedDirectory       string
		expectedSource          string
		expectedLanguage        string
		expectedActionType      ActionType
		expectedOutput          string
		expectedAnalyzerOptions []string
	}{
		{
			name: "simple case",
			compileCommand: compilecommand.CompileCommand{
				Command:   "clang++ -c foo.cpp -o foo.o",
				File:      "foo.cpp",
				Directory: "./simple_test"},
			expectedDirectory:       "./simple_test",
			expectedCompiler:        "clang++",
			expectedSource:          "foo.cpp",
			expectedLanguage:        "c++",
			expectedActionType:      COMPILE,
			expectedOutput:          "foo.o",
			expectedAnalyzerOptions: []string{},
		},
		{
			name: "test include options",
			compileCommand: compilecommand.CompileCommand{
				Command:   "clang++ -c foo.cpp -o foo.o -I . -isysroot ./arch/x86/kvm",
				File:      "foo.cpp",
				Directory: "./simple_test"},
			expectedDirectory:       "./simple_test",
			expectedCompiler:        "clang++",
			expectedSource:          "foo.cpp",
			expectedLanguage:        "c++",
			expectedActionType:      COMPILE,
			expectedOutput:          "foo.o",
			expectedAnalyzerOptions: []string{"-I", "simple_test", "-isysroot", "simple_test/arch/x86/kvm"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult := ParseOptions(&testCase.compileCommand, config, false)
			if parsedResult.directory != testCase.expectedDirectory {
				t.Errorf("unexpected result for parse directory %v. parsed: %v. expected: %v.", testCase.compileCommand, parsedResult.directory, testCase.expectedDirectory)
			} else if parsedResult.compiler != testCase.expectedCompiler {
				t.Errorf("unexpected result for parse compiler %v. parsed: %v. expected: %v.", testCase.expectedCompiler, parsedResult.compiler, testCase.expectedCompiler)
			} else if parsedResult.source != testCase.expectedSource {
				t.Errorf("unexpected result for parse source %v. parsed: %v. expected: %v.", testCase.expectedSource, parsedResult.source, testCase.expectedSource)
			} else if parsedResult.lang != testCase.expectedLanguage {
				t.Errorf("unexpected result for parse lang %v. parsed: %v. expected: %v.", testCase.compileCommand, parsedResult.lang, testCase.expectedLanguage)
			} else if parsedResult.actionType != testCase.expectedActionType {
				t.Errorf("unexpected result for parse actionType %v. parsed: %v. expected: %v.", testCase.compileCommand, parsedResult.actionType, testCase.expectedActionType)
			} else if parsedResult.output != testCase.expectedOutput {
				t.Errorf("unexpected result for parse output %v. parsed: %v. expected: %v.", testCase.compileCommand, parsedResult.output, testCase.expectedOutput)
			} else if !reflect.DeepEqual(parsedResult.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected result for parse analyzerOptions %v. parsed: %v. expected: %v.", testCase.compileCommand, parsedResult.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestSkipSource(t *testing.T) {
	for _, testCase := range [...]struct {
		name          string
		arguments     []string
		expectedFlags []bool
	}{
		{
			name: "return right flags",
			arguments: []string{
				"-v",
				"-c",
				"/usr/share/cmake/Modules/CMakeCCompilerABI.c"},
			expectedFlags: []bool{false, false, true},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := skipSources(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
		})
	}
}

func TestGetLanguage(t *testing.T) {
	for _, testCase := range [...]struct {
		name          string
		arguments     []string
		expectedFlags []bool
		expectLang    string
	}{
		{
			name: "set lang and return right flags",
			arguments: []string{
				"-v",
				"-x c++",
				"-xc++"},
			expectedFlags: []bool{false, true, true},
			expectLang:    "c++",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := getLanguage(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if details.lang != testCase.expectLang {
				t.Errorf("unexpected lang for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.lang, testCase.expectLang)
			}
		})
	}
}

func TestDetermineActionType(t *testing.T) {
	for _, testCase := range [...]struct {
		name               string
		arguments          []string
		expectedFlags      []bool
		expectedActionType ActionType
	}{
		{
			name: "set action type and return right flags",
			arguments: []string{
				"-c",
				"-print-prog-name",
				"-E",
				"-v"},
			expectedFlags:      []bool{true, true, true, false},
			expectedActionType: COMPILE,
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := determineActionType(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if details.actionType != testCase.expectedActionType {
				t.Errorf("unexpected action type for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.actionType, testCase.expectedActionType)
			}
		})
	}
}

func TestGetArch(t *testing.T) {
	for _, testCase := range [...]struct {
		name          string
		arguments     []string
		expectedFlags []bool
		expectedArch  string
	}{
		{
			name: "set arch and return right flags",
			arguments: []string{
				"-arch",
				"x86_64",
				"-xc++"},
			expectedFlags: []bool{true, false, false},
			expectedArch:  "x86_64",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := getArch(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if details.arch != testCase.expectedArch {
				t.Errorf("unexpected arch for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.arch, testCase.expectedArch)
			}
		})
	}
}

func TestGetOutput(t *testing.T) {
	for _, testCase := range [...]struct {
		name           string
		arguments      []string
		expectedFlags  []bool
		expectedOutput string
	}{
		{
			name: "set output and return right flags",
			arguments: []string{
				"-arch",
				"-o",
				"foo.cpp"},
			expectedFlags:  []bool{false, true, false},
			expectedOutput: "foo.cpp",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := getOutput(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if details.output != testCase.expectedOutput {
				t.Errorf("unexpected output for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.output, testCase.expectedOutput)
			}
		})
	}
}

func TestSkipClang(t *testing.T) {
	for _, testCase := range [...]struct {
		name          string
		arguments     []string
		expectedFlags []bool
	}{
		{
			name: "return right flags",
			arguments: []string{
				"-Werror",
				"-pedantic-errors",
				"-w",
				"foo.cpp"},
			expectedFlags: []bool{true, true, true, false},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := skipClang(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
		})
	}
}

func TestSkipGCC(t *testing.T) {
	for _, testCase := range [...]struct {
		name          string
		arguments     []string
		expectedFlags []bool
	}{
		{
			name: "return right flags",
			arguments: []string{
				"-bundle_loader",
				"-multiply_defined",
				"-sectorder",
				"-v"},
			expectedFlags: []bool{true, true, false, false},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := skipGcc(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
		})
	}
}

func TestCollectTransformXclangOpts(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		arguments               []string
		expectedFlags           []bool
		expectedAnalyzerOptions []string
	}{
		{
			name: "set analyzer options and return right flags",
			arguments: []string{
				"-Xclang",
				"-multiply_defined",
				"-Xclang",
				"-v"},
			expectedFlags: []bool{true, false, true, false},
			expectedAnalyzerOptions: []string{
				"-Xclang",
				"-multiply_defined",
				"-Xclang",
				"-v"},
		},
		{
			name: "skip XCLANG_FLAGS_TO_SKIP flags",
			arguments: []string{
				"-Xclang",
				"-v",
				"-module-file-info",
				"-S",
				"-Xclang",
				"-emit-llvm",
				"-rewrite-objc"},
			expectedFlags: []bool{true, false, false, false, true, false, false},
			expectedAnalyzerOptions: []string{"-Xclang",
				"-v"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := collectTransformXclangOpts(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if !reflect.DeepEqual(details.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected analyzer options for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestCollectTransformIncludeOpts(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		arguments               []string
		expectedFlags           []bool
		expectedAnalyzerOptions []string
	}{
		{
			name: "set analyzer options and return right flags",
			arguments: []string{
				"-isysroot=/user/bin",
				"-sdkroot/user/bin",
				"--include=/user/bin",
				"-include/user/bin",
				"-iquote=/user/bin",
				"-isystem/user/bin",
				"-Xclang",
				"-v",
				"-Isrc",
				"-I/src",
				"-I.",
				"-I../cmd"},
			expectedFlags: []bool{true, true, true, true, true, true, false, false, true, true, true, true},
			expectedAnalyzerOptions: []string{
				"-isysroot",
				"/user/bin",
				"-sdkroot/user/bin",
				"--include=/user/bin",
				"-include/user/bin",
				"-iquote=/user/bin",
				"-isystem/user/bin",
				"-I/bld/src",
				"-I/src",
				"-I/bld",
				"-I/cmd"},
		},
		{
			name: "test seperate include options",
			arguments: []string{
				"-I",
				"/src/arch/x86/kvm"},
			expectedFlags: []bool{true, false},
			expectedAnalyzerOptions: []string{
				"-I",
				"/src/arch/x86/kvm"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			details.directory = "/bld"
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := collectTransformIncludeOpts(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if !reflect.DeepEqual(details.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected analyzer options for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestCollectClangCompileOpts(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		arguments               []string
		expectedFlags           []bool
		expectedAnalyzerOptions []string
	}{
		{
			name: "set analyzer options and return right flags",
			arguments: []string{
				"-v",
				"-c",
				"foo.cpp",
				"-sdkroot",
				"/usr/share/cmake/Modules/CMakeCCompilerABI.c",
				"-include",
			},
			expectedFlags: []bool{true, true, true, true, true, true},
			expectedAnalyzerOptions: []string{
				"-v",
				"-c",
				"foo.cpp",
				"-sdkroot",
				"/usr/share/cmake/Modules/CMakeCCompilerABI.c",
				"-include",
			},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := collectClangCompileOpts(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if !reflect.DeepEqual(details.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected analyzer options for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestReplace(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		arguments               []string
		expectedFlags           []bool
		expectedAnalyzerOptions []string
	}{
		{
			name: "set analyzer options and return right flags",
			arguments: []string{
				"-v",
				"-mips32",
				"-sdkroot",
				"-mpowerpc",
			},
			expectedFlags:           []bool{false, true, false, true},
			expectedAnalyzerOptions: []string{"-target mips -mips32", "-target powerpc"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := replace(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if !reflect.DeepEqual(details.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected analyzer options for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestCollectCompileOpts(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		arguments               []string
		expectedFlags           []bool
		expectedAnalyzerOptions []string
	}{
		{
			name: "return right flags",
			arguments: []string{
				"-v",
				"--gcc-toolchain=",
				"-sdkroot",
				"--sysroot=",
			},
			expectedFlags: []bool{false, true, true, true},
			expectedAnalyzerOptions: []string{"--gcc-toolchain=",
				"-sdkroot",
				"--sysroot="},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := collectCompileOpts(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if !reflect.DeepEqual(details.analyzerOptions, testCase.expectedAnalyzerOptions) {
				t.Errorf("unexpected analyzer options for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.analyzerOptions, testCase.expectedAnalyzerOptions)
			}
		})
	}
}

func TestGetTarget(t *testing.T) {
	for _, testCase := range [...]struct {
		name                   string
		arguments              []string
		expectedFlags          []bool
		expectedCompilerTarget string
	}{
		{
			name: "set target and return right flags",
			arguments: []string{
				"-v",
				"--target",
				"x86_64-redhat-linux",
				"--sysroot=",
			},
			expectedFlags:          []bool{false, true, false, false},
			expectedCompilerTarget: "x86_64-redhat-linux",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			details := csaBuildAction{}
			resultFlags := []bool{}
			for idx := 0; idx < len(testCase.arguments); idx++ {
				flag := getTarget(testCase.arguments[idx:], &details)
				resultFlags = append(resultFlags, flag)
			}
			if !reflect.DeepEqual(resultFlags, testCase.expectedFlags) {
				t.Errorf("unexpected result for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, resultFlags, testCase.expectedFlags)
			}
			if details.compilerTarget != testCase.expectedCompilerTarget {
				t.Errorf("unexpected compiler target for flag processing %v. parsed: %v. expected: %v.", testCase.arguments, details.compilerTarget, testCase.expectedCompilerTarget)
			}
		})
	}
}

func TestGccToolchainInArgs(t *testing.T) {
	for _, testCase := range [...]struct {
		name                 string
		analyzerOptions      []string
		expectedGccToolchain string
	}{
		{
			name:                 "simple case",
			analyzerOptions:      []string{"-c"},
			expectedGccToolchain: "",
		},
		{
			name: "complex case",
			analyzerOptions: []string{
				"-v",
				"-c",
				"-o",
				"--gcc-toolchain=CMakeFiles/cmTC_29d52.dir/CMakeCCompilerABI.c.o",
				"/usr/share/cmake/Modules/CMakeCCompilerABI.c"},
			expectedGccToolchain: "--gcc-toolchain=CMakeFiles/cmTC_29d52.dir/CMakeCCompilerABI.c.o",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult := gccToolchainInArgs(testCase.analyzerOptions)
			if !reflect.DeepEqual(parsedResult, testCase.expectedGccToolchain) {
				t.Errorf("unexpected result for parse gccToolchain %v. parsed: %v. expected: %v.", testCase.analyzerOptions, parsedResult, testCase.expectedGccToolchain)
			}
		})
	}
}

func TestGetCommandStdout(t *testing.T) {
	for _, testCase := range [...]struct {
		name       string
		commands   []string
		workingDir string
		label      string
	}{
		{
			name:       "get stdout for clang++ target",
			commands:   []string{"clang++", "-v"},
			workingDir: "",
			label:      "Target:",
		},
		{
			name:       "get stdout for cc target",
			commands:   []string{"/usr/bin/cc", "-v"},
			workingDir: "",
			label:      "Target:",
		},
		{
			name:       "get stdout for gcc target",
			commands:   []string{"/usr/bin/gcc", "-v"},
			workingDir: "",
			label:      "Target:",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult, err := utils.GetCommandStdout(testCase.commands, testCase.workingDir)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if !slices.Contains(parsedResult, testCase.label) {
				t.Errorf("unexpected stdout for parse includes. parsed: %v.", parsedResult)
			}
		})
	}
}

func TestIsNotIncludeFixedFilter(t *testing.T) {
	for _, testCase := range [...]struct {
		name                     string
		compilerIncludes         []string
		expectedCompilerIncludes []string
	}{
		{
			name:                     "return right filtered includes",
			compilerIncludes:         []string{"/usr/include/c++/11", "/usr/include/c++/11/x86_64-redhat-linux/include-fixed", "/usr/include/c++/11/backward/include-fixed", "/usr/lib64/clang/13/include/include-fixed", "/usr/local/include", "/usr/include"},
			expectedCompilerIncludes: []string{"/usr/include/c++/11", "/usr/local/include", "/usr/include"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			filteredResult := filter(testCase.compilerIncludes, isNotIncludeFixed)
			if !reflect.DeepEqual(filteredResult, testCase.expectedCompilerIncludes) {
				t.Errorf("unexpected result for filter CompilerIncludes. filtered: %v. expected: %v.", filteredResult, testCase.expectedCompilerIncludes)
			}
		})
	}
}

// Refer to codechecker/analyzer/tests/unit/test_log_parser.py: test_compiler_extra_args_filter_empty,
// test_compiler_implicit_include_flags, and test_compiler_implicit_include_flags_sysroot.
func TestFilterCompilerIncludesExtraArgs(t *testing.T) {
	for _, testCase := range [...]struct {
		name                     string
		compilerIncludes         []string
		expectedCompilerIncludes []string
	}{
		{
			name:                     "filter empty",
			compilerIncludes:         []string{},
			expectedCompilerIncludes: []string{},
		},
		{
			// Specific stdlib, build architecture related flags should be kept.
			name:                     "implicit include flags",
			compilerIncludes:         []string{"-I", "/usr/include", "-m64", "-stdlib=libc++", "-std=c++17"},
			expectedCompilerIncludes: []string{"-m64", "-stdlib=libc++", "-std=c++17"},
		},
		{
			// Sysroot flags should be kept.
			name:                     "implicit include flags sysroot",
			compilerIncludes:         []string{"-I", "/usr/include", "--sysroot=/usr/mysysroot"},
			expectedCompilerIncludes: []string{"--sysroot=/usr/mysysroot"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			filteredResult := filterCompilerIncludesExtraArgs(testCase.compilerIncludes)
			if !reflect.DeepEqual(filteredResult, testCase.expectedCompilerIncludes) {
				t.Errorf("unexpected result for filter CompilerIncludes. filtered: %v. expected: %v.", filteredResult, testCase.expectedCompilerIncludes)
			}
		})
	}
}

func TestGetCompileCommand(t *testing.T) {
	for _, testCase := range [...]struct {
		name             string
		analyzerOptions  []string
		compilerIncludes []string
		compilerStandard string
		compilerTarget   string
		directory        string
		lang             string
		output           string
		source           string
		expectedCommand  []string
	}{
		{
			name:             "return parsed compile command",
			compilerTarget:   "x86_64-redhat-linux",
			compilerStandard: "-std=gnu17",
			lang:             "c",
			output:           "main.o",
			source:           "foo.c",
			analyzerOptions: []string{
				"-v",
				"-c",
				"-sdkroot",
				"/usr/share/cmake/Modules/CMakeCCompilerABI.c",
			},
			compilerIncludes: []string{"/usr/include/c++/11", "/usr/local/include", "/usr/include"},
			expectedCommand:  []string{"/usr/bin/clang", "--target=x86_64-redhat-linux", "-isystem", "/usr/include/c++/11", "-isystem", "/usr/local/include", "-isystem", "/usr/include", "-c", "-x", "c", "-v", "-c", "-sdkroot", "/usr/share/cmake/Modules/CMakeCCompilerABI.c", "foo.c", "-std=gnu17"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			action := csaBuildAction{}
			action.compilerTarget = testCase.compilerTarget
			action.compilerIncludes = testCase.compilerIncludes
			action.lang = testCase.lang
			action.analyzerOptions = testCase.analyzerOptions
			action.output = testCase.output
			action.source = testCase.source
			action.compilerStandard = testCase.compilerStandard
			parsedCommand, err := getCompileCommand(&action)
			if err != nil {
				t.Errorf("get compile command failed: %s", action)
			}
			// Path of clang is different in systems other than linux.
			if strings.Contains(parsedCommand[0], "clang") && !reflect.DeepEqual(parsedCommand[1:], testCase.expectedCommand[1:]) {
				t.Errorf("unexpected result for parse command. parsed: %v. expected: %v.", parsedCommand, testCase.expectedCommand)
			}
		})
	}
}

func TestGetExtdefMappingCmd(t *testing.T) {
	for _, testCase := range [...]struct {
		name             string
		analyzerOptions  []string
		compilerIncludes []string
		compilerStandard string
		compilerTarget   string
		directory        string
		lang             string
		output           string
		source           string
		expectedCommand  []string
	}{
		{
			name:             "return parsed extdef-mapping command",
			compilerTarget:   "x86_64-redhat-linux",
			compilerStandard: "-std=gnu17",
			lang:             "c",
			output:           "main.o",
			source:           "foo.c",
			analyzerOptions: []string{
				"-v",
				"-c",
				"-sdkroot",
				"/usr/share/cmake/Modules/CMakeCCompilerABI.c",
			},
			compilerIncludes: []string{"/usr/include/c++/11", "/usr/local/include", "/usr/include"},
			expectedCommand:  []string{"/usr/bin/clang-extdef-mapping", "foo.c", "--", "--target=x86_64-redhat-linux", "-isystem", "/usr/include/c++/11", "-isystem", "/usr/local/include", "-isystem", "/usr/include", "-c", "-x", "c", "-v", "-c", "-sdkroot", "/usr/share/cmake/Modules/CMakeCCompilerABI.c", "foo.c", "-std=gnu17"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			action := csaBuildAction{}
			action.compilerTarget = testCase.compilerTarget
			action.compilerIncludes = testCase.compilerIncludes
			action.lang = testCase.lang
			action.analyzerOptions = testCase.analyzerOptions
			action.output = testCase.output
			action.source = testCase.source
			action.compilerStandard = testCase.compilerStandard
			parsedCommand, err := getExtdefMappingCmd(&action, config.ClangmappingBin)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			// Path of clang-extdef-mapping is different in systems other than linux.
			if strings.Contains(parsedCommand.Args[0], "clang-extdef-mapping") && !reflect.DeepEqual(parsedCommand.Args[1:], testCase.expectedCommand[1:]) {
				t.Errorf("unexpected result for parse command. parsed: %v. expected: %v.", parsedCommand, testCase.expectedCommand)
			}
		})
	}
}

func TestGetCommandStdoutLines(t *testing.T) {
	for _, testCase := range [...]struct {
		name          string
		commands      []string
		workingDir    string
		expectedLines []string
	}{
		{
			name:          "get stdout lines for echo",
			commands:      []string{"echo", "testing whether it is able to\nget stdout lines\nor not"},
			workingDir:    "",
			expectedLines: []string{"testing whether it is able to", "get stdout lines", "or not"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			cmd := exec.Command(testCase.commands[0], testCase.commands[1:]...)
			parsedResult, err := utils.GetCommandStdoutLines(cmd, testCase.workingDir)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if !reflect.DeepEqual(parsedResult, testCase.expectedLines) {
				t.Errorf("unexpected stdout lines. parsed: %v. expected: %v.", parsedResult, testCase.expectedLines)
			}
		})
	}
}

func TestFuncMapListSrcToAst(t *testing.T) {
	for _, testCase := range [...]struct {
		name                string
		funcSrcList         []string
		expectedFuncAstList map[string][]string
	}{
		{
			name:        "simple case",
			funcSrcList: []string{"c:@F@pop_stream /src/y.tab.c", "c:@F@find_reserved_word /src/y.tab.c"},
			expectedFuncAstList: map[string][]string{
				"c:@F@pop_stream":         {"/src/y.tab.c"},
				"c:@F@find_reserved_word": {"/src/y.tab.c"}},
		},
		{
			name:        "the first is digit to split",
			funcSrcList: []string{"15:c:@F@pop stream /src/y.tab.c", "23:c:@F@find reserved word /src/y.tab.c"},
			expectedFuncAstList: map[string][]string{
				"15:c:@F@pop stream":         {"/src/y.tab.c"},
				"23:c:@F@find reserved word": {"/src/y.tab.c"}},
		},
		{
			name:        "duplicated key",
			funcSrcList: []string{"c:@F@pop_stream /src/y.tab.c", "c:@F@pop_stream /src/variables.c"},
			expectedFuncAstList: map[string][]string{
				"c:@F@pop_stream": {"/src/y.tab.c", "/src/variables.c"}},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			parsedResult, err := funcMapListSrcToAst(testCase.funcSrcList)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			if !reflect.DeepEqual(parsedResult, testCase.expectedFuncAstList) {
				t.Errorf("unexpected stdout lines. parsed: %v. expected: %v.", parsedResult, testCase.expectedFuncAstList)
			}
		})
	}
}

// Refer to codechecker/analyzer/tests/unit/test_log_parser.py: test_response_file_simple,
// test_response_file_contains_source_file, test_response_file_contains_multiple_source_files,
// and test_source_file_contains_at_sign.
func TestExtendCompilationDatabaseEntries(t *testing.T) {
	for _, testCase := range [...]struct {
		name                string
		compileCommands     []compilecommand.CompileCommand
		responseFileContent []byte
		expectedCommand     string
	}{
		{
			// Test simple response file where the source file comes outside the response file.
			name: "simple case",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "g++ " + srcFilePath + " @" + rspFilePath,
				File:      srcFilePath,
				Directory: testDir}},
			responseFileContent: []byte("-DVARIABLE=some value"),
			expectedCommand:     "g++ " + srcFilePath + " -DVARIABLE=some value",
		},
		{
			// Test response file where the source file comes from the response file.
			name: "source file comes from response file",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "g++ " + srcFilePath + " @" + rspFilePath,
				File:      srcFilePath,
				Directory: testDir}},
			responseFileContent: []byte("-DVARIABLE=some value " + srcFilePath),
			expectedCommand:     "g++ " + srcFilePath + " -DVARIABLE=some value " + srcFilePath,
		},
		{
			// Test source file which path contains '@' sign in path.
			name: "source path contains at sign",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "g++ " + srcFilePath + " @" + rspFilePath,
				File:      srcFilePath,
				Directory: testDir}},
			responseFileContent: []byte("-DVARIABLE=some value " + filepath.Join(testDir+"@", "main.cpp")),
			expectedCommand:     "g++ " + srcFilePath + " -DVARIABLE=some value " + filepath.Join(testDir+"@", "main.cpp"),
		},
		{
			// Test response file where multiple source files come from the response file.
			name: "multiple source files from response file",
			compileCommands: []compilecommand.CompileCommand{{
				Command:   "g++ " + srcFilePath + " @" + rspFilePath,
				File:      srcFilePath,
				Directory: testDir}},
			responseFileContent: []byte("-DVARIABLE=some value " + filepath.Join(testDir, "a.cpp") + " " + filepath.Join(testDir, "b.cpp")),
			expectedCommand:     "g++ " + srcFilePath + " -DVARIABLE=some value " + filepath.Join(testDir, "a.cpp") + " " + filepath.Join(testDir, "b.cpp"),
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			err := os.WriteFile(rspFilePath, testCase.responseFileContent, 0644)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			parsedResult := extendCompilationDatabaseEntries(&testCase.compileCommands)
			if parsedResult[0].Command != testCase.expectedCommand {
				t.Errorf("unexpected result for parse command containing response file. parsed: %v. expected: %v.", parsedResult[0].Command, testCase.expectedCommand)
			}
		})
	}
}

func TestProcessResponseFile(t *testing.T) {
	for _, testCase := range [...]struct {
		name                string
		responseFileContent []byte
		expectedOptions     []string
		expectedSources     []string
	}{
		{
			// Test simple response file where the source file comes outside the response file.
			name:                "simple case",
			responseFileContent: []byte("-DVARIABLE=some value"),
			expectedOptions:     []string{"-DVARIABLE=some", "value"},
			expectedSources:     []string{},
		},
		{
			// Test response file where the source file comes from the response file.
			name:                "source file comes from response file",
			responseFileContent: []byte("-DVARIABLE=some value " + srcFilePath),
			expectedOptions:     []string{"-DVARIABLE=some", "value", srcFilePath},
			expectedSources:     []string{srcFilePath},
		},
		{
			// Test source file which path contains '@' sign in path.
			name:                "source path contains at sign",
			responseFileContent: []byte("-DVARIABLE=some value " + filepath.Join(testDir+"@", "main.cpp")),
			expectedOptions:     []string{"-DVARIABLE=some", "value", filepath.Join(testDir+"@", "main.cpp")},
			expectedSources:     []string{filepath.Join(testDir+"@", "main.cpp")},
		},
		{
			// Test response file where multiple source files come from the response file.
			name:                "multiple source files from response file",
			responseFileContent: []byte("-DVARIABLE=some value " + filepath.Join(testDir, "a.cpp") + " " + filepath.Join(testDir, "b.cpp")),
			expectedOptions:     []string{"-DVARIABLE=some", "value", filepath.Join(testDir, "a.cpp"), filepath.Join(testDir, "b.cpp")},
			expectedSources:     []string{filepath.Join(testDir, "a.cpp"), filepath.Join(testDir, "b.cpp")},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			err := os.WriteFile(rspFilePath, testCase.responseFileContent, 0644)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			parsedOptions, parsedSources := processResponseFile(rspFilePath)
			if !reflect.DeepEqual(parsedOptions, testCase.expectedOptions) {
				t.Errorf("unexpected options for processing response file. parsed: %v. expected: %v.", parsedOptions, testCase.expectedOptions)
			}
			if !reflect.DeepEqual(parsedSources, testCase.expectedSources) {
				t.Errorf("unexpected sources for processing response file. parsed: %v. expected: %v.", parsedSources, testCase.expectedSources)
			}
		})
	}
}

func TestGenerateInvocationList(t *testing.T) {
	for _, testCase := range [...]struct {
		name                    string
		analyzerOptions         []string
		compilerIncludes        []string
		compilerStandard        string
		compilerTarget          string
		directory               string
		lang                    string
		output                  string
		source                  string
		expectedInvocationLines []string
	}{
		{
			name:             "simple case",
			compilerTarget:   "x86_64-redhat-linux",
			compilerStandard: "-std=gnu17",
			lang:             "c",
			output:           "main.o",
			source:           "foo.c",
			analyzerOptions: []string{
				"-c",
			},
			compilerIncludes:        []string{"/usr/include"},
			expectedInvocationLines: []string{"/usr/bin/clang", "--target=x86_64-redhat-linux", "-isystem", "/usr/include", "-c", "-x", "c", "-c", "foo.c", "-std=gnu17", "-D__clang_analyzer__", "-w", "-D", "MARCO1=VALUE1", "-DMARCO2=VALUE2", "-D", "MARCO3=VALUE3"},
		},
		{
			name:                    "options with double quotes",
			compilerTarget:          "x86_64-redhat-linux",
			compilerStandard:        "-std=gnu17",
			lang:                    "c",
			output:                  "main.o",
			source:                  "foo.c",
			analyzerOptions:         []string{"-c", "-DKBUILD_MODNAME=\"string\""},
			compilerIncludes:        []string{"/usr/include"},
			expectedInvocationLines: []string{"/usr/bin/clang", "--target=x86_64-redhat-linux", "-isystem", "/usr/include", "-c", "-x", "c", "-c", "-DKBUILD_MODNAME=\"string\"", "foo.c", "-std=gnu17", "-D__clang_analyzer__", "-w", "-D", "MARCO1=VALUE1", "-DMARCO2=VALUE2", "-D", "MARCO3=VALUE3"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			action := csaBuildAction{}
			action.compilerTarget = testCase.compilerTarget
			action.compilerIncludes = testCase.compilerIncludes
			action.lang = testCase.lang
			action.analyzerOptions = testCase.analyzerOptions
			action.output = testCase.output
			action.source = testCase.source
			action.compilerStandard = testCase.compilerStandard
			action.arch = "x86_64"
			ctuDir := filepath.Join(testDir, "ctu-dir")
			err := generateInvocationList(&action, ctuDir, config)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			tripleArchDir := filepath.Join(ctuDir, action.arch)
			invocationList := filepath.Join(tripleArchDir, "invocation-list.yml")
			yamlFile, err := os.ReadFile(invocationList)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			type YamlList struct {
				YamlLine []string `yaml:"foo.c"`
			}
			var parsedList YamlList
			err = yaml.Unmarshal(yamlFile, &parsedList)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			// The attached ParseGccPredefinedMacros may depend on the input config.
			if !reflect.DeepEqual(parsedList.YamlLine, testCase.expectedInvocationLines) {
				t.Errorf("unexpected options for processing response file. parsed: %v. expected: %v.", parsedList.YamlLine, testCase.expectedInvocationLines)
			}
		})
	}
}

func TestCreateGlobalCTUFunctionMap(t *testing.T) {
	for _, testCase := range [...]struct {
		name                        string
		ctuDir                      string
		mergedFuncAstListByArch     map[string]map[string][]string
		expectedGlobalFuncMapByArch map[string]string
	}{
		{
			name:                    "simple case",
			ctuDir:                  "simple_test/ctu-dir",
			mergedFuncAstListByArch: map[string]map[string][]string{"x86_64_1": map[string][]string{"c:@F@pop_stream": []string{"/src/y.tab.c"}}},
			expectedGlobalFuncMapByArch: map[string]string{
				"x86_64_1": "c:@F@pop_stream /src/y.tab.c\n"},
		},
		{
			name:                    "duplicated key",
			ctuDir:                  "simple_test/ctu-dir",
			mergedFuncAstListByArch: map[string]map[string][]string{"x86_64_2": map[string][]string{"c:@F@pop_stream": []string{"/src/y.tab.c", "/src/arch/x86/realmode/rm/wakemain.c"}, "c:@F@main": []string{"/src/arch/x86/realmode/rm/wakemain.c"}}},
			expectedGlobalFuncMapByArch: map[string]string{
				"x86_64_2": "c:@F@main /src/arch/x86/realmode/rm/wakemain.c\n"},
		},
		{
			name:                    "different arch",
			ctuDir:                  "simple_test/ctu-dir",
			mergedFuncAstListByArch: map[string]map[string][]string{"x86_64_3": map[string][]string{"c:@F@pop_stream": []string{"/src/y.tab.c"}}, "x86_64_4": map[string][]string{"c:@F@main": []string{"/src/arch/x86/realmode/rm/wakemain.c"}}},
			expectedGlobalFuncMapByArch: map[string]string{
				"x86_64_3": "c:@F@pop_stream /src/y.tab.c\n",
				"x86_64_4": "c:@F@main /src/arch/x86/realmode/rm/wakemain.c\n"},
		},
		{
			name:                    "different arch with a same key",
			ctuDir:                  "simple_test/ctu-dir",
			mergedFuncAstListByArch: map[string]map[string][]string{"x86_64_5": map[string][]string{"c:@F@main": []string{"/src/y.tab.c"}}, "x86_64_6": map[string][]string{"c:@F@main": []string{"/src/arch/x86/realmode/rm/wakemain.c"}}},
			expectedGlobalFuncMapByArch: map[string]string{
				"x86_64_5": "c:@F@main /src/y.tab.c\n",
				"x86_64_6": "c:@F@main /src/arch/x86/realmode/rm/wakemain.c\n"},
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			err := createGlobalCTUFunctionMap(testCase.ctuDir, &testCase.mergedFuncAstListByArch)
			if err != nil {
				t.Errorf("unexpected error: %v", err)
			}
			for tripleArch, expectedGlobalFuncMap := range testCase.expectedGlobalFuncMapByArch {
				readGlobalFuncMap, err := os.ReadFile(filepath.Join(testCase.ctuDir, tripleArch, "externalDefMap.txt"))
				if err != nil {
					t.Errorf("unexpected error: %v", err)
				}
				if !reflect.DeepEqual(string(readGlobalFuncMap), expectedGlobalFuncMap) {
					t.Errorf("unexpected global function mapping by arch %s. parsed: %v. expected: %v.", tripleArch, string(readGlobalFuncMap), expectedGlobalFuncMap)
				}
			}
		})
	}
}
