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
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"reflect"
	"regexp"
	"sort"
	"strings"
	"time"

	"github.com/golang/glog"
	"github.com/google/shlex"
	"golang.org/x/exp/slices"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/cruleslib/i18n"
	"naive.systems/analyzer/cruleslib/stats"
	"naive.systems/analyzer/misra/checker_integration/compilecommand"
	"naive.systems/analyzer/misra/utils"
)

type csaBuildAction struct {
	actionType       ActionType
	analyzerOptions  []string
	arch             string
	compiler         string
	compilerIncludes []string
	compilerStandard string
	compilerTarget   string
	directory        string
	lang             string
	output           string
	source           string
}

type actionType int

// Refer to codechecker/analyzer/codechecker_analyzer/buildlog/build_action.py.
const (
	LINK actionType = iota
	COMPILE
	PREPROCESS
	INFO
	UNASSIGNED
)

type ActionType interface {
	ActionType() actionType
}

func (at actionType) ActionType() actionType {
	return at
}

// This function determines the compiler from the given compilation command. If the first part of the gcc_command
// is ccache invocation then the rest should be a complete compilation command.
func determineCompiler(command []string) string {
	if strings.HasSuffix(command[0], "ccache") {
		executableBinary, err := exec.LookPath(command[1])
		if executableBinary != "" {
			return command[1]
		} else if err != nil {
			glog.Warningf("%s is not installed in your PATH, so we will use %s instead.", command[1], command[0])
		}
	}
	return command[0]
}

type FilterFunction func(commands []string, details *csaBuildAction) bool

// Refer to get_language in codechecker/analyzer/codechecker_analyzer/buildlog/log_parser.py
var extMappingLang = map[string]string{
	".c":   "c",
	".cp":  "c++",
	".cpp": "c++",
	".cxx": "c++",
	".txx": "c++",
	".cc":  "c++",
	".C":   "c++",
	".ii":  "c++"}

var SOURCE_EXTENSIONS = []string{".c", ".cc", ".cp", ".cpp", ".cxx", ".c++", ".o", ".so", ".a"}

// All constant variables below refer to the constants of the same name in
// codechecker/analyzer/codechecker_analyzer/buildlog/log_parser.py
var PRECOMPILATION_OPTION = regexp.MustCompile("-(E|M[G|T|Q|F|J|P|V|M]*)$")

var IGNORED_OPTIONS_CLANG_LIST = []string{
	"-Werror",
	"-pedantic-errors",
	"-w"}
var IGNORED_OPTIONS_CLANG = regexp.MustCompile(strings.Join(IGNORED_OPTIONS_CLANG_LIST, "|"))

var IGNORED_OPTIONS_GCC_LIST = []string{
	"-fallow-fetchr-insn",
	"-fcall-saved-",
	"-fcond-mismatch",
	"-fconserve-stack",
	"-fcrossjumping",
	"-fcse-follow-jumps",
	"-fcse-skip-blocks",
	"-fcx-limited-range$",
	"-fext-.*-literals",
	"-ffixed-r2",
	"-ffp$",
	"-mfp16-format",
	"-fgcse-lm",
	"-fhoist-adjacent-loads",
	"-findirect-inlining",
	"-finline-limit",
	"-finline-local-initialisers",
	"-fipa-sra",
	"-fmacro-prefix-map",
	"-fno-aggressive-loop-optimizations",
	"-fno-canonical-system-headers",
	"-fno-delete-null-pointer-checks",
	"-fno-defer-pop",
	"-fno-extended-identifiers",
	"-fno-jump-table",
	"-fno-keep-static-consts",
	"-f(no-)?reorder-functions",
	"-fno-strength-reduce",
	"-fno-toplevel-reorder",
	"-fno-unit-at-a-time",
	"-fno-var-tracking-assignments",
	"-fobjc-link-runtime",
	"-fpartial-inlining",
	"-fpeephole2",
	"-fr$",
	"-fregmove",
	"-frename-registers",
	"-frerun-cse-after-loop",
	"-fs$",
	"-fsched-spec",
	"-fstack-usage",
	"-fstack-reuse",
	"-fthread-jumps",
	"-ftree-pre",
	"-ftree-switch-conversion",
	"-ftree-tail-merge",
	"-m(no-)?abm",
	"-m(no-)?sdata",
	"-m(no-)?spe",
	"-m(no-)?string$",
	"-m(no-)?dsbt",
	"-m(no-)?fixed-ssp",
	"-m(no-)?pointers-to-nested-functions",
	"-mno-fp-ret-in-387",
	"-mpreferred-stack-boundary",
	"-mpcrel-func-addr",
	"-mrecord-mcount$",
	"-maccumulate-outgoing-args",
	"-mcall-aixdesc",
	"-mppa3-addr-bug",
	"-mtraceback=",
	"-mtext=",
	"-misa=",
	"-mfunction-return=",
	"-mindirect-branch-register",
	"-mindirect-branch=",
	"-mfix-cortex-m3-ldrd$",
	"-mmultiple$",
	"-msahf$",
	"-mskip-rax-setup$",
	"-mthumb-interwork$",
	"-mupdate$",
	"-mapcs",
	"-fno-merge-const-bfstores$",
	"-fno-ipa-sra$",
	"-mno-thumb-interwork$",
	"-mno-sched-prolog",
	"-DNDEBUG$",
	"-save-temps",
	"-Werror",
	"-pedantic-errors",
	"-w",
	"-g(.+)?$",
	"-flto",
	"-mxl",
	"-mfloat-gprs",
	"-mabi"}
