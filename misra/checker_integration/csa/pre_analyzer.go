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

/*
The function PerformPreAnalysis() implements the preparation of on-demand
cross-translation-unit analysis.
See https://clang.llvm.org/docs/analyzer/user-docs/CrossTranslationUnit.html#on-demand-analysis.

This analysis produces AST of external TUs during analysis.
- The invocationList tells analyzer how to how to compile an external TU to get
  the AST.
- The index， a map<USR, filename>, tells analyzer which external TU need to be
  compiled.
*/

package csa

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode"

	"github.com/golang/glog"
	"gopkg.in/yaml.v2"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/cruleslib/i18n"
	"naive.systems/analyzer/cruleslib/stats"
	"naive.systems/analyzer/misra/utils"
)

var (
	generateInvocationListLock sync.Mutex
	performPreAnalysisLock     sync.Mutex
)

// Generate a standardized and cleaned compile command serving as a base for other operations.
// Refer to codechecker/analyzer/codechecker_analyzer/analyzers/clangsa/ctu_triple_arch.py:get_compile_command
func getCompileCommand(action *csaBuildAction) ([]string, error) {
	clangBin, err := exec.LookPath("clang")
	if err != nil {
		return nil, fmt.Errorf("exec.LookPath: %v", err)
	}
	cmd := []string{clangBin}
	if action.compilerTarget != "" {
		cmd = append(cmd, "--target="+action.compilerTarget)
	}
	for _, param := range action.compilerIncludes {
		cmd = append(cmd, "-isystem", param)
	}
	cmd = append(cmd, "-c")
	cmd = append(cmd, "-x", action.lang)
	cmd = append(cmd, action.analyzerOptions...)
	if action.source != "" {
		cmd = append(cmd, action.source)
	}
	flag := false
	for _, x := range cmd {
		if strings.HasPrefix(x, "-std") || strings.HasPrefix(x, "--std") {
			flag = true
		}
	}
	if !flag {
		cmd = append(cmd, action.compilerStandard)
	}
	return cmd, nil
}

// Generates the invocation for the source file of the current compilation command.
// The invocation list is a mapping from absolute paths of the source files to the parts of
// the compilation commands that are used to generate from each one of them.
func generateInvocationList(action *csaBuildAction, ctuDir string, config *pb.CheckerConfiguration) error {
	tripleArchDir := filepath.Join(ctuDir, action.arch)
	err := os.MkdirAll(tripleArchDir, os.ModePerm)
	if err != nil {
		return fmt.Errorf("os.MkdirAll: %v", err)
	}
	invocationList := filepath.Join(tripleArchDir, "invocation-list.yml")
	cmd, err := getCompileCommand(action)
	if err != nil {
		return fmt.Errorf("getCompileCommand: %v", err)
	}
	cmd = append(cmd, "-D__clang_analyzer__", "-w")
	// Append CSA System Lib Options & GCC Predefined Macros to the end of each command.
	cmd = append(cmd, strings.Fields(config.CsaSystemLibOptions)...)
	cmd = append(cmd, ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), false)...)
	invocationLine, err := yaml.Marshal(map[string][]string{action.source: cmd})
	if err != nil {
		glog.Errorf("Cannot generate invocationLine of %s.", action.source)
		return fmt.Errorf("yaml.Marshal: %v", err)
	}
	fd, err := os.OpenFile(invocationList, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		return fmt.Errorf("os.OpenFile: %v", err)
	}
	defer fd.Close()
	n, err := fd.Write(invocationLine)
	if err != nil {
		return fmt.Errorf("fd.Write: %v", err)
	}
	if n < len(invocationLine) {
		return io.ErrShortWrite
	}
	return nil
}

// Get command to create CTU index file.
func getExtdefMappingCmd(action *csaBuildAction, clangExtdefBin string) (*exec.Cmd, error) {
	cmd, err := getCompileCommand(action)
	if err != nil {
		return nil, fmt.Errorf("getCompileCommand: %v", err)
	}
	mapCmd := exec.Command(clangExtdefBin, action.source, "--")
	mapCmd.Args = append(mapCmd.Args, cmd[1:]...)
	return mapCmd, nil
}

