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

package misra

import (
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/misra/checker_integration/csa"
	"naive.systems/analyzer/misra/utils"
)

// NOTE(tianhaoyu): equivalent to map[string](stringSet)
// struct{} doesn't require any additional space, and supports efficient lookup
type CallGraph map[string](map[string]struct{})

// Add (caller, callee)-pair into callgraph
func (callgraph CallGraph) AddCallerCallee(caller, callee string) {
	_, callerExist := callgraph[caller]
	if !callerExist {
		callgraph[caller] = make(map[string]struct{})
	}
	callgraph[caller][callee] = struct{}{}
}

// Add (caller, callees)-pairs into callgraph
func (callgraph CallGraph) AddCallerCallees(caller string, callees []string) {
	if caller == "" {
		return
	}
	for _, callee := range callees {
		if callee == "" {
			continue
		}
		if caller == "< root >" {
			caller = "@root" // @root's incoming degree is always zero
		}
		callgraph.AddCallerCallee(string(caller), string(callee))
	}
}

// Get all call-circles' paths of a callgraph, using @root
func (callgraph CallGraph) GetCircles() [][]string {
	_, hasRoot := callgraph["@root"]
	if !hasRoot {
		return nil
	}
	result := make([][]string, 0)
	result = append(result, utils.RecursiveTarjanSCC((*map[string](map[string]struct{}))(&callgraph))...)
	// SCC wouldn't handle self-loops, we manually check it.
	for nodeU := range callgraph {
		for nodeV := range callgraph[nodeU] {
			if nodeU == nodeV {
				result = append(result, []string{nodeU})
			}
		}
	}
	return result
}

// Get CallGraph of source files
func GetCallGraph(compileCommandsPath, checkRule, resultsDir string, buildActions *[]csa.BuildAction, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) (CallGraph, error) {
	callgraph := make(CallGraph, 0)
	systemLibOptions := config.CsaSystemLibOptions // TODO: remove this config path hack
	taskName := filepath.Base(resultsDir)
	reportDir := filepath.Join(resultsDir, "csa-out")
	err := os.MkdirAll(reportDir, os.ModePerm)
	if err != nil {
		return nil, err
	}
	reportPath := filepath.Join(reportDir, "report.json")
	// regexp for `Function: Foo calls: bar1 bar2`
	re := regexp.MustCompile(`Function: ([<> a-zA-Z_\d]*) calls:([a-zA-Z _\d]*)`)
	for _, action := range *buildActions {
		entranceFile := action.Command.File
		_, err := os.Stat(entranceFile)
		if err != nil {
			if os.IsNotExist(err) {
				glog.Infof("in %s: %s does not exist", taskName, entranceFile)
			}
			continue
		}

		// resultsDir: /output/tmp/misra_c_2012/rule_17_2-*/
		// ctuDir: /output/ctu-dir/ARCH/
		tmpDir := filepath.Dir(filepath.Dir(resultsDir))
		ctuDir := filepath.Join(filepath.Dir(tmpDir), "ctu-dir", action.Arch)

		if !filepath.IsAbs(ctuDir) {
			absCTUDir, err := filepath.Abs(ctuDir)
			if err != nil {
				glog.Errorf("in %s: failed to convert ctuDir %s to abspath: %v", taskName, ctuDir, err)
			} else {
				ctuDir = absCTUDir
			}
		}

		cmd_args := []string{"--analyze", "--analyzer-no-default-checks", "--analyzer-output", "sarif",
			"-o", reportPath, entranceFile,
			"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "experimental-enable-naive-ctu-analysis=true",
			"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "ctu-dir=" + ctuDir,
			"-Xanalyzer", "-analyzer-config", "-Xanalyzer", "ctu-invocation-list=" + ctuDir + "/invocation-list.yml"}
		if action.Target != "" {
			cmd_args = append(cmd_args, "-target", action.Target)
		}
		// enable/disable external solver z3
		cmd_args = append(cmd_args, "-Xanalyzer", "-analyzer-config", "-Xanalyzer", "crosscheck-with-z3=true")
		preprocessorSymbols := csa.ParsePreprocessorSymbols(action, true, true)
		cmd_args = append(cmd_args, preprocessorSymbols...)
		cmd_args = append(cmd_args, strings.Fields(systemLibOptions)...)
		cmd_args = append(cmd_args, "-Xanalyzer", "-analyzer-checker=debug.DumpCallGraph")
		cmd_args = append(cmd_args, csa.ParseGccPredefinedMacros(config.GetGccPredefinedMacros(), false)...)

		cmd := exec.Command(config.ClangBin, cmd_args...)
		cmd.Dir = action.Command.Directory
		glog.Infof("in %s, executing: %s", taskName, cmd.String())
		out, err := basic.CombinedOutput(cmd, taskName, limitMemory, timeoutNormal, timeoutOom)
		if err != nil {
			outStr := string(out)
			glog.Errorf("in %s, executing: %s, reported:\n%s", taskName, cmd.String(), outStr)
			return nil, err
		}
		matches := re.FindAllStringSubmatch(string(out), -1)
		for _, match := range matches {
			callgraph.AddCallerCallees(match[1], strings.Split(match[2], " "))
		}
	}
	return callgraph, nil
}
