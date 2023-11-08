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

package analyzerinterface

import (
	"bufio"
	"bytes"
	"crypto/sha1"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"time"

	"github.com/bmatcuk/doublestar/v4"
	"github.com/golang/glog"
	"github.com/google/uuid"
	"github.com/hhatto/gocloc"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/filter"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra/checker_integration/compilecommand"
	"naive.systems/analyzer/misra/utils"
	"naive.systems/analyzer/rulesets"
	rs "naive.systems/analyzer/rulesets"
)

type ArrayFlags []string

const CCJson string = "compile_commands.json"
const ActualCCJson string = "actual_compile_commands.json"

func (i *ArrayFlags) String() string {
	return "array flags"
}

func (i *ArrayFlags) Set(value string) error {
	*i = append(*i, value)
	return nil
}

type ProjectType int

const (
	Other  ProjectType = 0
	CMake  ProjectType = 1
	Make   ProjectType = 2
	Keil   ProjectType = 3
	QMake  ProjectType = 4
	Script ProjectType = 5
)

func PrintCmdOutput(cmd *exec.Cmd) error {
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err := cmd.Start()
	if err != nil {
		return err
	}
	return cmd.Wait()
}

func createFakeCCJson(srcdir, compileCommandsPath string) {
	var cppFiles []string
	err := filepath.Walk(srcdir, func(path string, info os.FileInfo, err error) error {
		// Check for errors during traversal.
		if err != nil {
			return err
		}
		// Ignore directories.
		if info.IsDir() {
			return nil
		}
		// Check if file has cpp extension.
		if filter.IsCCFile(path) {
			if !strings.HasPrefix(path, "/src/") {
				cppFiles = append(cppFiles, filepath.Join(srcdir, path))
			} else {
				cppFiles = append(cppFiles, path)
			}
		}
		return nil
	})
	if err != nil {
		glog.Fatalf("createFakeCCJson: Failed to walk src dir %v", err)
	}

	var commands []compilecommand.CompileCommand
	for _, file := range cppFiles {
		var command compilecommand.CompileCommand
		command.File = file
		command.Directory = srcdir
		commands = append(commands, command)
	}
	content, err := json.Marshal(&commands)
	if err != nil {
		glog.Fatalf("createFakeCCJson: Failed to marshal command list %v", err)
	}
	err = os.WriteFile(compileCommandsPath, content, os.ModePerm)
	if err != nil {
		glog.Fatalf("createFakeCCJson: Failed to write ccjson file %v", err)
	}
}

func isEmptyFileContent(filePath string) bool {
	content, err := os.ReadFile(filePath)
	if err != nil {
		glog.Fatalf("isEmptyFileContent: failed to read ccjson file %v", err)
	}

	if strings.HasPrefix(string(content), "[]") {
		return true
	} else {
		return false
	}
}

func CheckCompilationDatabase(
	compileCommandsPath string,
	createNew bool,
	projType ProjectType,
	dir,
	qmakeBin,
	qtProPath,
	scriptContents string,
	allowBuildFail bool,
	isDev bool,
) (bool, error) {
	buildFailed := false
	if createNew {
		var err error
		var cc_exist bool
		switch projType {
		case Make:
			err, cc_exist = CreateCompilationDatabaseByMake(compileCommandsPath, dir)
		case CMake:
			err, cc_exist = CreateCompilationDatabaseByCMake(compileCommandsPath, dir, isDev)
		case QMake:
			err, cc_exist = CreateCompilationDatabaseByQMake(compileCommandsPath, dir, qmakeBin, qtProPath)
		case Script:
			err = CreateCompilationDatabaseByScript(dir, scriptContents)
		case Other:
			glog.Fatalf("no viable builder found in %s. Stop", dir)
		}
		if err != nil && projType != Script {
			if !allowBuildFail {
				glog.Fatalf("failed to create compilation database: %v", err)
			} else {
				glog.Errorf("failed to create compilation database: %v", err)
			}
			if !cc_exist || isEmptyFileContent(compileCommandsPath) {
				createFakeCCJson(dir, compileCommandsPath)
			}
			buildFailed = true
		}
	}

	_, err := os.Stat(compileCommandsPath)
	if err != nil {
		return buildFailed, err
	}

	glog.Info("compileCommandsPath ", compileCommandsPath)

	return buildFailed, nil
}

