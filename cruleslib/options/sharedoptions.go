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
	"flag"

	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
)

type SharedOptions struct {
	AddLineHash                 *bool
	AllowBuildFail              *bool
	AvailMemRatio               *float64
	CheckProgress               *bool
	CheckerConfig               *string
	CheckerPathRoot             *string
	ClangBin                    *string
	ClangmappingBin             *string
	ClangqueryBin               *string
	ClangtidyBin                *string
	CmakeCXXCompiler            *string
	CmakeExportCompileCommands  *bool
	CodeCheckerBin              *string
	ConfigDir                   *string
	CppcheckBin                 *string
	CpplintScript               *string
	CsaSystemLibOptions         *string
	DebugMode                   *bool
	EnableCodeChecker           *bool
	GccPredefinedMacros         *string
	IgnoreDirPatterns           analyzerinterface.ArrayFlags
	IncludeStddef               *string
	InferBin                    *string
	InferExtraOptions           *string
	InferJobs                   *int64
	KeepBuildActionPercent      *float64
	Lang                        *string
	LimitMemory                 *bool
	MisraCheckerPath            *string
	ProjectName                 *string
	ProjectType                 *string
	PythonBin                   *string
	QmakeBin                    *string
	QtProPath                   *string
	ResultsDir                  *string
	ScriptContents              *string
	ShowJsonResults             *bool
	ShowLineNumber              *bool
	ShowResults                 *bool
	ShowResultsCount            *bool
	SkipBearMake                *bool
	SrcDir                      *string
	TimeoutNormal               *int
	TimeoutOom                  *int
	DisableParallelismInChecker *bool
}

func (s SharedOptions) GetAddLineHash() bool {
	return *s.AddLineHash
}

func (s SharedOptions) GetAllowBuildFail() bool {
	return *s.AllowBuildFail
}

func (s SharedOptions) GetAvailMemRatio() float64 {
	return *s.AvailMemRatio
}

func (s SharedOptions) GetCheckProgress() bool {
	return *s.CheckProgress
}

func (s SharedOptions) GetCheckerConfig() string {
	return *s.CheckerConfig
}

func (s SharedOptions) GetCheckerPathRoot() string {
	return *s.CheckerPathRoot
}

func (s SharedOptions) GetClangBin() string {
	return *s.ClangBin
}

func (s SharedOptions) GetClangmappingBin() string {
	return *s.ClangmappingBin
}

func (s SharedOptions) GetClangqueryBin() string {
	return *s.ClangqueryBin
}

func (s SharedOptions) GetClangtidyBin() string {
	return *s.ClangtidyBin
}

func (s SharedOptions) GetCmakeCXXCompiler() string {
	return *s.CmakeCXXCompiler
}

func (s SharedOptions) GetCmakeExportCompileCommands() bool {
	return *s.CmakeExportCompileCommands
}

func (s SharedOptions) GetCodeCheckerBin() string {
	return *s.CodeCheckerBin
}

func (s SharedOptions) GetConfigDir() string {
	return *s.ConfigDir
}

func (s SharedOptions) GetCppcheckBin() string {
	return *s.CppcheckBin
}

func (s SharedOptions) GetCpplintScript() string {
	return *s.CpplintScript
}

func (s SharedOptions) GetCsaSystemLibOptions() string {
	return *s.CsaSystemLibOptions
}

func (s SharedOptions) GetDebugMode() bool {
	return *s.DebugMode
}

func (s SharedOptions) GetEnableCodeChecker() bool {
	return *s.EnableCodeChecker
}

func (s SharedOptions) GetGccPredefinedMacros() string {
	return *s.GccPredefinedMacros
}

func (s SharedOptions) GetIgnoreDirPatterns() analyzerinterface.ArrayFlags {
	return s.IgnoreDirPatterns
}

func (s SharedOptions) GetIncludeStddef() string {
	return *s.IncludeStddef
}

func (s SharedOptions) GetInferBin() string {
	return *s.InferBin
}

func (s SharedOptions) GetInferExtraOptions() string {
	return *s.InferExtraOptions
}

func (s SharedOptions) GetInferJobs() int64 {
	return *s.InferJobs
}