var IGNORED_OPTIONS_GCC = regexp.MustCompile(strings.Join(IGNORED_OPTIONS_GCC_LIST, "|"))

var IGNORED_PARAM_OPTIONS = map[*regexp.Regexp]int{
	regexp.MustCompile("-install_name"):           1,
	regexp.MustCompile("-exported_symbols_list"):  1,
	regexp.MustCompile("-current_version"):        1,
	regexp.MustCompile("-compatibility_version"):  1,
	regexp.MustCompile("-init$"):                  1,
	regexp.MustCompile("-e$"):                     1,
	regexp.MustCompile("-seg1addr"):               1,
	regexp.MustCompile("-bundle_loader"):          1,
	regexp.MustCompile("-multiply_defined"):       1,
	regexp.MustCompile("-sectorder"):              3,
	regexp.MustCompile("--param$"):                1,
	regexp.MustCompile("-u$"):                     1,
	regexp.MustCompile("--serialize-diagnostics"): 1,
	regexp.MustCompile("-framework"):              1,
	regexp.MustCompile("-filelist"):               1}

var XCLANG_FLAGS_TO_SKIP = []string{
	"-module-file-info",
	"-S",
	"-emit-llvm",
	"-emit-llvm-bc",
	"-emit-llvm-only",
	"-emit-llvm-uselists",
	"-rewrite-objc"}

var COMPILE_OPTIONS_MERGED_LIST = []string{
	"--sysroot",
	"-sdkroot",
	"--include",
	"-include",
	"-iquote",
	"-[DIUF]",
	"-idirafter",
	"-isystem",
	"-imacros",
	"-isysroot",
	"-iprefix",
	"-iwithprefix",
	"-iwithprefixbefore"}
var COMPILE_OPTIONS_MERGED = regexp.MustCompile("(" + strings.Join(COMPILE_OPTIONS_MERGED_LIST, "|") + ")")

var CLANG_OPTIONS = regexp.MustCompile(".*")

var REPLACE_OPTIONS_MAP = map[string][]string{
	"-mips32":     {"-target", "mips", "-mips32"},
	"-mips64":     {"-target", "mips64", "-mips64"},
	"-mpowerpc":   {"-target", "powerpc"},
	"-mpowerpc64": {"-target", "powerpc64"}}

var COMPILE_OPTIONS_LIST = []string{
	"-nostdinc",
	"-nostdinc\\+\\+", // r'-nostdinc\+\+'
	"-pedantic",
	"-O[1-3]",
	"-Os",
	"-std=",
	"-stdlib=",
	"-f",
	"-m",
	"-Wno-",
	"--sysroot=",
	"-sdkroot",
	"--gcc-toolchain="}
var COMPILE_OPTIONS = regexp.MustCompile(strings.Join(COMPILE_OPTIONS_LIST, "|"))

var INCLUDE_OPTIONS_MERGED_LIST = []string{
	"-iquote",
	"-[IF]",
	"-isystem",
	"-iprefix",
	"-iwithprefix",
	"-iwithprefixbefore",
	// The adding '-cxx-isystem' is used to add directory to the C++ SYSTEM include search path
	"-cxx-isystem"}
var INCLUDE_OPTIONS_MERGED = regexp.MustCompile("(" + strings.Join(INCLUDE_OPTIONS_MERGED_LIST, "|") + ")")

// This function skips the source file names (i.e. the arguments which don't start with a dash character).
func skipSources(commands []string, details *csaBuildAction) bool {
	return commands[0][0] != '-'
}

