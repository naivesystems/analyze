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

package rule_A2_13_6

import (
	"bufio"
	"os"
	"regexp"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/misra/analyzer"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results := &pb.ResultsList{}
	compileCommandsPath := options.GetCompileCommandsPath(srcdir)
	sourceFiles, err := analyzer.ListSourceFiles(compileCommandsPath, opts.EnvOption.IgnoreDirPatterns)
	if err != nil {
		glog.Errorf("failed to get source files: %v", err)
	}

	rMultilineComment := regexp.MustCompile(`(?s:/\*.*?\*/)`)
	rComment := regexp.MustCompile(`(?m://.*$)`)
	rStringLiteral := regexp.MustCompile(`(?s:".*?")`)
	rCharLiteral := regexp.MustCompile(`'.*'`)

	for _, sourceFile := range sourceFiles {
		content := ""
		file, _ := os.Open(sourceFile)
		defer file.Close()

		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			line := scanner.Text()
			content += line + "\n"
		}

		content = delete(content, rMultilineComment, rComment, rStringLiteral, rCharLiteral)

		lines := strings.Split(content, "\n")
		for n, line := range lines {
			if strings.Contains(line, "\\U") || strings.Contains(line, "\\u") {
				reportError(n+1, sourceFile, results)
			}
		}
	}
	return results, nil
}

func delete(str string, rList ...*regexp.Regexp) string {
	for _, r := range rList {
		str = r.ReplaceAllLiteralString(str, "")
	}
	return str
}

func reportError(lineNumber int, filepath string, results *pb.ResultsList) {
	results.Results = append(results.Results, &pb.Result{
		Path:         filepath,
		LineNumber:   int32(lineNumber),
		ErrorMessage: "Universal character names shall be used only inside character or string literals.",
	})
}