func (s SharedOptions) GetLang() string {
	return *s.Lang
}

func (s SharedOptions) GetKeepBuildActionPercent() float64 {
	return *s.KeepBuildActionPercent
}

func (s SharedOptions) GetLimitMemory() bool {
	return *s.LimitMemory
}

func (s SharedOptions) GetMisraCheckerPath() string {
	return *s.MisraCheckerPath
}

func (s SharedOptions) GetProjectName() string {
	return *s.ProjectName
}

func (s SharedOptions) GetProjectType() string {
	return *s.ProjectType
}

func (s SharedOptions) GetPythonBin() string {
	return *s.PythonBin
}

func (s SharedOptions) GetQmakeBin() string {
	return *s.QmakeBin
}

func (s SharedOptions) GetQtProPath() string {
	return *s.QtProPath
}

func (s SharedOptions) GetResultsDir() string {
	return *s.ResultsDir
}

func (s SharedOptions) GetScriptContents() string {
	return *s.ScriptContents
}

func (s SharedOptions) GetShowJsonResults() bool {
	return *s.ShowJsonResults
}

func (s SharedOptions) GetShowLineNumber() bool {
	return *s.ShowLineNumber
}

func (s SharedOptions) GetShowResults() bool {
	return *s.ShowResults
}

func (s SharedOptions) GetShowResultsCount() bool {
	return *s.ShowResultsCount
}

func (s SharedOptions) GetSkipBearMake() bool {
	return *s.SkipBearMake
}

func (s SharedOptions) GetSrcDir() string {
	return *s.SrcDir
}

func (s SharedOptions) GetTimeoutNormal() int {
	return *s.TimeoutNormal
}

func (s SharedOptions) GetTimeoutOom() int {
	return *s.TimeoutOom
}

func (s SharedOptions) GetDisableParallelismInChecker() bool {
	return *s.DisableParallelismInChecker
}

func (s SharedOptions) SetCsaSystemLibOptions(options string) {
	*s.CsaSystemLibOptions = options
}

func (s SharedOptions) SetGccPredefinedMacros(macro string) {
	*s.GccPredefinedMacros = macro
}

func (s SharedOptions) SetLang(lang string) {
	*s.Lang = lang
}

func (s SharedOptions) SetLimitMemory(limit bool) {
	*s.LimitMemory = limit
}

func (s SharedOptions) SetSrcDir(srcdir string) {
	*s.SrcDir = srcdir
}

type DefaultOptionValues struct {
	AddLineHash                 bool
	AllowBuildFail              bool
	AvailMemRatio               float64
	CheckProgress               bool
	CheckerConfig               string
	CheckerPathRoot             string
	ClangBin                    string
	ClangmappingBin             string
	ClangqueryBin               string
	ClangtidyBin                string
	CmakeCXXCompiler            string
	CmakeExportCompileCommands  bool
	CodeCheckerBin              string
	ConfigDir                   string
	CppcheckBin                 string
	CpplintScript               string
	CsaSystemLibOptions         string
	DebugMode                   bool
	EnableCodeChecker           bool
	GccPredefinedMacros         string
	IgnoreDirPatterns           analyzerinterface.ArrayFlags
	IncludeStddef               string
	InferBin                    string
	InferExtraOptions           string
	InferJobs                   int64
	KeepBuildActionPercent      float64
	Lang                        string
	LimitMemory                 bool
	MisraCheckerPath            string
	ProjectName                 string
	ProjectType                 string
	PythonBin                   string
	QmakeBin                    string
	QtProPath                   string
	ResultsDir                  string
	ScriptContents              string
	ShowJsonResults             bool
	ShowLineNumber              bool
	ShowResults                 bool
	ShowResultsCount            bool
	SkipBearMake                bool
	SrcDir                      string
	TimeoutNormal               int
	TimeoutOom                  int
	DisableParallelismInChecker bool
}