// This function consumes -x flag which is followed by the language.
func getLanguage(commands []string, details *csaBuildAction) bool {
	if strings.HasPrefix(commands[0], "-x") {
		if commands[0] == "-x" {
			if len(commands[0]) > 1 {
				details.lang = commands[1]
				return true
			}
		} else {
			details.lang = commands[0][2:] // 2 == len('-x')
			return true
		}
	}
	return false
}

// This function determines whether this is a preprocessing, compilation or linking action.
// If the action type is set to COMPILE earlier then we don't set it to anything else.
func determineActionType(commands []string, details *csaBuildAction) bool {
	if commands[0] == "-c" {
		details.actionType = COMPILE
		return true
	} else if strings.HasPrefix(commands[0], "-print-prog-name") {
		if details.actionType != COMPILE {
			details.actionType = INFO
		}
		return true
	} else if PRECOMPILATION_OPTION.MatchString(commands[0]) {
		if details.actionType != COMPILE {
			details.actionType = PREPROCESS
		}
		return true
	}
	return false
}

// This function consumes -arch flag which is followed by the target architecture.
func getArch(commands []string, details *csaBuildAction) bool {
	if commands[0] == "-arch" {
		if len(commands) > 1 {
			details.arch = commands[1]
			return true
		}
	}
	return false
}

// This function consumes -o flag which is followed by the output file of the action. This file is then collected to the buildaction object.
func getOutput(commands []string, details *csaBuildAction) bool {
	if commands[0] == "-o" {
		if len(commands) > 1 {
			details.output = commands[1]
			return true
		}
	}
	return false
}

// This function skips the flag pointed by the given flag_iterator with its parameters if any.
func skipClang(commands []string, details *csaBuildAction) bool {
	return IGNORED_OPTIONS_CLANG.MatchString(commands[0]) && IGNORED_OPTIONS_CLANG.FindStringIndex(commands[0])[0] == 0
}

// This function skips the flag pointed by the given flag_iterator with its parameters if any.
func skipGcc(commands []string, details *csaBuildAction) bool {
	if IGNORED_OPTIONS_GCC.MatchString(commands[0]) && IGNORED_OPTIONS_GCC.FindStringIndex(commands[0])[0] == 0 {
		return true
	}
	for pattern, argNum := range IGNORED_PARAM_OPTIONS {
		if pattern.MatchString(commands[0]) && pattern.FindStringIndex(commands[0])[0] == 0 {
			if argNum < len(commands) {
				return true
			}
		}
	}
	return false
}

// Some specific -Xclang constucts need to be filtered out.
// To generate the proper plist reports and not LLVM IR or ASCII text as an output the flags need to be removed.
func collectTransformXclangOpts(commands []string, details *csaBuildAction) bool {
	if commands[0] == "-Xclang" {
		if len(commands) > 1 {
			nextFlag := commands[1]
			if slices.Contains(XCLANG_FLAGS_TO_SKIP, nextFlag) {
				return true
			}
			details.analyzerOptions = append(details.analyzerOptions, "-Xclang", nextFlag)
			return true
		}
	}
	return false
}

// This function collects the compilation (i.e. not linker or preprocessor) flags to the build action.
func collectTransformIncludeOpts(commands []string, details *csaBuildAction) bool {
	item := commands[0]
	if !COMPILE_OPTIONS_MERGED.MatchString(item) || COMPILE_OPTIONS_MERGED.FindStringIndex(item)[0] != 0 {
		return false
	}
	flag := COMPILE_OPTIONS_MERGED.FindString(item)
	together := len(flag) != len(item)
	var param string
	if together {
		param = item[len(flag):]
	} else {
		if len(commands) > 1 {
			param = commands[1]
		}
	}
	flagWithPath := []string{"-I", "-idirafter", "-iquote", "-isysroot", "-isystem", "-sysroot", "--sysroot"}
	f := string(flag)
	if slices.Contains(flagWithPath, f) && (strings.Contains(f, "isysroot") || param[0] != '=') {
		if param[0] == '=' {
			param = param[1:]
			together = false
		}
		// If the later path is an absolute path, previous path is thrown away.
		includePath := ""
		if filepath.IsAbs(param) {
			includePath = string(param)
		} else {
			includePath = details.directory + "/" + string(param)
		}
		param = filepath.Clean(filepath.FromSlash(includePath))
	}
	if together {
		details.analyzerOptions = append(details.analyzerOptions, f+param)
	} else {
		details.analyzerOptions = append(details.analyzerOptions, f, param)
	}
	return true
}

// Collect all the options for clang do not filter anything.
func collectClangCompileOpts(commands []string, details *csaBuildAction) bool {
	if CLANG_OPTIONS.MatchString(commands[0]) && CLANG_OPTIONS.FindStringIndex(commands[0])[0] == 0 {
		details.analyzerOptions = append(details.analyzerOptions, commands[0])
		return true
	}
	return false
}

