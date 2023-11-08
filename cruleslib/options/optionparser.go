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
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"

	"github.com/golang/glog"
	"google.golang.org/protobuf/encoding/protojson"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cpumem"
	"naive.systems/analyzer/cruleslib/basic"
	"naive.systems/analyzer/cruleslib/i18n"
	"naive.systems/analyzer/cruleslib/stats"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
)

var supportProjectType = map[string]analyzerinterface.ProjectType{"makefile": analyzerinterface.Make, "cmake": analyzerinterface.CMake, "keil": analyzerinterface.Keil, "qmake": analyzerinterface.QMake, "script": analyzerinterface.Script}
var supportIncludeStddef = map[string]bool{"pure": true, "armgcc": true, "none": true}

func ConcatStringField(concatString string, stringField *string) *string {
	if stringField == nil {
		return &concatString
	}
	*stringField = concatString + " " + *stringField
	return stringField
}

func ParseProjectType(sharedOptions *SharedOptions) (analyzerinterface.ProjectType, error) {
	projType, exist := supportProjectType[sharedOptions.GetProjectType()]
	if !exist {
		return projType, fmt.Errorf("unsupported project type: %v", sharedOptions.ProjectType)
	}
	if _, exist := supportIncludeStddef[sharedOptions.GetIncludeStddef()]; !exist {
		return projType, fmt.Errorf("unsupported include stddef method: %v", sharedOptions.IncludeStddef)
	}
	return projType, nil
}

func ProcessKeilProject(sharedOptions *SharedOptions) {
	// Process keil project
	convertCmd := exec.Command("python", "converter.py", "uvprojx", sharedOptions.GetSrcDir())
	convertCmd.Dir = "/ProjectConverter/"
	glog.Info("executing: ", convertCmd.String())
	if err := convertCmd.Run(); err != nil {
		glog.Fatal(err)
		os.Exit(1)
	}
	cmd_arr := []string{sharedOptions.GetSrcDir(), "-DCMAKE_CXX_COMPILER=" + sharedOptions.GetCmakeCXXCompiler()}
	if sharedOptions.GetCmakeExportCompileCommands() {
		cmd_arr = append(cmd_arr, "-DCMAKE_EXPORT_COMPILE_COMMANDS=on")
	}
	cmd := exec.Command("cmake", cmd_arr...)
	tempDir, err := os.MkdirTemp("/tmp/", "")
	if err != nil {
		glog.Fatal(err)
		os.Exit(1)
	}
	cmd.Dir = tempDir
	fmt.Println(cmd.String())
	glog.Info("executing: ", cmd.String())
	err = analyzerinterface.PrintCmdOutput(cmd)
	if err != nil {
		glog.Fatal(err)
		os.Exit(1)
	}

	src := filepath.Join(tempDir, "compile_commands.json")
	dst := filepath.Join(sharedOptions.GetSrcDir(), "compile_commands.json")
	fin, err := os.Open(src)
	if err != nil {
		glog.Fatal(err)
		os.Exit(1)
	}
	defer fin.Close()
	fout, err := os.Create(dst)
	if err != nil {
		glog.Fatal(err)
		os.Exit(1)
	}
	defer fout.Close()
	_, err = io.Copy(fout, fin)
	if err != nil {
		glog.Fatal(err)
		os.Exit(1)
	}
}

func getGccLibPath(osType, osVersion string) string {
	var gccLib string
	switch osType {
	case "ubuntu":
		switch osVersion {
		case "18.04":
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/8/include"
		case "20.04":
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/9/include"
		case "22.04":
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"
		case "23.04":
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/12/include"
		default:
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"
		}
	case "debian":
		switch osVersion {
		case "11":
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/10/include"
		default:
			gccLib = "-isystem /usr/lib/gcc/x86_64-linux-gnu/10/include"
		}
	case "almalinux":
		switch osVersion {
		case "8.7":
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/8/include"
		case "9.1":
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/11/include"
		default:
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/11/include"
		}
	case "centos":
		switch osVersion {
		case "7":
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/4.8.5/include"
		default:
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/4.8.5/include"
		}
	case "fedora":
		switch osVersion {
		case "35":
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/11/include"
		case "36", "37":
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/12/include"
		default:
			gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/12/include"
		}
	default:
		gccLib = "-isystem /usr/lib/gcc/x86_64-redhat-linux/12/include"
	}
	return gccLib
}

