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

package testlib

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/golang/glog"
	"naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cpumem"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/utils"
)

var checkingStandards = []string{
	// BEGIN-INTERNAL
	"gjb5369",
	"gjb8114",
	"cwe",
	"clang-tidy",
	"cppcheck",
	// END-INTERNAL
	"misra",
	"misra_cpp_2008",
	"autosar",
	"googlecpp",
	"toy_rules",
}

func getCheckerConfig(projectBaseDir string) *proto.CheckerConfiguration {
	checkerConfig := proto.CheckerConfiguration{
		InferBin:          filepath.Join(projectBaseDir, "out", "bin", "infer"),
		ClangBin:          filepath.Join(projectBaseDir, "bazel-bin", "external", "llvm-project", "clang", "clang"),
		CodeCheckerBin:    "CodeChecker",
		CppcheckBin:       filepath.Join(projectBaseDir, "third_party", "cppcheck", "cppcheck"),
		PythonBin:         "python3",
		ClangtidyBin:      filepath.Join(projectBaseDir, "bazel-bin", "external", "llvm-project", "clang-tools-extra", "clang-tidy"),
		ClangqueryBin:     "clang-query",
		MisraCheckerPath:  filepath.Join(projectBaseDir, "bazel-bin", "misra"),
		InferExtraOptions: "--max-nesting=3 --bo-field-depth-limit=6",
		ClangmappingBin:   filepath.Join(projectBaseDir, "out", "bin", "clang-extdef-mapping"),
		InferJobs:         8,
		CpplintScript:     filepath.Join(projectBaseDir, "third_party", "github.com", "cpplint", "cpplint", "cpplint.py"),
	}

	return &checkerConfig
}

var ignoreDirPatterns = analyzerinterface.ArrayFlags{}

func GetProjectBaseDir() string {
	_, filename, _, ok := runtime.Caller(0)
	if !ok {
		glog.Fatal("can not get caller information")
	}
	projectBase := filepath.Join(filepath.Dir(filename), "..", "..")

	return projectBase
}

func CreateTestCompilationDatabse(srcdir string) error {
	projType := analyzerinterface.SelectBuilder(srcdir)
	var err error
	cc_exist := true

	switch projType {
	case analyzerinterface.Make:
		err, cc_exist = analyzerinterface.CreateCompilationDatabaseByMake(options.GetCompileCommandsPath(srcdir), srcdir)
	case analyzerinterface.CMake:
		err, cc_exist = analyzerinterface.CreateCompilationDatabaseByCMake(
			options.GetCompileCommandsPath(srcdir),
			srcdir,
			/*isDev=*/ true,
		)
	case analyzerinterface.Other:
		err = fmt.Errorf("no viable builder found in %s. Stop", srcdir)
		cc_exist = false
	}
	if cc_exist {
		if err != nil {
			glog.Errorf("compile failed but generate ccjson success, err: %v", err)
		}
		return nil
	}
	return err
}

func NewOption(srcdir string, edition string, ignoreCpp bool, standard string) (*options.CheckOptions, error) {
	projectBase := GetProjectBaseDir()
	checkerConfig := getCheckerConfig(projectBase)
	outputPath := filepath.Join(srcdir, "output")
	logDir := ""
	numWorkers := int32(runtime.NumCPU())
	cpumem.Init(int(numWorkers), 0)

	checkerPathsMap := map[string]string{}
	for _, edition := range checkingStandards {
		checkerPathsMap[edition] = filepath.Join(projectBase, "bazel-bin", edition)
	}

	jsonOptions := &checkrule.JSONOption{}
	// read json options from ${srcdir}/options.json if the file exists
	jsonOptionsPath := filepath.Join(srcdir, "options.json")
	jsonOptionsContent, err := os.ReadFile(jsonOptionsPath)
	if os.IsNotExist(err) {
		jsonOptions.Standard = standard
	} else if err != nil {
		return nil, err
	} else {
		err = json.Unmarshal(jsonOptionsContent, &jsonOptions)
		if err != nil {
			return nil, err
		}
	}

	standardSet := make(map[string]bool)
	standardSet[jsonOptions.Standard] = true

	envOptions := options.NewEnvOptions(
		standardSet,
		outputPath,
		srcdir,
		logDir,
		checkerPathsMap,
		checkerConfig,
		ignoreDirPatterns,
		/*checkProgress=*/ true,
		/*enableCodeChecker=*/ false,
		ignoreCpp,
		/*debug=*/ true,
		/*limitMemory=*/ false,
		0.9,
		numWorkers,
		/*isDev=*/ true,
		90,
		30,
		"zh",
		/*onlyCppcheck=*/ false,
		/*disableParallelismInChecker=*/ false,
	)
	if envOptions == nil {
		glog.Fatal("EnvOption is nil, please check your compile_commands.json")
	}

	ruleOptions := options.NewRuleSpecificOptions("test_run", outputPath)

	checkOptions := options.MakeCheckOptions(jsonOptions, envOptions, ruleOptions)

	return &checkOptions, nil
}

func selectCheckingStandard() string {
	_, filename, _, ok := runtime.Caller(3)
	if !ok {
		glog.Fatal("can not get caller information")
	}
	for _, e := range checkingStandards {
		if strings.Contains(filename, e) {
			return e
		}
	}
	// default gjb edition: 5369
	return checkingStandards[0]
}

func MakeTestOption(srcdir string) (*options.CheckOptions, error) {
	return MakeTestOptionRealAdvance(srcdir, "c99")
}

// add this func to make sure every call to selectCheckingStandard() has 3 call layers
func MakeTestOptionAdvance(srcdir string, standard string) (*options.CheckOptions, error) {
	return MakeTestOptionRealAdvance(srcdir, standard)
}

func MakeTestOptionRealAdvance(srcdir string, standard string) (*options.CheckOptions, error) {
	err := CreateTestCompilationDatabse(srcdir)
	if err != nil {
		return nil, err
	}
	edition := selectCheckingStandard()
	ignoreCpp := false
	if edition == "gjb5369" {
		ignoreCpp = true
	}
	return NewOption(srcdir, edition, ignoreCpp, standard)
}

func ToTestResult(results *proto.ResultsList, err error) (*proto.ResultsList, error) {
	if err == nil && results != nil {
		for _, result := range results.Results {
			result.ErrorKind = 0
			result.ExternalMessage = ""
			result.Locations = nil
			result.Severity = 0
		}
	}

	return results, err
}

func ToRelPath(srcdir string, results *proto.ResultsList) error {
	for _, result := range results.Results {
		if rel, err := filepath.Rel(srcdir, result.Path); err != nil {
			return err
		} else {
			result.Path = rel
		}
	}
	return nil
}

func ConcatClangSystemHeader(options *string) {
	clangVersion := utils.GetCurrentClangVersion()
	*options = fmt.Sprintf("-isystem /usr/lib64/clang/%s/include %s", clangVersion, *options)
}

func ConcatGCCSystemHeader(options *string) {
	cppVersion := utils.GetCurrentCppVersion()
	*options = fmt.Sprintf("-isystem /usr/include/c++/%s -isystem /usr/include/c++/%s/x86_64-redhat-linux %s", cppVersion, cppVersion, *options)
}

func AddSystemHeader(options *options.CheckOptions) {
	if runtime.GOOS == "darwin" {
		// don't need this in macos
		return
	}
	ConcatGCCSystemHeader(&options.EnvOption.CheckerConfig.CsaSystemLibOptions)
	ConcatClangSystemHeader(&options.EnvOption.CheckerConfig.CsaSystemLibOptions)
}