// This function extends the analyzer options list with the corresponding replacement based on
// REPLACE_OPTIONS_MAP if the flag_iterator is currently pointing to a flag to replace.
func replace(commands []string, details *csaBuildAction) bool {
	value := REPLACE_OPTIONS_MAP[commands[0]]
	if value != nil {
		details.analyzerOptions = append(details.analyzerOptions, strings.Join(value, " "))
	}
	return value != nil
}

// This function collects the compilation (i.e. not linker or preprocessor) flags to the buildaction.
func collectCompileOpts(commands []string, details *csaBuildAction) bool {
	if COMPILE_OPTIONS.MatchString(commands[0]) && COMPILE_OPTIONS.FindStringIndex(commands[0])[0] == 0 {
		details.analyzerOptions = append(details.analyzerOptions, commands[0])
		return true
	}
	return false
}

// Check for the --gcc-toolchain in the compilation options.
func gccToolchainInArgs(compilerOption []string) string {
	for _, cmpOpt := range compilerOption {
		if strings.Contains(cmpOpt, "--gcc-toolchain") {
			reg := regexp.MustCompile("^--gcc-toolchain=(?P<tcpath>.*)$")
			tcpath := reg.FindString(cmpOpt)
			return tcpath
		}
	}
	return ""
}

// Return the list of flags which affect the list of implicit includes.
// Refer to codechecker/analyzer/codechecker_analyzer/buildlog/log_parser.py:filter_compiler_includes_extra_args.
func filterCompilerIncludesExtraArgs(compilerFlags []string) []string {
	// Keep flags like -std=, --sysroot=, -m32, -m64, -nostdinc or -stdlib= and filter out extra flags.
	pattern := regexp.MustCompile("-m(32|64)|-std=|-stdlib=|-nostdinc")
	extraOpts := []string{}
	for idx, flag := range compilerFlags {
		if pattern.MatchString(flag) && pattern.FindStringIndex(flag)[0] == 0 {
			extraOpts = append(extraOpts, flag)
		}
		if strings.HasPrefix(flag, "--sysroot") {
			if flag == "--sysroot" && len(compilerFlags) > idx+1 {
				extraOpts = append(extraOpts, "--sysroot"+compilerFlags[idx+1])
			} else {
				extraOpts = append(extraOpts, compilerFlags[idx])
			}
		}
	}
	return extraOpts
}

// Returns a list of default includes of the given compiler.
func getCompilerIncludes(compiler string, language string, compilerFlags []string) ([]string, error) {
	// Add a filter to handle cases like linux kernel analysis, where there are many extra flags in analyzerOptions.
	// More details in: https://github.com/Ericsson/codechecker/issues/3110.
	extraOpts := filterCompilerIncludesExtraArgs(compilerFlags)
	cmds := []string{"-E", "-x", language, "-", "-v"}
	cmds = append(extraOpts, cmds...)
	cmds = append([]string{compiler}, cmds...)
	var includeDirs []string
	// "gcc -v -E -" prints a set of information about the execution of the preprocessor.
	// This includeDirs contains the implicitly included paths.
	// It collects these paths from the output of the gcc command above.
	// The actual build command is the parameter of this function because the list of implicit
	// include paths is affected by some compiler flags (e.g. --sysroot, -x, etc.
	startMark := "#include"
	startState := []string{"#include", "<...>", "search", "starts", "here:"}
	endMark := "End"
	endState := []string{"End", "of", "search", "list."}
	cmd := exec.Command(cmds[0], cmds[1:]...)
	stdoutStderr, err := cmd.CombinedOutput()
	if err != nil {
		// To get the stderr, it is not an error that needed returning. Print it here for debugging.
		glog.Infof("To get the stderr, this is not an error. cmd.CombinedOutput: %v", err)
	}
	fields := strings.Fields(string(stdoutStderr))
	hasStartState, hasEndState := false, false
	if fields != nil {
		startIdx := -1
		skipFrameworkDir := []string{"(framework", "directory)"}
		for idx, field := range fields {
			// Only if the joined group of the following fields matches endState does, appending process is ended.
			if field == endMark && reflect.DeepEqual(fields[idx:idx+len(endState)], endState) {
				hasEndState = true
				break
			}
			if startIdx != -1 && idx >= startIdx && !slices.Contains(skipFrameworkDir, field) {
				includeDirs = append(includeDirs, field)
			}
			// Start counting fields as long as the joined group of the following fields matches startState.
			if field == startMark && reflect.DeepEqual(fields[idx:idx+len(startState)], startState) {
				hasStartState = true
				startIdx = idx + len(startState)
			}
		}
	}
	if !hasStartState || !hasEndState {
		glog.Errorf("Retrieving default includes by executing: %s", cmd.String())
		glog.Errorf("Unexpected stderr for parse includes. stdoutStderr: %v.", string(stdoutStderr))
		return nil, fmt.Errorf("cmd.CombinedOutput: %v", err)
	}
	resultIncludeDirs := []string{}
	for _, includePath := range includeDirs {
		resultIncludeDirs = append(resultIncludeDirs, filepath.Clean(filepath.FromSlash(includePath)))
	}
	return resultIncludeDirs, nil
}