func getClangLibPath(osType, osVersion string) string {
	var clangLib string
	switch osType {
	case "ubuntu":
		switch osVersion {
		case "18.04":
			clangLib = "-isystem /usr/lib/clang/6.0/include"
		case "20.04":
			clangLib = "-isystem /usr/lib/clang/10/include"
		case "22.04":
			clangLib = "-isystem /usr/lib/clang/14/include"
		case "23.04":
			clangLib = "-isystem /usr/lib/clang/15/include"
		default:
			clangLib = "-isystem /usr/lib/clang/14/include"
		}
	case "debian":
		switch osVersion {
		case "11":
			clangLib = "-isystem /usr/lib/clang/11/include"
		default:
			clangLib = "-isystem /usr/lib/clang/11/include"
		}
	case "almalinux":
		switch osVersion {
		case "8.7", "9.1":
			clangLib = "-isystem /usr/lib64/clang/14/include"
		default:
			clangLib = "-isystem /usr/lib64/clang/14/include"
		}
	case "centos":
		switch osVersion {
		case "7":
			clangLib = "-isystem /usr/lib/clang/3.4.2/include"
		default:
			clangLib = "-isystem /usr/lib/clang/3.4.2/include"
		}
	case "fedora":
		switch osVersion {
		case "35":
			clangLib = "-isystem /usr/lib64/clang/13/include"
		case "36":
			clangLib = "-isystem /usr/lib64/clang/14/include"
		case "37":
			clangLib = "-isystem /usr/lib64/clang/15/include"
		default:
			clangLib = "-isystem /usr/lib64/clang/14/include"
		}
	default:
		clangLib = "-isystem /usr/lib64/clang/14/include"
	}
	return clangLib
}

func getArmgccLibPath(osType string) string {
	var armgccLib string
	switch osType {
	case "ubuntu":
		armgccLib = "-isystem /usr/lib/arm-none-eabi/include"
	case "fedora":
		armgccLib = "-isystem /usr/arm-none-eabi/include"
	default:
		armgccLib = "-isystem /usr/arm-none-eabi/include"
	}
	return armgccLib
}

// TODO: deprecated checker config
func ParseCheckerConfig(sharedOptions *SharedOptions, numWorkers int32, cpplines int, projType analyzerinterface.ProjectType) *pb.CheckerConfiguration {
	parsedCheckerConfig := &pb.CheckerConfiguration{}
	err := protojson.Unmarshal([]byte(sharedOptions.GetCheckerConfig()), parsedCheckerConfig)
	if err != nil {
		glog.Fatal("parsing checker config: ", err)
	}
	// replace parsedCheckerConfig if some field set by standalone options
	if sharedOptions.GetClangBin() != "" {
		parsedCheckerConfig.ClangBin = sharedOptions.GetClangBin()
	}
	if sharedOptions.GetClangmappingBin() != "" {
		parsedCheckerConfig.ClangmappingBin = sharedOptions.GetClangmappingBin()
	}
	if sharedOptions.GetClangqueryBin() != "" {
		parsedCheckerConfig.ClangqueryBin = sharedOptions.GetClangqueryBin()
	}
	if sharedOptions.GetClangtidyBin() != "" {
		parsedCheckerConfig.ClangtidyBin = sharedOptions.GetClangtidyBin()
	}
	if sharedOptions.GetCodeCheckerBin() != "" {
		parsedCheckerConfig.CodeCheckerBin = sharedOptions.GetCodeCheckerBin()
	}
	if sharedOptions.GetCppcheckBin() != "" {
		parsedCheckerConfig.CppcheckBin = sharedOptions.GetCppcheckBin()
	}
	if sharedOptions.GetCsaSystemLibOptions() != "" {
		parsedCheckerConfig.CsaSystemLibOptions = sharedOptions.GetCsaSystemLibOptions()
	}
	if sharedOptions.GetGccPredefinedMacros() != "" {
		parsedCheckerConfig.GccPredefinedMacros = sharedOptions.GetGccPredefinedMacros()
	}
	if sharedOptions.GetInferBin() != "" {
		parsedCheckerConfig.InferBin = sharedOptions.GetInferBin()
	}
	if sharedOptions.GetInferExtraOptions() != "" {
		parsedCheckerConfig.InferExtraOptions = sharedOptions.GetInferExtraOptions()
	}
	if sharedOptions.GetInferJobs() > 0 {
		parsedCheckerConfig.InferJobs = sharedOptions.GetInferJobs()
	}
	if sharedOptions.GetMisraCheckerPath() != "" {
		parsedCheckerConfig.MisraCheckerPath = sharedOptions.GetMisraCheckerPath()
	}
	if sharedOptions.GetPythonBin() != "" {
		parsedCheckerConfig.PythonBin = sharedOptions.GetPythonBin()
	}

	if parsedCheckerConfig.InferJobs > int64(numWorkers) {
		parsedCheckerConfig.InferJobs = int64(numWorkers)
	}
	parsedCheckerConfig.NumWorkers = numWorkers

	if projType == analyzerinterface.Keil {
		// TODO: remove gcc_predefined_macros hack for clang related checkers
		parsedCheckerConfig.GccPredefinedMacros = *ConcatStringField("-D__GNUC__", &parsedCheckerConfig.GccPredefinedMacros)
	}

	// -D__GNUC__ can be added manually or determined by project type
	osType, osVersion, err := basic.GetOperatingSystemType()
	if err != nil {
		glog.Errorf("getOperatingSystemType: %v", err)
	}
	if parsedCheckerConfig.GccPredefinedMacros != "" && strings.Contains(parsedCheckerConfig.GccPredefinedMacros, "-D__GNUC__") {
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField(getGccLibPath(osType, osVersion), &parsedCheckerConfig.CsaSystemLibOptions)
	} else {
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField(getClangLibPath(osType, osVersion), &parsedCheckerConfig.CsaSystemLibOptions)
	}
	if sharedOptions.GetIncludeStddef() == "pure" {
		parsedCheckerConfig.GccPredefinedMacros = *ConcatStringField("-include stddef.h", &parsedCheckerConfig.GccPredefinedMacros)
	} else if sharedOptions.GetIncludeStddef() == "armgcc" {
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField(getArmgccLibPath(osType), &parsedCheckerConfig.CsaSystemLibOptions)
	}

	if cpplines > 0 {
		// add libstdc++ header path
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField("-isystem /usr/include/c++/12", &parsedCheckerConfig.CsaSystemLibOptions)
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField("-isystem /usr/include/c++/12/x86_64-redhat-linux", &parsedCheckerConfig.CsaSystemLibOptions)
		// TODO: The following options is added for qt5 project, need remove it in future.
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField(
			"-isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtCore -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtXml",
			&parsedCheckerConfig.CsaSystemLibOptions)
		parsedCheckerConfig.CsaSystemLibOptions = *ConcatStringField("-D__linux__", &parsedCheckerConfig.CsaSystemLibOptions)
	}
	return parsedCheckerConfig
}

