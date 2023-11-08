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
	"path/filepath"
	"reflect"
	"strings"

	"github.com/golang/glog"
	"github.com/google/shlex"
	"gopkg.in/yaml.v2"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration/compilecommand"
)

// Use CodeChecker to generate needed extra arguments for CTU analysis from given
// compile_commands.json, including externalDefMap.txt & invocation-list.yml .
func GenerateCTUExtraArgumentsFromCodeChecker(compileCommandsPath, resultsDir string, config *pb.CheckerConfiguration) error {
	cmd_arr := []string{"analyze", "--ctu-collect", "--ctu-ast-mode", "parse-on-demand"}
	cmd_arr = append(cmd_arr, "-o", resultsDir) // TODO: Replace with a configurable path
	cmd_arr = append(cmd_arr, compileCommandsPath)
	cmd := exec.Command(config.CodeCheckerBin, cmd_arr...)
	glog.Info("executing: ", cmd.String())
	if out, err := cmd.CombinedOutput(); err != nil {
		glog.Errorf("failed to execute codechecker, reported: \n%s", string(out))
		return err
	}
	// Inject CSA System Lib Options & GCC Predefined Macros into all `/ctu-dir/*/invocation-list.yml``
	invocationListPattern := filepath.Join(resultsDir, "ctu-dir", "*", "invocation-list.yml")
	matches, err := filepath.Glob(invocationListPattern)
	if err != nil {
		return fmt.Errorf("filepath.Glob: %v", err)
	}
	for _, match := range matches {
		invocations := make(map[string][]string)
		contents, err := os.ReadFile(match)
		if err != nil {
			glog.Errorf("os.ReadFile: %v", err)
			continue
		}
		err = yaml.Unmarshal(contents, &invocations)
		if err != nil {
			glog.Errorf("yaml.Unmarshal: %v", err)
			continue
		}
		for k := range invocations {
			invocations[k] = append(invocations[k], strings.Fields(config.CsaSystemLibOptions)...)
			invocations[k] = append(invocations[k], ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), false)...)
		}
		contents, err = yaml.Marshal(invocations)
		if err != nil {
			glog.Errorf("yaml.Unmarshal: %v", err)
			continue
		}
		err = os.WriteFile(match, contents, os.ModePerm)
		if err != nil {
			glog.Errorf("os.WriteFile: %v", err)
		}
	}
	return nil
}

// BuildAction contains info extracted from compile command.
type BuildAction struct {
	Command compilecommand.CompileCommand
	Arch    string // the processor architecture, e.g., 'x86_64'
	Target  string // the target hardware platform, e.g.,'x86_64-redhat-linux'
}

// To prepare unique entrance files for CSA.
func ParseCompileCommands(
	compileCommandsPath string,
	ignoreDirPatterns []string,
	config *pb.CheckerConfiguration,
	ignoreCpp,
	onlyCppcheck,
	isDev bool,
) (*[]BuildAction, error) {
	compileCommands, err := compilecommand.ReadCompileCommandsFromFile(compileCommandsPath)
	if err != nil {
		glog.Errorf("compilecommand.ReadCompileCommandsFromFiles: %v", err)
		return nil, err
	}
	uniqueBuildActions := []BuildAction{}
	fileEntrances := map[string]bool{}
	for _, command := range *compileCommands {
		// Convert `command` field to `arguments` if and only if `arguments` is empty.
		// If both are given, ignore `command` and use `arguments`
		if len(command.Arguments) == 0 && len(command.Command) > 0 {
			command.Arguments, err = shlex.Split(command.Command)
			if err != nil {
				glog.Errorf("failed to parse command: %v", err)
				continue
			}
		}
		if len(command.Arguments) == 0 && !onlyCppcheck {
			glog.Error("failed to parse command: arguments field is empty")
			continue
		}
		if fileEntrances[command.File] {
			continue
		}
		if strings.HasSuffix(command.File, ".s") || strings.HasSuffix(command.File, ".S") ||
			(ignoreCpp && (strings.HasSuffix(command.File, ".cpp") ||
				strings.HasSuffix(command.File, ".cc") ||
				strings.HasSuffix(command.File, ".cp") ||
				strings.HasSuffix(command.File, ".cxx") ||
				strings.HasSuffix(command.File, ".CPP") ||
				strings.HasSuffix(command.File, ".C") ||
				strings.HasSuffix(command.File, ".ii") ||
				strings.HasSuffix(command.File, ".m") ||
				strings.HasSuffix(command.File, ".mi") ||
				strings.HasSuffix(command.File, ".mii") ||
				strings.HasSuffix(command.File, ".mm") ||
				strings.HasSuffix(command.File, ".M") ||
				strings.HasSuffix(command.File, ".cxx"))) {
			fileEntrances[command.File] = true
			continue
		}
		// filter out the files which are not under /src/
		if !isDev && filepath.IsAbs(command.File) && !strings.HasPrefix(command.File, "/src") {
			fileEntrances[command.File] = true
			continue
		}
		action := ParseOptions(&command, config, onlyCppcheck)
		if !onlyCppcheck {
			if action.arch == "" {
				archInTriple, err := getTripleArch(&action)
				if err != nil {
					glog.Errorf("getTripleArch: %v", err)
				}
				action.arch = archInTriple
			}
			if action.compilerTarget == "" {
				compilerTarget, err := getCompilerTarget(action.compiler)
				if err != nil {
					glog.Errorf("getCompilerTarget: %v", err)
				}
				action.compilerTarget = compilerTarget
			}
		}
		matched, err := analyzerinterface.MatchIgnoreDirPatterns(ignoreDirPatterns, command.File)
		if err != nil {
			glog.Error(err)
			continue
		}
		if !matched {
			uniqueBuildActions = append(uniqueBuildActions, BuildAction{command, action.arch, action.compilerTarget})
		}
		fileEntrances[command.File] = true
	}
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	return &uniqueBuildActions, nil
}