// Returns the default compiler standard of the given compiler.
// The standard is determined by the values of __STDC_VERSION__ and __cplusplus predefined macros.
// These values are integers indicating the date of the standard.
// However, GCC supports a GNU extension for each standard. For sake of generality we return the GNU extended standard,
// since it should be a superset of the non-extended one, thus applicable in a more general manner.
func getCompilerStandard(compiler string, language string) (string, error) {
	VERSION_C :=
		`#ifdef __STDC_VERSION__
#  if __STDC_VERSION__ >= 201710L
#    error CC_FOUND_STANDARD_VER#17
#  elif __STDC_VERSION__ >= 201112L
#    error CC_FOUND_STANDARD_VER#11
#  elif __STDC_VERSION__ >= 199901L
#    error CC_FOUND_STANDARD_VER#99
#  elif __STDC_VERSION__ >= 199409L
#    error CC_FOUND_STANDARD_VER#94
#  else
#    error CC_FOUND_STANDARD_VER#90
#  endif
#else
#  error CC_FOUND_STANDARD_VER#90
#endif`
	VERSION_CPP :=
		`#ifdef __cplusplus
#  if __cplusplus >= 201703L
#    error CC_FOUND_STANDARD_VER#17
#  elif __cplusplus >= 201402L
#    error CC_FOUND_STANDARD_VER#14
#  elif __cplusplus >= 201103L
#    error CC_FOUND_STANDARD_VER#11
#  elif __cplusplus >= 199711L
#    error CC_FOUND_STANDARD_VER#98
#  else
#    error CC_FOUND_STANDARD_VER#98
#  endif
#else
#  error CC_FOUND_STANDARD_VER#98
#endif`
	standard := ""
	suffix := ".cpp"
	if language == "c" {
		suffix = ".c"
	}
	tmpFile, err := os.CreateTemp(os.TempDir(), "tmp-*"+suffix)
	if err != nil {
		return "", fmt.Errorf("os.CreateTemp: %v", err)
	}
	if language == "c" {
		_, err = tmpFile.Write([]byte(VERSION_C))
	} else {
		_, err = tmpFile.Write([]byte(VERSION_CPP))
	}
	if err != nil {
		return "", fmt.Errorf("tmpFile.Write: %v", err)
	}
	cmd := exec.Command(compiler, tmpFile.Name())
	stderr, err := cmd.CombinedOutput()
	if err != nil {
		// To get the stderr, it is not an error that needed returning. Print it here for debugging.
		glog.Infof("To get the stderr, this is not an error. cmd.CombinedOutput: %v", err)
	}
	line := string(stderr)
	if line == "" {
		return "", fmt.Errorf("cmd.CombinedOutput: %v", err)
	} else {
		pattern := regexp.MustCompile("CC_FOUND_STANDARD_VER#(.+)")
		match := pattern.FindAllStringSubmatch(line, -1)
		if len(match) > 0 {
			standard = match[0][1][0:2]
		} else {
			glog.Errorf("Failed get compiler standard by executing: %s", cmd.String())
			glog.Errorf("The stdout and stderr is %s", line)
			return "", fmt.Errorf("cmd.CombinedOutput: %v", err)
		}
	}
	if standard != "" {
		if standard == "94" {
			// Special case for C94 standard.
			standard = "-std=iso9899:199409"
		} else {
			str := "++"
			if language == "c" {
				str = ""
			}
			standard = "-std=gnu" + str + standard
		}
	}
	defer os.Remove(tmpFile.Name())
	return standard, nil
}