// Turns textual function map list with source files into a mapping from mangled names to mapped paths.
// Refer to codechecker/analyzer/codechecker_analyzer/analyzers/clangsa/ctu_manager.py:func_map_list_src_to_ast.
//
// This function parses stdout of clang-extdef-mapping, e.g.
//
//	9:c:@F@foo# /path/to/your/project/foo.cpp
//	...
//
// This stdout is created from createCrossTUIndexString(Index),
// See third_party/llvm-project/clang/include/clang/CrossTU/CrossTranslationUnit.h.
//
// Index is a map<lookupName, filepathName> like
//
//	{"c:@F@foo#": "/path/to/your/project/foo.cpp", ...}
//
// The key is created from generateUSRForDecl(), length:USR,
// See third_party/llvm-project/clang/include/clang/Index/USRGeneration.h.
//
// Return funcAstList: map<USR, []filepathName>.
func funcMapListSrcToAst(funcSrcList []string) (map[string][]string, error) {
	funcAstList := make(map[string][]string)
	for _, funcSrcTxt := range funcSrcList {
		// Refer to the lastest commit of CodeChecker:
		// https://github.com/Ericsson/codechecker/commit/3ecc3e39f2bb7672c8264db5ed6f76712d8659bf
		var mangledName string
		var astFile string
		if unicode.IsDigit(rune(funcSrcTxt[0])) {
			lengthStr := strings.Split(funcSrcTxt, ":")[0]
			length, err := strconv.Atoi(lengthStr)
			if err != nil {
				return funcAstList, fmt.Errorf("strconv.Atoi: %v", err)
			}
			sepPos := len(lengthStr) + 1 + length
			mangledName = funcSrcTxt[0:sepPos]
			astFile = funcSrcTxt[sepPos+1:] // Skipping the ' ' separator
		} else {
			dpos := strings.Index(funcSrcTxt, " ")
			if dpos != -1 {
				mangledName = funcSrcTxt[0:dpos]
				astFile = funcSrcTxt[dpos+1:]
			} else {
				return funcAstList, errors.New("Cannot get mappings from " + funcSrcTxt)
			}
		}
		funcAstList[mangledName] = append(funcAstList[mangledName], astFile)
	}
	return funcAstList, nil
}

// Generate function map file for the current source.
// On-demand CTU analysis requires the *mangled name* to *source file* mapping.
func generateExtdefMappings(action *csaBuildAction, ctuDir string, clangExtdefBin string) (map[string][]string, error) {
	cmd, err := getExtdefMappingCmd(action, clangExtdefBin)
	if err != nil {
		return nil, fmt.Errorf("getExtdefMappingCmd: %v", err)
	}
	funcSrcList, err := utils.GetCommandStdoutLines(cmd, action.directory)
	if err != nil {
		glog.Errorf("Errors occur when executing: %s", cmd.String())
		glog.Errorf("The stdout and stderr is %s", strings.Join(funcSrcList, "\n"))
		return nil, fmt.Errorf("utils.GetCommandStdoutLines: %v", err)
	}
	funcAstList, err := funcMapListSrcToAst(funcSrcList)
	if err != nil {
		return nil, fmt.Errorf("funcMapListSrcToAst: %v", err)
	}
	return funcAstList, nil
}