func ParsePreprocessorSymbols(action BuildAction, useClang, useCC1 bool) []string {
	preprocessorSymbols := make([]string, 0)
	for idx, arg := range action.Command.Arguments {
		// cppcheck needs preprocessorSymbols to correctly generate dump file for build action
		if strings.HasPrefix(arg, "-I") || strings.HasPrefix(arg, "-D") {
			if len(arg) == 2 { // for args like `-I include/folder or -D MARCONAME=VALUE`
				preprocessorSymbols = append(preprocessorSymbols, arg+action.Command.Arguments[idx+1])
			} else { // for args like `-Iinclude/folder or -DMARCONAME=VALUE`
				preprocessorSymbols = append(preprocessorSymbols, arg)
			}
		}
		if arg == "-isystem" {
			preprocessorSymbols = append(preprocessorSymbols, arg)
			preprocessorSymbols = append(preprocessorSymbols, action.Command.Arguments[idx+1])
		}
		if arg == "-include" {
			preprocessorSymbols = append(preprocessorSymbols, arg)
			preprocessorSymbols = append(preprocessorSymbols, action.Command.Arguments[idx+1])
		}
		if arg == "-fshort-wchar" && useClang {
			if useCC1 {
				preprocessorSymbols = append(preprocessorSymbols, "-fno-signed-wchar", "-fwchar-type=short")
			} else {
				preprocessorSymbols = append(preprocessorSymbols, arg)
			}
		}
		if arg == "-m32" {
			if useClang {
				preprocessorSymbols = append(preprocessorSymbols, arg)
			} else if !useCC1 {
				preprocessorSymbolsForM32 := []string{
					"-DUCHAR_MAX=0xFFU",
					"-DSCHAR_MAX=0x7F",
					"-DUSHRT_MAX=0xFFFFU",
					"-DSHRT_MAX=0x7FFF",
					"-DUINT_MAX=0xFFFFFFFFU",
					"-DINT_MAX=0x7FFFFFFF",
					"-DULONG_MAX=0xFFFFFFFFU",
					"-DLONG_MAX=0x7FFFFFFF"}
				preprocessorSymbols = append(preprocessorSymbols, preprocessorSymbolsForM32...)
			}
		}
	}
	return preprocessorSymbols
}

func ParseMachineDependentOptions(action BuildAction) []string {
	machineDependentOptions := make([]string, 0)
	for _, arg := range action.Command.Arguments {
		if strings.HasPrefix(arg, "-m") {
			machineDependentOptions = append(machineDependentOptions, arg)
		}
	}
	return machineDependentOptions
}

func ParseGccPredefinedMacros(gccPredefinedMacros string, needExtraArg bool) []string {
	if gccPredefinedMacros != "" {
		if needExtraArg { // For CSA with `-cc1`
			args := []string{}
			marcos := strings.Fields(gccPredefinedMacros)
			for idx := range marcos {
				if strings.HasPrefix(marcos[idx], "-D") {
					if marcos[idx] == "-D" { // for args like `-D MARCONAME=VALUE`
						args = append(args, "--extra-arg", "-D"+marcos[idx+1])
					} else { // for args like `-DMARCONAME=VALUE`
						args = append(args, "--extra-arg", marcos[idx])
					}
				}
			}
			return args
		} else {
			return strings.Fields(gccPredefinedMacros)
		}
	} else {
		return []string{}
	}
}

func prepareCTUDir(resultsDir string) (string, error) {
	absResultsDir, err := filepath.Abs(resultsDir)
	if err != nil {
		return "", fmt.Errorf("filepath.Abs: %v", err)
	}
	ctuDir := filepath.Join(absResultsDir, "ctu-dir")
	err = os.RemoveAll(ctuDir)
	if err != nil {
		return "", fmt.Errorf("os.RemoveAll: %v", err)
	} else {
		// Clear the CTU-dir if the user turned on the collection phase.
		glog.Info("Previous CTU contents have been deleted.")
	}
	err = os.MkdirAll(ctuDir, os.ModePerm)
	if err != nil {
		return "", fmt.Errorf("os.MkdirAll: %v", err)
	}
	glog.Infof("Output will be stored to: %s. The CTU dir is %s.", resultsDir, ctuDir)
	return ctuDir, nil
}

func GenerateCTUExtraArguments(compileCommandsPath, resultsDir string, config *pb.CheckerConfiguration, checkProgress bool, lang string, numWorkers int32) error {
	compileCommands, err := compilecommand.ReadCompileCommandsFromFile(compileCommandsPath)
	if err != nil {
		return fmt.Errorf("compilecommand.ReadCompileCommandsFromFiles: %v", err)
	}
	if reflect.DeepEqual(*compileCommands, []compilecommand.CompileCommand{}) {
		return fmt.Errorf("empty compile database")
	}
	ctuDir, err := prepareCTUDir(resultsDir)
	if err != nil {
		return fmt.Errorf("prepareCTUDir: %v", err)
	}
	actions := GetBuildActionsFromCompileCommands(resultsDir,
		compileCommands, config, checkProgress, lang)
	if actions == nil {
		glog.Info("No analysis is required.\nThere were no compilation commands in the provided compilation database or all of them were skipped.")
		return nil
	}
	PerformPreAnalysis(ctuDir, actions, config, checkProgress, lang, numWorkers)
	return nil
}