// Returns the target triple of the given compiler as a string.
func getCompilerTarget(compiler string) (string, error) {
	cmds := []string{compiler, "-v"}
	fields, err := utils.GetCommandStdout(cmds, "")
	if err != nil {
		// The error may not affect the parsing of target, so do not return it here.
		glog.Warningf("This error may not affect the parsing after. cmd.CombinedOutput: %v", err)
	}
	if fields == nil {
		return "", fmt.Errorf("utils.GetCommandStdout: %v", err)
	}
	targetLabel := "Target:"
	target := ""
	for idx, field := range fields {
		if field == targetLabel {
			target = fields[idx+1]
		}
	}
	if target == "" {
		glog.Errorf("Failed to get target by executing: %s", strings.Join(cmds, " "))
		glog.Errorf("The stdout and stderr is %s", strings.Join(fields, " "))
		return "", fmt.Errorf("utils.GetCommandStdout: %v", err)
	}
	return target, nil
}

// This function consumes --target or -target flag which is followed by the compilation target architecture.
// This target might be different from the default compilation target collected from the compiler
// if cross compilation is done for another target.
func getTarget(commands []string, details *csaBuildAction) bool {
	targets := []string{"--target", "-target"}
	if slices.Contains(targets, commands[0]) && len(commands) > 1 {
		details.compilerTarget = commands[1]
		return true
	}
	return false
}

func filter(lst []string, f func(i string) bool) []string {
	var r []string
	for _, s := range lst {
		if f(s) {
			r = append(r, s)
		}
	}
	return r
}

// This function returns True in case the given dirname is NOT a GCC-specific include-fixed
// directory containing standard headers.
func isNotIncludeFixed(dirname string) bool {
	return path.Base(filepath.Clean(filepath.FromSlash(dirname))) != "include-fixed"
}

// Returns True if the given directory doesn't contain any intrinsic headers.
func containsNoIntrinsicHeaders(dirname string) bool {
	_, err := os.Stat(dirname)
	if err != nil {
		return true
	}
	if ok, _ := filepath.Glob(filepath.Join(dirname, "*intrin.h")); ok != nil {
		return false
	}
	return true
}

// This function filters out intrin directories in analyzer options.
func filterOutIntrinOptions(analyzerOptions *[]string) {
	aopWithoutIntrin := []string{}
	skipNextOption := false
	for k, aopt := range *analyzerOptions {
		if skipNextOption {
			skipNextOption = false
			continue
		}
		if INCLUDE_OPTIONS_MERGED.MatchString(aopt) && INCLUDE_OPTIONS_MERGED.FindStringIndex(aopt)[0] == 0 {
			flag := INCLUDE_OPTIONS_MERGED.FindString(aopt)
			together := len(string(flag)) != len(aopt)
			var param string
			if together {
				param = aopt[len(flag):]
			} else if k+1 < len(*analyzerOptions) {
				flag = aopt
				param = (*analyzerOptions)[k+1]
				// The next option is the included path which has been appended in this loop.
				// Without skipping it, there will be a duplicated option following.
				skipNextOption = true
			}
			// The param cannot be a relative path since the flags indicating include dirs
			// in analyzerOptions have been processed in func collectTransformIncludeOpts.
			f, err := os.Stat(param)
			isDir := (err == nil && f.IsDir())
			if isDir && containsNoIntrinsicHeaders(param) || !isDir {
				if together {
					aopWithoutIntrin = append(aopWithoutIntrin, aopt)
				} else {
					aopWithoutIntrin = append(aopWithoutIntrin, flag)
					aopWithoutIntrin = append(aopWithoutIntrin, param)
				}
			}
		} else {
			aopWithoutIntrin = append(aopWithoutIntrin, aopt)
		}
	}
	*analyzerOptions = aopWithoutIntrin
}