// Find arch following "-triple" in the stdout of command.
// e.g. x86_64 in stdout with "-triple" "x86_64-redhat-linux"
// Refer to codechecker_analyzer/analyzers/clangsa/ctu_triple_arch.py:_find_arch_in_command.
func findArchInCommand(cmds []string, directory string) (string, error) {
	// cmds are like ["clang", "-###", args...]
	// outputTokens are like
	//	[`clang version 14.0.5 (Fedora 14.0.5-1.fc36)`,
	//	`Target: x86_64-redhat-linux-gnu`,
	//	`Thread model: posix`,
	//	`InstalledDir: /usr/bin`,
	//	` (in-process)`,
	//	`"/usr/bin/clang-14" "-cc1" "-triple" "x86_64-redhat-linux-gnu" ...`]
	outputTokens, err := utils.GetCommandStdout(cmds, directory)
	if err != nil {
		// The error may not affect the parsing of arch, so do not return it here.
		glog.Warningf("This error may not affect the parsing after. getCommandStdout: %v", err)
	}
	if outputTokens == nil {
		return "", fmt.Errorf("utils.GetCommandStdout: %v", err)
	}
	archInTriple := ""
	for idx, token := range outputTokens {
		if token == "\"-triple\"" {
			tripleToken := outputTokens[idx+1]
			tripleToken = strings.TrimPrefix(tripleToken, "\"")
			tripleToken = strings.TrimSuffix(tripleToken, "\"")
			archInTriple = strings.Split(tripleToken, "-")[0]
		}
	}
	if archInTriple == "" {
		glog.Errorf("Failed to get archInTriple by executing: %s", strings.Join(cmds, " "))
		glog.Errorf("The stdout and stderr is %s", strings.Join(outputTokens, " "))
		return "", fmt.Errorf("utils.GetCommandStdout: %v", err)
	}
	return archInTriple, nil
}

// Returns the architecture part of the target triple for the given compilation command.
// Refer to codechecker_analyzer/analyzers/clangsa/ctu_triple_arch.py:get_triple_arch.
func getTripleArch(action *csaBuildAction) (string, error) {
	cmds, err := getCompileCommand(action)
	if err != nil {
		return "", fmt.Errorf("getCompileCommand: %v", err)
	}
	// Insert '-###' flag into invocation to emit the commands of substeps in a build process.
	cmds = append(cmds[:2], cmds[1:]...)
	cmds[1] = "-###"
	archInTriple, err := findArchInCommand(cmds, action.directory)
	if err != nil {
		return "", fmt.Errorf("findArchInCommand: %v", err)
	}
	return archInTriple, nil
}