func SelectBuilder(srcdir string) ProjectType {
	cmakePath := filepath.Join(srcdir, "CMakeLists.txt")
	makePath := filepath.Join(srcdir, "Makefile")
	makePathLower := filepath.Join(srcdir, "makefile")
	if _, err := os.Stat(cmakePath); err == nil {
		return CMake
	} else if _, err := os.Stat(makePath); err == nil {
		return Make
	} else if _, err := os.Stat(makePathLower); err == nil {
		return Make
	} else {
		return Other
	}
}

func CreateCompilationDatabaseByMake(compileCommandsPath, dir string) (error, bool) {
	cmd := exec.Command("make", "clean")
	cmd.Dir = dir
	err := cmd.Run()
	if err != nil {
		glog.Warningf("failed to execute make clean: %v", err)
	}
	cmd = exec.Command("bear", "--output", compileCommandsPath, "--", "make")
	cmd.Dir = dir
	glog.Info("executing: ", cmd.String())
	err = PrintCmdOutput(cmd)
	cc_exist := true
	if err != nil {
		glog.Errorf("failed to execute make,: %v", err)
		_, stat_err := os.Stat(compileCommandsPath)
		if stat_err != nil {
			cc_exist = false
		}
		return err, cc_exist
	}
	return nil, cc_exist
}

func CreateCompilationDatabaseByCMake(compileCommandsPath, dir string, isDev bool) (error, bool) {
	args := []string{dir}
	if isDev {
		args = append(args, "-DCMAKE_EXPORT_COMPILE_COMMANDS=on")
	}
	cmd := exec.Command("cmake", args...)
	cc_exist := false
	if isDev {
		cmd.Dir = dir
	} else {
		tempDir, err := os.MkdirTemp("/tmp/", "")
		if err != nil {
			glog.Errorf("failed to create a temporary directory: %v", err)
			return err, cc_exist
		}
		cmd.Dir = tempDir
	}
	glog.Info("executing: ", cmd.String())
	fmt.Println(cmd.String())
	err := PrintCmdOutput(cmd)
	if err != nil {
		glog.Errorf("failed to execute cmake: %v", err)
		return err, cc_exist
	}

	if err, cc_exist = CreateCompilationDatabaseByMake(compileCommandsPath, cmd.Dir); err != nil {
		return err, cc_exist
	}
	return nil, cc_exist
}

func CreateCompilationDatabaseByQMake(compileCommandsPath, dir, qmakeBin, qtProPath string) (error, bool) {
	var cmd *exec.Cmd
	if qtProPath == "" {
		cmd = exec.Command(qmakeBin)
		cmd.Dir = dir
	} else {
		qtProPathAbs := filepath.Join(dir, qtProPath)
		cmd_arr := []string{"-makefile", qtProPathAbs}
		cmd = exec.Command(qmakeBin, cmd_arr...)
		cmd.Dir = dir
	}
	glog.Info("executing: ", cmd.String())
	err := PrintCmdOutput(cmd)
	if err != nil {
		glog.Errorf("failed to execute qmake: %v", err)
		return err, false
	}
	err, cc_exist := CreateCompilationDatabaseByMake(compileCommandsPath, dir)
	return err, cc_exist
}

func CreateCompilationDatabaseByScript(dir, scriptContents string) error {
	cmd := exec.Command("bash", "-c", scriptContents)
	cmd.Dir = dir
	err := PrintCmdOutput(cmd)
	if err != nil {
		glog.Errorf("failed to execute %s: %v", cmd.String(), err)
		return err
	}
	return nil
}

func GenerateActualCCJsonByRemoveIgnoreFiles(compileCommandsPath string, ignoreDirPatterns []string) (string, error) {
	compileCommands := []compilecommand.CompileCommand{}
	contents, err := os.ReadFile(compileCommandsPath)
	if err != nil {
		return "", err
	}
	err = json.Unmarshal(contents, &compileCommands)
	if err != nil {
		return "", err
	}
	actualCompileCommands := []compilecommand.CompileCommand{}
	for _, command := range compileCommands {
		if strings.HasSuffix(command.File, ".s") || strings.HasSuffix(command.File, ".S") ||
			strings.HasSuffix(command.Output, ".s") || strings.HasSuffix(command.Output, ".S") {
			continue
		}
		matched, err := MatchIgnoreDirPatterns(ignoreDirPatterns, command.File)
		if err != nil {
			glog.Error(err)
			continue
		}
		if matched {
			continue
		}
		actualCompileCommands = append(actualCompileCommands, command)
	}
	contents, err = json.Marshal(actualCompileCommands)
	if err != nil {
		return "", err
	}
	actualCompileCommandsPath := filepath.Join(filepath.Dir(compileCommandsPath), ActualCCJson)
	err = os.WriteFile(actualCompileCommandsPath, contents, os.ModePerm)
	if err != nil {
		return "", err
	}
	return actualCompileCommandsPath, nil
}

func CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath string) (string, error) {
	commands, err := compilecommand.ReadCompileCommandsFromFile(compileCommandsPath)
	if err != nil {
		return "", err
	}

	i := 0
	for _, cc := range *commands {
		if !cc.ContainsCC1() {
			(*commands)[i] = cc
			i++
		}
	}
	*commands = (*commands)[:i]

	dir := filepath.Dir(compileCommandsPath)
	tmpDir, err := os.MkdirTemp(dir, "")
	if err != nil {
		return "", err
	}
	newPath := filepath.Join(tmpDir, CCJson)
	newccFile, err := json.Marshal(commands)
	if err != nil {
		return "", err
	}
	err = os.WriteFile(newPath, newccFile, os.ModePerm)
	if err != nil {
		return "", err
	}

	return tmpDir, nil
}

func CreateLogDir(logDir string) error {
	err := os.MkdirAll(logDir, os.ModePerm)
	if err != nil {
		return err
	}
	libtoolingLogDir := filepath.Join(logDir, "libtooling")
	err = os.MkdirAll(libtoolingLogDir, os.ModePerm)
	if err != nil {
		return err
	}
	return nil
}

func FilterCheckRules(checkrules []checkrule.CheckRule, prefix string) []checkrule.CheckRule {
	var returnCheckRules []checkrule.CheckRule
	for _, rule := range checkrules {
		if strings.HasPrefix(rule.Name, prefix) {
			returnCheckRules = append(returnCheckRules, rule)
		}
	}
	return returnCheckRules
}

func ReadCheckRules(checkRulesPath string) ([]checkrule.CheckRule, error) {
	glog.Info("checkRulesPath ", checkRulesPath)
	checkRulesFile, err := os.Open(checkRulesPath)
	if err != nil {
		return nil, err
	}
	defer checkRulesFile.Close()

	scanner := bufio.NewScanner(checkRulesFile)
	checkRules := make([]checkrule.CheckRule, 0)
	logCheckRules := []string{}

	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.SplitN(line, " ", 2)
		ruleName := parts[0]
		jsonOptions := "{}"
		if len(parts) > 1 {
			jsonOptions = parts[1]
		}

		checkRule, err := checkrule.MakeCheckRule(ruleName, jsonOptions)
		if err != nil {
			return nil, err
		}
		logCheckRules = append(logCheckRules, line)
		checkRules = append(checkRules, *checkRule)
	}

	err = scanner.Err()
	if err != nil {
		return nil, err
	}
	glog.Infof("check_rules content:\n%s", strings.Join(logCheckRules, "\n"))
	return checkRules, nil
}

func CreateResultDir(resultsDir string) error {
	dir, err := os.Stat(resultsDir)
	if err != nil {
		if os.IsNotExist(err) {
			err = os.MkdirAll(resultsDir, os.ModePerm)
			return err
		} else {
			return err
		}
	}

	if !dir.IsDir() {
		// a file exists instead of dir
		return os.ErrExist
	}

	return nil
}

func ProcessIgnoreDir(allResults *pb.ResultsList, ignoreDirPatterns *ArrayFlags) *pb.ResultsList {
	for _, ignoreDirPattern := range *ignoreDirPatterns {
		newResults := []*pb.Result{}
		for _, result := range allResults.Results {
			matched, err := doublestar.Match(ignoreDirPattern, result.Path)
			if err != nil {
				glog.Error("malformed ignore_dir pattern ", ignoreDirPattern)
				newResults = allResults.Results
				break
			}
			if matched {
				glog.Infof("Result in path %s ignored due to pattern %s", result.Path, ignoreDirPattern)
			} else {
				newResults = append(newResults, result)
			}
		}
		allResults.Results = newResults
	}
	return allResults
}

func AddID(allResults *pb.ResultsList) {
	for i := 0; i < len(allResults.Results); i++ {
		id, err := uuid.NewRandom()
		if err != nil {
			// Just warning If id left empty here, it will be regenerated when
			// inserting results into DB. Report errors at that time.
			glog.Warningf("uuid.NewRandom: %v", err)
			continue
		}
		allResults.Results[i].Id = id.String()
	}
}