var Defaults = DefaultOptionValues{
	AddLineHash:    false,
	AllowBuildFail: true,
	AvailMemRatio:  0.9,
	CheckProgress:  true,
	CheckerConfig: `{"csa_system_lib_options":"",
		"infer_bin":"infer",
		"clang_bin":"/opt/naivesystems/clang",
		"code_checker_bin":"CodeChecker",
		"cppcheck_bin":"/cppcheck/cppcheck",
		"python_bin":"python3",
		"clangtidy_bin":"/opt/naivesystems/clang-tidy",
		"clangquery_bin":"/opt/naivesystems/clang-query",
		"misra_checker_path":"/opt/naivesystems/misra",
		"infer_extra_options":"--max-nesting=3 --bo-field-depth-limit=6",
		"clangmapping_bin":"/opt/naivesystems/clang-extdef-mapping",
		"infer_jobs":8,
		"cpplint_script":"/opt/naivesystems/cpplint.py"}`,
	CheckerPathRoot:             "/opt/naivesystems/",
	ClangBin:                    "",
	ClangmappingBin:             "/opt/naivesystems/clang-extdef-mapping",
	ClangqueryBin:               "",
	ClangtidyBin:                "",
	CmakeCXXCompiler:            "clang",
	CmakeExportCompileCommands:  true,
	CodeCheckerBin:              "",
	ConfigDir:                   "/config",
	CppcheckBin:                 "",
	CpplintScript:               "/opt/naivesystems/cpplint.py",
	CsaSystemLibOptions:         "",
	DebugMode:                   false,
	EnableCodeChecker:           false,
	GccPredefinedMacros:         "",
	IgnoreDirPatterns:           []string{"/src/output/**"},
	IncludeStddef:               "pure",
	InferBin:                    "",
	InferExtraOptions:           "",
	InferJobs:                   0,
	KeepBuildActionPercent:      1,
	Lang:                        "zh",
	LimitMemory:                 false,
	MisraCheckerPath:            "",
	ProjectName:                 "",
	ProjectType:                 "makefile",
	PythonBin:                   "",
	QmakeBin:                    "/usr/bin/qmake-qt5",
	QtProPath:                   "",
	ResultsDir:                  "/output",
	ScriptContents:              "",
	ShowJsonResults:             true,
	ShowLineNumber:              true,
	ShowResults:                 false,
	ShowResultsCount:            false,
	SkipBearMake:                false,
	SrcDir:                      "/src",
	TimeoutNormal:               90,
	TimeoutOom:                  30,
	DisableParallelismInChecker: false,
}