// Refer to codechecker/analyzer/tools/merge_clang_extdef_mappings/codechecker_merge_clang_extdef_mappings/
// merge_clang_extdef_mappings.py:_create_global_ctu_function_map
func createGlobalCTUFunctionMap(ctuDir string, mergedFuncAstListByArch *map[string]map[string][]string) error {
	for tripleArch, mergedFuncAstList := range *mergedFuncAstListByArch {
		for mangledName, astFiles := range mergedFuncAstList {
			// As the key in extdef mapping, one mangled name mapping to multiple AST files is invalid.
			// Therefore, skip items with multiple AST files, which will cause an error in the following analysis phase,
			// 'multiple definitions are found for the same key in index'.
			if len(astFiles) != 1 {
				continue
			}
			mangledAstPair := mangledName + " " + astFiles[0]
			// Save each mangledAstPair by its arch.
			if tripleArch == "" {
				return fmt.Errorf("Arch for %s in %s does not exist.", mangledName, astFiles[0])
			}
			mappingDir := filepath.Join(ctuDir, tripleArch)
			if _, err := os.Stat(mappingDir); os.IsNotExist(err) {
				glog.Infof("The externalDefMap dir for arch %s does not exist. Create one.", tripleArch)
			}
			err := os.MkdirAll(mappingDir, os.ModePerm)
			if err != nil {
				return fmt.Errorf("os.MkdirAll: %v", err)
			}
			ctuFuncMapFile := filepath.Join(ctuDir, tripleArch, "externalDefMap.txt")
			fd, err := os.OpenFile(ctuFuncMapFile, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
			if err != nil {
				return fmt.Errorf("os.OpenFile: %v", err)
			}
			defer fd.Close()
			n, err := fd.WriteString(mangledAstPair + "\n")
			if err != nil {
				return fmt.Errorf("fd.WriteString: %v", err)
			} else if n < len(mangledAstPair) {
				return fmt.Errorf("fd.WriteString: %v", io.ErrShortWrite)
			}
		}
	}
	return nil
}

func getFuncAstListByArch(
	actions <-chan csaBuildAction,
	arches chan<- string,
	results chan<- map[string][]string,
	ctuDir string,
	config *pb.CheckerConfiguration,
) {
	for action := range actions {
		if action.arch == "" {
			archInTriple, err := getTripleArch(&action)
			if err != nil {
				glog.Errorf("getTripleArch: %v", err)
			}
			action.arch = archInTriple
		}
		generateInvocationListLock.Lock()
		err := generateInvocationList(&action, ctuDir, config)
		generateInvocationListLock.Unlock()
		if err != nil {
			glog.Errorf("generateInvocationList: %v", err)
		}
		var funcAstList map[string][]string
		funcAstList, err = generateExtdefMappings(&action, ctuDir, config.ClangmappingBin)
		if err != nil {
			glog.Errorf("generateExtdefMappings: %v", err)
		}
		results <- funcAstList
		arches <- action.arch
	}
}

// Refer to codechecker/analyzer/codechecker_analyzer/pre_analysis_manager.py
func PerformPreAnalysis(ctuDir string, actions *[]csaBuildAction, config *pb.CheckerConfiguration, checkProgress bool, lang string, numWorkers int32) {
	printer := i18n.GetPrinter(lang)
	start := time.Now()
	var finishedTaskNumbers int32 = 0
	taskNumbers := len(*actions)
	// Three dimensions are arch, mangledName, astFiles.
	mergedFuncAstListByArch := make(map[string]map[string][]string)

	tasks := make(chan csaBuildAction, numWorkers)
	arches := make(chan string, taskNumbers)
	results := make(chan map[string][]string, taskNumbers)
	if checkProgress {
		basic.PrintfWithTimeStamp(printer.Sprintf("Starting generating CTU infomation"))
	}
	for i := 0; i < int(numWorkers); i++ {
		go getFuncAstListByArch(tasks, arches, results, ctuDir, config)
	}
	for _, action := range *actions {
		tasks <- action
	}
	close(tasks)
	for i := 0; i < taskNumbers; i++ {
		funcAstList := <-results
		arch := <-arches
		performPreAnalysisLock.Lock()
		for name, astFiles := range funcAstList {
			if _, exist := mergedFuncAstListByArch[arch]; !exist {
				mergedFuncAstListByArch[arch] = make(map[string][]string)
				mergedFuncAstListByArch[arch][name] = astFiles
			} else {
				mergedFuncAstListByArch[arch][name] = append(mergedFuncAstListByArch[arch][name], astFiles...)
			}
		}
		performPreAnalysisLock.Unlock()
		finishedTaskNumbers++
		if checkProgress {
			percent := basic.GetPercentString(int(finishedTaskNumbers), taskNumbers)
			stats.WriteProgress(filepath.Dir(ctuDir), stats.CTU, percent, start)
			percent += "%"
			basic.PrintfWithTimeStamp(printer.Sprintf("%s CTU information generated (%v/%v)", percent, finishedTaskNumbers, taskNumbers))
		}
	}

	elapsed := time.Since(start)
	if checkProgress {
		timeUsed := basic.FormatTimeDuration(elapsed)
		basic.PrintfWithTimeStamp(printer.Sprintf("CTU information generated successfully [%s]", timeUsed))
		basic.PrintfWithTimeStamp(printer.Sprintf("Starting writing CTU information"))
	}
	mergeStart := time.Now()
	err := createGlobalCTUFunctionMap(ctuDir, &mergedFuncAstListByArch)
	if err != nil {
		glog.Errorf("createGlobalCTUFunctionMap: %v", err)
	}
	// TODO: perform pre-analysis in a multi-processing way
	mergeElapsed := time.Since(mergeStart)
	if checkProgress {
		timeUsed := basic.FormatTimeDuration(mergeElapsed)
		basic.PrintfWithTimeStamp(printer.Sprintf("Writing CTU information completed [%s]", timeUsed))
	}
}