func AddCodeLineHash(allResults *pb.ResultsList) {
	start := time.Now()
	// suppose resultsList has been sorted by paths and line numbers
	// you may take baseline.SortResults() for reference
	var lastLineHash string
	for i := 0; i < len(allResults.Results); {
		result := allResults.Results[i]
		// rescan file only when the path is different from the last one
		if i != 0 && result.Path == allResults.Results[i-1].Path {
			i++
			continue
		}
		fi, err := os.Stat(result.Path)
		// skip if result.Path does not exist or other errors occur
		if err != nil {
			glog.Errorf("os.Stat('%s'): %v", result.Path, err)
			i++
			continue
		}
		// skip if result.Path is a dir
		if fi.IsDir() {
			glog.Warningf("'%s' is not a file", result.Path)
			i++
			continue
		}
		fileContent, _ := os.Open(result.Path)
		fileScanner := bufio.NewScanner(fileContent)
		var count int32 = 1
		for fileScanner.Scan() {
			for ; i < len(allResults.Results); i++ {
				curResult := allResults.Results[i]
				if count != curResult.LineNumber {
					break
				}
				// recompute line hash only when the lineNumber is different
				if i != 0 && curResult.LineNumber == allResults.Results[i-1].LineNumber {
					curResult.CodeLineHash = lastLineHash
					continue
				}
				// TODO: involve code context (more lines around) into line hash
				content := strings.TrimSpace(fileScanner.Text())
				h := sha1.New()
				h.Write([]byte(content))
				contentSha1 := hex.EncodeToString(h.Sum(nil))
				lastLineHash = contentSha1[:16]
				curResult.CodeLineHash = lastLineHash
			}
			count++
		}
		fileContent.Close()
	}
	// TODO: remove this log in the future
	glog.Infof("spent %s on adding CodeLineHash for all results", time.Since(start))
}

func ProcessSuppression(allResults *pb.ResultsList, suppressionDir string) (*pb.ResultsList, error) {
	var suppressionFiles []string
	err := filepath.Walk(suppressionDir, visit(&suppressionFiles))
	if err != nil {
		return allResults, err
	}
	suppressionMap, err := getSuppressionMap(suppressionFiles)
	if err != nil {
		return allResults, err
	}
	countMap := make(map[string]int)
	newResults := []*pb.Result{}
	for _, result := range allResults.Results {
		re := regexp.MustCompile(`\[(.*?)\]`)
		ruleCode := re.FindString(result.ErrorMessage)
		ruleCode = ruleCode[1 : len(ruleCode)-1]
		key := suppressionAsKey{content: result.CodeLineHash, ruleCode: ruleCode}
		_, exist := suppressionMap[key]
		if exist {
			_, ok := countMap[ruleCode]
			if ok {
				countMap[ruleCode] += 1
			} else {
				countMap[ruleCode] = 1
			}
		} else {
			newResults = append(newResults, result)
		}
	}
	for ruleCode, count := range countMap {
		glog.Infof("%d violations of %s are filtered out with suppression", count, ruleCode)
	}
	allResults.Results = newResults

	return allResults, nil
}

// convert path from relative path to absolute path
// remove results of which the path not in src dir
func FormatResultPath(allResults *pb.ResultsList, srcDir string) *pb.ResultsList {
	formattedResult := &pb.ResultsList{}
	for _, result := range allResults.Results {
		if !filepath.IsAbs(result.Path) {
			result.Path = filepath.Join(srcDir, result.Path)
		}
		for _, location := range result.Locations {
			if !filepath.IsAbs(location.Path) {
				location.Path = filepath.Join(srcDir, location.Path)
			}
		}
		if strings.HasPrefix(result.Path, srcDir) {
			formattedResult.Results = append(formattedResult.Results, result)
		}
	}

	return formattedResult
}

func WriteResults(allResults *pb.ResultsList, resultsPath string) error {
	out, err := proto.Marshal(allResults)
	if err != nil {
		return err
	}

	err = os.WriteFile(resultsPath, out, os.ModePerm)
	if err != nil {
		return err
	}

	return nil
}

func WriteJsonResults(allResults *pb.ResultsList, resultsPath string) error {
	options := protojson.MarshalOptions{
		Indent:        "",
		UseProtoNames: true,
	}
	out, err := options.Marshal(allResults)
	if err != nil {
		return err
	}
	var rawMessage json.RawMessage = out
	outWithIndent, err := json.MarshalIndent(rawMessage, "", "  ")
	if err != nil {
		return err
	}
	err = os.WriteFile(resultsPath, outWithIndent, os.ModePerm)
	if err != nil {
		return err
	}

	return nil
}

