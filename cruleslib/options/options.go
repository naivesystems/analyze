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

package options

import (
	"os"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/analyzer"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

type CheckOptions struct {
	JsonOption         checkrule.JSONOption // TODO: remove
	EnvOption          EnvOptions
	RuleSpecificOption RuleSpecificOptions
}

type EnvOptions struct {
	ResultsDir                  string
	BuildActions                *[]csa.BuildAction
	CheckerConfig               *pb.CheckerConfiguration
	IgnoreDirPatterns           analyzerinterface.ArrayFlags
	CheckProgress               bool
	EnableCodeChecker           bool
	DumpErrors                  map[string]string
	Debug                       bool
	AvailMemRatio               float64
	LimitMemory                 bool
	NumWorkers                  int32
	IsDev                       bool
	TimeoutNormal               int
	TimeoutOom                  int
	Lang                        string
	OnlyCppcheck                bool
	DisableParallelismInChecker bool

	LogDir string

	CheckerPathsMap map[string]string
}

type RuleSpecificOptions struct {
	RuleSpecificResultDir    string
	LibtoolingResultDir      string
	CSAResultDir             string
	InferResultDir           string
	CppcheckResultDir        string
	FixedCmdArgsStringOption []string
}

func NewRuleSpecificOptions(ruleName string, generalResultsDir string) *RuleSpecificOptions {
	options := &RuleSpecificOptions{}

	rulset, rule, found := strings.Cut(ruleName, "/")
	if !found {
		rule = ruleName
	}
	tmpResultsDir := filepath.Join(generalResultsDir, "tmp", rulset)
	err := os.MkdirAll(tmpResultsDir, os.ModePerm)
	if err != nil {
		glog.Fatalf("failed to create tmp dir: %v", err)
	}
	resultsDir, err := os.MkdirTemp(tmpResultsDir, rule+"-*")
	if err != nil {
		glog.Fatalf("failed to create result dir: %v", err)
	}
	// TODO: manage individual output dir here
	options.RuleSpecificResultDir = resultsDir
	options.CSAResultDir = resultsDir
	options.InferResultDir = resultsDir
	options.LibtoolingResultDir = resultsDir
	options.CppcheckResultDir = resultsDir

	options.FixedCmdArgsStringOption = []string{}

	return options
}

func NewEnvOptions(
	standardSet map[string]bool,
	resultsDir string, srcdir string,
	logDir string,
	checkerPathsMap map[string]string,
	checkerConfig *pb.CheckerConfiguration,
	ignoreDirPatterns analyzerinterface.ArrayFlags,
	checkProgress bool,
	enableCodeChecker bool,
	ignoreCpp bool,
	debug bool,
	limitMemory bool,
	availMemRatio float64,
	numWorkers int32,
	isDev bool,
	timeoutNormal int,
	timeoutOom int,
	lang string,
	onlyCppcheck bool,
	disableParallelismInChecker bool,
) *EnvOptions {
	envOptions := &EnvOptions{}
	envOptions.ResultsDir = resultsDir
	envOptions.BuildActions = &[]csa.BuildAction{}
	envOptions.CheckerConfig = checkerConfig
	envOptions.CheckProgress = checkProgress
	envOptions.EnableCodeChecker = enableCodeChecker
	envOptions.IgnoreDirPatterns = ignoreDirPatterns
	envOptions.Debug = debug
	envOptions.CheckerPathsMap = checkerPathsMap
	envOptions.LogDir = logDir
	envOptions.LimitMemory = limitMemory
	envOptions.AvailMemRatio = availMemRatio
	envOptions.NumWorkers = numWorkers
	envOptions.IsDev = isDev
	envOptions.TimeoutNormal = timeoutNormal
	envOptions.TimeoutOom = timeoutOom
	envOptions.Lang = lang
	envOptions.OnlyCppcheck = onlyCppcheck
	envOptions.DisableParallelismInChecker = disableParallelismInChecker

	compileCommandsPath := GetCompileCommandsPath(srcdir)
	if !isDev {
		// TODO: there still some duplicated action which remove ignore files twice.
		actualCompileCommandsPath, err := analyzerinterface.GenerateActualCCJsonByRemoveIgnoreFiles(compileCommandsPath, ignoreDirPatterns)
		if err != nil {
			glog.Fatalf("analyzerinterface.GenerateActualCCJsonByRemoveIgnoreFiles(: %v", err)
		}
		compileCommandsPath = actualCompileCommandsPath
	}

	var err error
	envOptions.BuildActions, err = csa.ParseCompileCommands(
		compileCommandsPath,
		ignoreDirPatterns,
		envOptions.CheckerConfig,
		ignoreCpp,
		onlyCppcheck,
		isDev,
	)

	if err != nil {
		glog.Fatalf("csa.ParseCompileCommands: %v", err)
	}

	if len(*envOptions.BuildActions) == 0 {
		return nil
	}

	// TODO: fill actually used checkers
	usedCheckers := map[checker_integration.Checker]bool{
		checker_integration.Cppcheck_STU: true,
		checker_integration.Cppcheck_CTU: true,
		checker_integration.CSA:          true,
	}

	dumpError, err := analyzer.PreAnalyzeBody(
		compileCommandsPath,
		envOptions.BuildActions,
		usedCheckers,
		standardSet,
		envOptions.ResultsDir,
		envOptions.CheckerConfig,
		envOptions.CheckProgress,
		envOptions.EnableCodeChecker,
		envOptions.NumWorkers,
		envOptions.IgnoreDirPatterns,
		envOptions.Lang,
		envOptions.OnlyCppcheck,
	)
	if err != nil {
		glog.Fatalf("failed to generateDumpError: %v", err)
	}
	envOptions.DumpErrors = dumpError

	return envOptions
}

// TODO: deprecate checkerConfig after rule transfer
func NewEnvOptionsFromShared(
	standardSet map[string]bool,
	logDir string,
	checkerPathsMap map[string]string,
	sharedOptions *SharedOptions,
	checkerConfig *pb.CheckerConfiguration,
	ignoreCpp bool,
	numWorkers int32,
	onlyCppcheck bool,
) *EnvOptions {
	return NewEnvOptions(
		standardSet,
		sharedOptions.GetResultsDir(),
		sharedOptions.GetSrcDir(),
		logDir,
		checkerPathsMap,
		checkerConfig,
		sharedOptions.GetIgnoreDirPatterns(),
		sharedOptions.GetCheckProgress(),
		sharedOptions.GetEnableCodeChecker(),
		ignoreCpp,
		sharedOptions.GetDebugMode(),
		sharedOptions.GetLimitMemory(),
		sharedOptions.GetAvailMemRatio(),
		numWorkers,
		/*isDev=*/ false,
		sharedOptions.GetTimeoutNormal(),
		sharedOptions.GetTimeoutOom(),
		sharedOptions.GetLang(),
		onlyCppcheck,
		sharedOptions.GetDisableParallelismInChecker(),
	)
}

func MakeCheckOptions(jsonOption *checkrule.JSONOption, envOption *EnvOptions, ruleOption *RuleSpecificOptions) CheckOptions {
	return CheckOptions{
		JsonOption:         *jsonOption,
		EnvOption:          *envOption,
		RuleSpecificOption: *ruleOption,
	}
}

func GetCompileCommandsPath(srcdir string) string {
	return filepath.Join(srcdir, analyzerinterface.CCJson)
}

func GetActualCompileCommandsPath(srcdir string) string {
	return filepath.Join(srcdir, analyzerinterface.ActualCCJson)
}