func NewSharedOptions() *SharedOptions {
	option := &SharedOptions{}

	option.AddLineHash = flag.Bool("add_line_hash", Defaults.AddLineHash, "Whether to add code line hash into results")
	option.AllowBuildFail = flag.Bool("allow_build_fail", Defaults.AllowBuildFail, "Still analyze even the project can't build")
	option.AvailMemRatio = flag.Float64("avail_mem_ratio", Defaults.AvailMemRatio, "The ratio of available memory to be used. Negative value means no limitation")
	option.CheckProgress = flag.Bool("check_progress", Defaults.CheckProgress, "Show the checking progress")
	option.CheckerConfig = flag.String("checker_config", Defaults.CheckerConfig,
		"Checker configuration in JSON format")
	option.CheckerPathRoot = flag.String("checker_path", Defaults.CheckerPathRoot, "Use checker path root to find checker locations")
	option.ClangBin = flag.String("clang_bin", Defaults.ClangBin, "NaiveSystems Clang binary location")
	option.ClangmappingBin = flag.String("clangmapping_bin", Defaults.ClangmappingBin, "Clang-extdef-mapping binary location")
	option.ClangqueryBin = flag.String("clangquery_bin", Defaults.ClangqueryBin, "Clang-query binary location")
	option.ClangtidyBin = flag.String("clangtidy_bin", Defaults.ClangtidyBin, "Clang-tidy binary location")
	option.CmakeCXXCompiler = flag.String("cmake_CXX_compiler", Defaults.CmakeCXXCompiler, "Path of a valid CXX compipler, default clang")
	option.CmakeExportCompileCommands = flag.Bool("cmake_export_cc", Defaults.CmakeExportCompileCommands, "Directly generate compile commands from CMakeLists.txt")
	option.CodeCheckerBin = flag.String("code_checker_bin", Defaults.CodeCheckerBin, "CodeChecker binary location")
	option.ConfigDir = flag.String("config_dir", Defaults.ConfigDir, "Absolute path to a directory containing all configuration files")
	option.CppcheckBin = flag.String("cppcheck_bin", Defaults.CppcheckBin, "Cppcheck binary location")
	option.CpplintScript = flag.String("cpplint_script", Defaults.CpplintScript, "Cpplint script location")
	option.CsaSystemLibOptions = flag.String("csa_system_lib_options", Defaults.CsaSystemLibOptions, "Include path for clang")
	option.DebugMode = flag.Bool("debug_mode", Defaults.DebugMode, "Whether to display error information")
	option.EnableCodeChecker = flag.Bool("enable_codechecker", Defaults.EnableCodeChecker, "Use CodeChecker to perform CTU pre-analysis for CSA")
	option.GccPredefinedMacros = flag.String("gcc_predefined_macros", Defaults.GccPredefinedMacros, "Gcc predefiend macro arguments")
	option.IncludeStddef = flag.String("include_stddef", Defaults.IncludeStddef, "How to include <stddef.h>. Support pure(using -include), armgcc(including from arm-none-eabi) and none")
	option.InferBin = flag.String("infer_bin", Defaults.InferBin, "Infer binary location")
	option.InferExtraOptions = flag.String("infer_extra_options", Defaults.InferExtraOptions, "Infer options")
	option.InferJobs = flag.Int64("infer_jobs", Defaults.InferJobs, "Infer jobs")
	option.Lang = &Defaults.Lang
	option.KeepBuildActionPercent = flag.Float64("keep_build_action_percent", Defaults.KeepBuildActionPercent, "Will random remove build actions by this parameter")
	option.LimitMemory = flag.Bool("limit_memory", Defaults.LimitMemory, "Whether to limit the usage of memory")
	option.MisraCheckerPath = flag.String("misra_checker_path", Defaults.MisraCheckerPath, "Misra checker binary location")
	option.ProjectName = flag.String("project_name", Defaults.ProjectType, "Name of the checked project.")
	option.ProjectType = flag.String("project_type", Defaults.ProjectType, "Type of the checked project. Support makefile and cmake")
	option.PythonBin = flag.String("python_bin", Defaults.PythonBin, "Python binary location")
	option.QmakeBin = flag.String("qmake_bin", Defaults.QmakeBin, "Qmake binary location")
	option.QtProPath = flag.String("qt_pro_path", Defaults.QtProPath, "Path of the Qt project file relative to src_dir")
	option.ResultsDir = flag.String("results_dir", Defaults.ResultsDir, "Absolute path to the directory of results files")
	option.ScriptContents = flag.String("script_contents", Defaults.ScriptContents, "The contents of script to generate compile_commands.json")
	option.ShowJsonResults = flag.Bool("json_results", Defaults.ShowJsonResults, "Whether to output results in protojson format")
	option.ShowLineNumber = flag.Bool("show_line_number", Defaults.ShowLineNumber, "Show line count infomation")
	option.ShowResults = flag.Bool("show_results", Defaults.ShowResults, "Show results after the analysis")
	option.ShowResultsCount = flag.Bool("show_results_count", Defaults.ShowResultsCount, "Show results count group by rules after the analysis")
	option.SkipBearMake = flag.Bool("skip_bear_make", Defaults.SkipBearMake, "Skip running `bear -- make` to generate compilation database before the analysis")
	option.SrcDir = flag.String("src_dir", Defaults.SrcDir, "Absolute path to the directory of code files")
	option.TimeoutNormal = flag.Int("timeout_normal", Defaults.TimeoutNormal, "Minutes of timeout for checking single rule. Default value is 90")
	option.TimeoutOom = flag.Int("timeout_oom", Defaults.TimeoutOom, "Minutes of timeout for specific checkers when limit memory enabled. Default value is 30")
	option.DisableParallelismInChecker = flag.Bool("disable_parallelism_in_checker", Defaults.DisableParallelismInChecker, "Disable the parallelism of checking multiple files")

	flag.Var(&option.IgnoreDirPatterns, "ignore_dir", "Shell file name pattern to a directory that will be ignored")

	return option
}