func PrintResults(allResults *pb.ResultsList, printCounts bool) {
	results := allResults.Results
	result_count_map := map[string]int{}

	sort.Slice(results, func(i, j int) bool {
		x := results[i]
		y := results[j]
		if x.Path < y.Path {
			return true
		}
		if x.Path > y.Path {
			return false
		}
		if x.LineNumber < y.LineNumber {
			return true
		}
		if x.LineNumber > y.LineNumber {
			return false
		}
		return x.ErrorMessage < y.ErrorMessage
	})

	for _, result := range results {
		fmt.Printf("%s:%d: %s\n\n", result.Path, result.LineNumber, result.ErrorMessage)
		result_count_map[result.ErrorMessage]++
	}
	if printCounts {
		// add a group by output to show the occurred times of an error in project.
		for errorMessage, count := range result_count_map {
			fmt.Printf("count: %d error message: %s\n", count, errorMessage)
		}
	}
}

func CleanResultDir(resultsDir string) error {
	filesToIgnore := []string{}
	// add *.nsa_metadata to filesToIgnore
	err := filepath.Walk(resultsDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if !info.IsDir() && strings.HasSuffix(path, ".nsa_metadata") {
			filesToIgnore = append(filesToIgnore, filepath.Base(path))
		}
		return nil
	})
	if err != nil {
		glog.Errorf("filepath.Walk: %v", err)
	}

	cleanedLogDir := filepath.Clean(flag.Lookup("log_dir").Value.String())
	cleanedResultsDir := filepath.Clean(resultsDir)

	// If logDir is in resultsDir, we shall keep it.
	if strings.HasPrefix(cleanedLogDir, cleanedResultsDir) {
		relPath, err := filepath.Rel(cleanedResultsDir, cleanedLogDir)
		if err != nil {
			glog.Errorf("filepath.Rel: %v", err)
		}
		ignoredName := strings.Split(relPath, string(filepath.Separator))[0] // ignore the whole subfolder.
		glog.Infof("clean cache: ignoring log dir: %s", ignoredName)
		filesToIgnore = append(filesToIgnore, ignoredName)
	}

	err = utils.CleanCache(resultsDir, filesToIgnore)
	if err != nil {
		return err
	}
	return nil
}

func MatchIgnoreDirPatterns(ignoreDirPatterns []string, filePath string) (bool, error) {
	matched := false
	var err error
	for _, ignoreDirPattern := range ignoreDirPatterns {
		matched, err = doublestar.Match(ignoreDirPattern, filePath)
		if err != nil {
			return matched, fmt.Errorf("malformed ignore_dir pattern %s", ignoreDirPattern)
		}
		if matched {
			glog.Infof("Source file %s ignored due to pattern %s", filePath, ignoreDirPattern)
			break
		}
	}
	return matched, nil
}

func CountLinesUnderDir(workingDirs []string, countLangs []string, ignoreDirPatterns []string) (int, error) {
	clocOpts := gocloc.NewClocOptions()
	languages := gocloc.NewDefinedLanguages()
	for _, lang := range countLangs {
		if _, exists := languages.Langs[lang]; exists {
			clocOpts.IncludeLangs[lang] = struct{}{}
		}
	}
	processor := gocloc.NewProcessor(languages, clocOpts)
	result, err := processor.Analyze(workingDirs)
	if err != nil {
		glog.Errorf("gocloc fail: %v", err)
		return 0, err
	}
	sum := 0
	for _, file := range result.Files {
		matched, err := MatchIgnoreDirPatterns(ignoreDirPatterns, file.Name)
		if err != nil {
			glog.Error(err)
			continue
		}
		if matched {
			continue
		}
		sum += int(file.Code)
	}

	return sum, nil
}