// This function parses a GCC or Clang compilation command
// and returns a csaBuildAction object which can be the input of Clang analyzer tools.
func ParseOptions(compilationDbEntry *compilecommand.CompileCommand, config *pb.CheckerConfiguration, onlyCppcheck bool) csaBuildAction {
	details := csaBuildAction{}
	details.directory = compilationDbEntry.Directory

	details.source = compilationDbEntry.File
	// In case the file attribute in the entry is empty.
	if details.source == "." {
		details.source = ""
	}
	if onlyCppcheck {
		return details
	}

	// Will be changed to other options like LINK, COMPILE, PREPROCESS, or INFO.
	details.actionType = UNASSIGNED
	var command []string
	if compilationDbEntry.Arguments != nil {
		command = compilationDbEntry.Arguments
	} else if compilationDbEntry.Command != "" {
		command, _ = shlex.Split(compilationDbEntry.Command)
	} else {
		glog.Error("No valid 'command' or 'arguments' entry found!")
		return details
	}
	details.compiler = determineCompiler(command)
	if strings.Contains(filepath.Base(details.compiler), "++") {
		details.lang = "c++"
	}
	// Source files are skipped first so they are not collected with the other compiler flags together.
	// Source file is handled separately from the compile command json.
	filterFunctionMap := map[string]FilterFunction{
		"collectClangCompileOpts":     collectClangCompileOpts,
		"collectCompileOpts":          collectCompileOpts,
		"collectTransformIncludeOpts": collectTransformIncludeOpts,
		"collectTransformXclangOpts":  collectTransformXclangOpts,
		"determineActionType":         determineActionType,
		"getArch":                     getArch,
		"getLanguage":                 getLanguage,
		"getOutput":                   getOutput,
		"getTarget":                   getTarget,
		"replace":                     replace,
		"skipClang":                   skipClang,
		"skipGcc":                     skipGcc,
		"skipSources":                 skipSources}
	clangFlagCollectors := []string{
		"skipSources",
		"skipClang",
		"collectTransformXclangOpts",
		"getOutput",
		"determineActionType",
		"getArch",
		"getTarget",
		"getLanguage",
		"collectTransformIncludeOpts",
		"collectClangCompileOpts"}
	gccFlagTransformers := []string{
		"skipGcc",
		"replace",
		"collectTransformIncludeOpts",
		"collectCompileOpts",
		"determineActionType",
		"skipSources",
		"getArch",
		"getTarget",
		"getLanguage",
		"getOutput"}
	flagProcessors := gccFlagTransformers
	usingSameClangToCompileAndAnalyze := false
	if strings.Contains(details.compiler, "clang") {
		flagProcessors = clangFlagCollectors
		usingSameClangToCompileAndAnalyze = true
	}
	for it := 1; it < len(command); it++ {
		for _, processorName := range flagProcessors {
			// TODO: use a more efficient way to construct an iterator as CodeChecker does.
			if filterFunctionMap[processorName](command[it:], &details) {
				break
			}
		}
	}
	lang := extMappingLang[path.Ext(details.source)]
	if lang != "" {
		if details.lang == "" {
			details.lang = lang
		}
	} else {
		details.actionType = LINK
	}
	if details.actionType == UNASSIGNED {
		details.actionType = COMPILE
	}
	compilationDbEntry.Arguments = command
	toolchain := gccToolchainInArgs(details.analyzerOptions)
	// Store the compiler built in include paths and defines.
	// If clang compiler is used for compilation and analysis, do not collect the implicit include paths.
	if toolchain == "" && !usingSameClangToCompileAndAnalyze {
		// C/C++ compilers have implicit assumptions about the environment.
		// Especially GCC has some built-in options which make the build process non-portable to other compilers.
		// The goal of the content in this 'if' block is to gather and maintain implicit information.
		// Refer to codechecker/analyzer/codechecker_analyzer/buildlog/log_parser.py:ImplicitCompilerInfo.

		// Specifically, set compilerIncludes, compilerStandard, and compilerTarget without changing other attributes in 'details'.
		if details.compilerIncludes == nil {
			compilerIncludes, err := getCompilerIncludes(details.compiler, details.lang, details.analyzerOptions)
			if err != nil {
				glog.Errorf("getCompilerIncludes: %v", err)
			}
			details.compilerIncludes = compilerIncludes
		}
		if details.compilerStandard == "" {
			compilerStandard, err := getCompilerStandard(details.compiler, details.lang)
			if err != nil {
				glog.Errorf("getCompilerStandard: %v", err)
			}
			details.compilerStandard = compilerStandard
		}
		if details.compilerTarget == "" {
			compilerTarget, err := getCompilerTarget(details.compiler)
			if err != nil {
				glog.Errorf("getCompilerTarget: %v", err)
			}
			details.compilerTarget = compilerTarget
		}
	}
	details.compilerIncludes = filter(details.compilerIncludes, isNotIncludeFixed)
	details.compilerIncludes = filter(details.compilerIncludes, containsNoIntrinsicHeaders)
	filterOutIntrinOptions(&details.analyzerOptions)
	return details
}

// Return list of options and source files from the given response file.
// Refer to codechecker/analyzer/codechecker_analyzer/buildlog/log_parser.py:process_response_file.
func processResponseFile(responseFile string) ([]string, []string) {
	rFile, err := os.ReadFile(responseFile)
	if err != nil {
		// This error is ignored in CodeChecker
		glog.Warningf("unable to read file: %v", err)
	}
	options, err := shlex.Split(string(rFile))
	if err != nil {
		glog.Warningf("shlex.Split: %v", rFile)
	}
	sources := []string{}
	for _, opt := range options {
		if !strings.HasPrefix(opt, "-") && slices.Contains(SOURCE_EXTENSIONS, strings.ToLower(filepath.Ext(opt))) {
			sources = append(sources, opt)
		}
	}
	return options, sources
}