func CheckCodeLines(compileCommandsPath string, sharedOptions *SharedOptions, linesLimitStr string) (int, int, error) {
	printer := i18n.GetPrinter(sharedOptions.GetLang())
	clines, err := analyzerinterface.CountLines(compileCommandsPath, []string{"C"}, sharedOptions.GetIgnoreDirPatterns())
	if err != nil {
		return clines, 0, fmt.Errorf("failed to check c lines: %v", err)
	}
	cpplines, err := analyzerinterface.CountLines(compileCommandsPath, []string{"C++"}, sharedOptions.GetIgnoreDirPatterns())
	if err != nil {
		return clines, cpplines, fmt.Errorf("failed to check cpp lines: %v", err)
	}
	headerlines, err := analyzerinterface.CountLinesUnderDir([]string{sharedOptions.GetSrcDir()}, []string{"C Header", "C++ Header"}, sharedOptions.GetIgnoreDirPatterns())
	if err != nil {
		return clines, cpplines, fmt.Errorf("failed to check header lines: %v", err)
	}
	if cpplines == 0 && clines == 0 && headerlines == 0 {
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of C code", clines))
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of C++ code", cpplines))
		return clines, cpplines, fmt.Errorf("nothing to analyze, please check %s", sharedOptions.GetSrcDir())
	}
	linesLimit, err := strconv.Atoi(linesLimitStr)
	if err != nil {
		return clines, cpplines, fmt.Errorf("invalid lines limit: %v", err)
	}
	if linesLimit != 0 && (clines+cpplines+headerlines) > linesLimit {
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of C code", clines))
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of C++ code", cpplines))
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of headers", headerlines))
		return clines, cpplines, fmt.Errorf("exceed maximum limit %d", linesLimit)
	}
	if sharedOptions.GetCheckProgress() && sharedOptions.GetShowLineNumber() {
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of C code", clines))
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of C++ code", cpplines))
		basic.PrintfWithTimeStamp(printer.Sprintf("%d lines of headers", headerlines))
	}
	stats.WriteLOC(sharedOptions.GetResultsDir(), clines+cpplines+headerlines)
	return clines, cpplines, nil
}

func ParseLimitMemory(sharedOptions *SharedOptions, numWorkersStr string) (int32, error) {
	num_workers, err := strconv.ParseInt(numWorkersStr, 10, 32)
	if err != nil {
		return int32(num_workers), fmt.Errorf("invalid number of workers: %v", err)
	}
	numWorkers := int32(num_workers)
	if numWorkers == 0 {
		numWorkers = int32(runtime.NumCPU())
	}
	if sharedOptions.GetLimitMemory() && sharedOptions.GetAvailMemRatio() >= 0 {
		err := basic.InitCgroup()
		if err != nil {
			return numWorkers, fmt.Errorf("failed to create cgroup for misra_analyzer: %v", err)
		}
		totalAvailMem, err := basic.GetTotalAvailMem()
		if err != nil {
			return numWorkers, fmt.Errorf("failed to get available memory: %v", err)
		}
		cpumem.Init(int(numWorkers), int(float64(totalAvailMem)*sharedOptions.GetAvailMemRatio()))
	} else {
		cpumem.Init(int(numWorkers), 0)
		sharedOptions.SetLimitMemory(false)
	}
	return numWorkers, nil
}