func CountLines(compileCommandsPath string, countLangs []string, ignoreDirPatterns []string) (int, error) {
	commandsPtr, err := compilecommand.ReadCompileCommandsFromFile(compileCommandsPath)
	if err != nil {
		glog.Errorf("failed to read compile commands: %v", err)
		return 0, err
	}

	workingDirs := []string{}
	dirMap := map[string]struct{}{}
	for _, command := range *commandsPtr {
		matched, err := MatchIgnoreDirPatterns(ignoreDirPatterns, command.File)
		if err != nil {
			glog.Error(err)
			continue
		}
		if matched {
			continue
		}

		workDir := ""
		if filepath.IsAbs(command.File) {
			workDir = command.File
		} else {
			workDir = filepath.Join(command.Directory, command.File)
		}
		dirMap[workDir] = struct{}{}
	}

	for dir := range dirMap {
		workingDirs = append(workingDirs, dir)
	}

	return CountLinesUnderDir(workingDirs, countLangs, ignoreDirPatterns)
}

type Severity struct {
	Id    string
	Label string
}

var SEVERITY = map[int32]Severity{
	1: {Id: "highest", Label: "最高"},
	2: {Id: "high", Label: "高"},
	3: {Id: "medium", Label: "中"},
	4: {Id: "low", Label: "低"},
	5: {Id: "lowest", Label: "最低"},
	0: {Id: "unknown", Label: "未定义"},
}

type Violation struct {
	Path    string `json:"path"`
	Code    string `json:"code"`
	Details string `json:"details,omitempty"`
}

type Rule struct {
	Ident      string      `json:"ident"`
	Subject    string      `json:"subject"`
	Severity   string      `json:"severity"`
	Violations []Violation `json:"violations"`
}

type Report struct {
	Rules []Rule `json:"rules"`
}

func GenerateReport(allResults *pb.ResultsList, srcDir, reportPath, lang string) error {
	ruleMap := make(map[string]Rule)
	ruleNameList := []string{}
	for _, result := range allResults.Results {
		var fullRuleName, subject, externalMessage string
		if result.Ruleset != "" {
			fullRuleName = fmt.Sprintf("%s/%s", result.Ruleset, result.RuleId)
			subject = result.ErrorMessage
		} else {
			var errorMessage string
			errorMessage, externalMessage, _ = strings.Cut(result.ErrorMessage, "\n")
			var displayGuideline string
			for n, guideline := range rulesets.GUIDELINES {
				if strings.Contains(errorMessage, guideline) {
					displayGuideline = n
					break
				}
			}
			if displayGuideline == "" {
				glog.Errorf("cannot match rule set for %s", errorMessage)
				continue
			}
			fullRuleName = rulesets.GetRuleFullName(displayGuideline, errorMessage)
			if fullRuleName == "" {
				glog.Errorf("cannot match rule name for %s", errorMessage)
				continue
			}
			re := regexp.MustCompile(`\[.*\]: (.*)`)
			matches := re.FindStringSubmatch(errorMessage)
			if len(matches) != 2 {
				glog.Errorf("cannot find the subject for %s", errorMessage)
				continue
			}
			subject = matches[1]
		}

		severity, exist := SEVERITY[result.Severity]
		if !exist {
			severity = SEVERITY[0]
		}
		var severityName string
		if lang == "zh" {
			severityName = severity.Label
		} else {
			severityName = severity.Id
		}

		rule, exist := ruleMap[fullRuleName]
		if !exist {
			ruleNameList = append(ruleNameList, fullRuleName)
			rule = Rule{Ident: fullRuleName, Subject: subject, Severity: severityName, Violations: make([]Violation, 0)}
		}

		path := strings.TrimPrefix(result.Path, "/src/")
		code, err := rs.GetCode(filepath.Join(srcDir, path), result.LineNumber /* charset =*/, "utf8")
		if err != nil {
			glog.Errorf("GetCode: %v", err)
			continue
		}
		rule.Violations = append(rule.Violations, Violation{Path: path, Code: code, Details: externalMessage})
		ruleMap[fullRuleName] = rule
	}
	sort.Strings(ruleNameList)
	rules := []Rule{}
	for _, ruleName := range ruleNameList {
		rule := ruleMap[ruleName]
		if len(rule.Violations) > 0 {
			rules = append(rules, rule)
		}
	}
	if len(rules) == 0 && len(allResults.Results) > 0 {
		glog.Warningf("the rulesets may not be supported to generate report yet")
	}
	report := Report{Rules: rules}
	buf := new(bytes.Buffer)
	enc := json.NewEncoder(buf)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "  ")
	err := enc.Encode(&report)
	if err != nil {
		return fmt.Errorf("enc.Encode: %v", err)
	}
	err = os.WriteFile(reportPath, buf.Bytes(), os.ModePerm)
	if err != nil {
		return fmt.Errorf("failed to write %s: %v", reportPath, err)
	}

	return nil
}