// Loop through the compilation database entries and whether compilation command contains a response file
// we read those files and replace the response file with the options from the file.
// Refer to codechecker/analyzer/codechecker_analyzer/buildlog/log_parser.py:extend_compilation_database_entries.
func extendCompilationDatabaseEntries(compilationDatabase *[]compilecommand.CompileCommand) []compilecommand.CompileCommand {
	var entries []compilecommand.CompileCommand
	for _, entry := range *compilationDatabase {
		// Response Files: On some systems (such as older UNIX systems and certain Windows variants) command-lines have relatively limited lengths.
		// Windows compilers therefore support "response files". These files are mentioned on the command-line (using the "@file") syntax.
		// The compiler reads these files and inserts the contents into argv, thereby working around the command-line length limits.
		// This feature is frequently requested by Windows users of GCC.
		if strings.Contains(entry.Command, "@") {
			cmd := []string{}
			sourceFiles := []string{}
			sourceDir := entry.Directory
			options, err := shlex.Split(entry.Command)
			if err != nil {
				glog.Warningf("shlex.Split: %v", entry.Command)
			}
			for _, opt := range options {
				if strings.HasPrefix(opt, "@") {
					responseFile := ""
					if strings.HasPrefix(filepath.Clean(opt[1:]), filepath.Clean(sourceDir)) {
						responseFile = opt[1:]
					} else {
						responseFile = filepath.Join(sourceDir, opt[1:])
					}
					if _, err := os.Stat(responseFile); err != nil {
						glog.Warningf("Response file %s does not exists.", responseFile)
						continue
					}
					opts, sources := processResponseFile(responseFile)
					cmd = append(cmd, opts...)
					sourceFiles = append(sourceFiles, sources...)
				} else {
					cmd = append(cmd, opt)
				}
			}
			entry.Command = strings.Join(cmd, " ")
			if strings.HasPrefix(entry.File, "@") {
				for _, sourceFile := range sourceFiles {
					entry.File = sourceFile
				}
			}
		}
		entries = append(entries, entry)
	}
	return entries
}

func GetBuildActionsFromCompileCommands(resultsDir string, compilationDatabase *[]compilecommand.CompileCommand, config *pb.CheckerConfiguration, checkProgress bool, lang string) *[]csaBuildAction {
	if compilationDatabase == nil {
		glog.Error("The compile database is empty.")
		return nil
	}
	printer := i18n.GetPrinter(lang)
	start := time.Now()
	var startedTaskNumbers int = 0
	taskNumbers := len(*compilationDatabase)
	sourceFileToBuildActionMap := make(map[string]csaBuildAction)
	for _, entry := range extendCompilationDatabaseEntries(compilationDatabase) {
		action := ParseOptions(&entry, config, false /*onlyCppcheck*/)
		startedTaskNumbers++
		percentStr := basic.GetPercentString(startedTaskNumbers, taskNumbers)
		percent := percentStr + "%"
		if action.lang == "" || action.actionType != COMPILE {
			if checkProgress {
				basic.PrintfWithTimeStamp(printer.Sprintf("%s BuildAction preparation completed (%v/%v)", percent, startedTaskNumbers, taskNumbers))
			}
			continue
		}
		// Based on source file, uniqueing by alphabetically first output object file name.
		// Following the same type of SOURCE_ALPHA in CompileActionUniqueingType of CodeChecker.
		if _, exist := sourceFileToBuildActionMap[action.source]; !exist {
			sourceFileToBuildActionMap[action.source] = action
		} else if action.output < sourceFileToBuildActionMap[action.source].output {
			sourceFileToBuildActionMap[action.source] = action
		}
		if checkProgress {
			basic.PrintfWithTimeStamp(printer.Sprintf("%s BuildAction preparation completed (%v/%v)", percent, startedTaskNumbers, taskNumbers))
			stats.WriteProgress(resultsDir, stats.PP, percentStr, start)
		}
	}
	// Sort commands alphabetically by their source file name.
	keys := make([]string, 0, len(sourceFileToBuildActionMap))
	for s := range sourceFileToBuildActionMap {
		keys = append(keys, s)
	}
	sort.Strings(keys)
	var BuildActions []csaBuildAction
	for _, k := range keys {
		BuildActions = append(BuildActions, sourceFileToBuildActionMap[k])
	}
	glog.Info("GetBuildActionsFromCompileCommands: parsing commands done.")
	elapsed := time.Since(start)
	if checkProgress {
		timeUsed := basic.FormatTimeDuration(elapsed)
		basic.PrintfWithTimeStamp(printer.Sprintf("BuildAction preparation completed [%s]", timeUsed))
	}
	return &BuildActions
}
