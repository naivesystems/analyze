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

package rule_A13_6_1

import (
	"fmt"
	"os"
	"regexp"
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
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

	for _, sourceFile := range sourceFiles {
		contentInByte, _ := os.ReadFile(sourceFile)
		content := string(contentInByte)

		content = deleteByRegexes(content, rMultilineComment, rComment, rStringLiteral)

		// check for hexadecimal numbers
		updatedContent := reportIfMatch(content, `0x([0-9A-Fa-f]+(%s[0-9A-Fa-f]+)+)`, 2, results, sourceFile)
		// check for binary numbers
		updatedContent = reportIfMatch(updatedContent, `0b([01]+(%s[01]+)+)`, 4, results, sourceFile)
		// check for decimal numbers
		reportIfMatch(updatedContent, `\b\d+((?:\.\d+)?%s\d+)+\b`, 3, results, sourceFile)
	}
	results = runner.SortResult(results)
	results = runner.RemoveDup(results)
	return results, nil
}

func reportIfMatch(content string, pattern string, digitalLen int, results *pb.ResultsList, sourceFile string) string {
	regexPattern := fmt.Sprintf(pattern, regexp.QuoteMeta("'"))
	regex := regexp.MustCompile(regexPattern)

	updatedContent := content

	lines := strings.Split(content, "\n")
	for n, line := range lines {
		matches := regex.FindAllString(line, -1)
		for _, match := range matches {
			// remove this matched string from the original content
			// to prevent repetitive match in later matching
			// eg. if the content contains the string "0b1010'1010'1", it would be matched when searching for binary numbers.
			// The portion "1010'1" would be matched again when searching for decimal numbers, which is not the desired behavior.
			updatedContent = strings.Replace(updatedContent, match, "", 1)
			// remove the prefix before checking the length of the string
			if digitalLen == 2 {
				match = strings.TrimPrefix(match, "0x")
			}
			if digitalLen == 4 {
				match = strings.TrimPrefix(match, "0b")
			}
			// split float numbers into integer and fraction and check each separately
			intFrac := strings.Split(match, ".")
			for _, splitMatch := range intFrac {
				parts := strings.Split(splitMatch, "'")
				// check the length of each part separated by ' (except the last part)
				for i := 0; i < len(parts)-1; i++ {
					part := parts[i]
					if len(part) != digitalLen {
						reportError(n+1, sourceFile, results)
					}
				}
				// check the length of the last part
				lastPart := parts[len(parts)-1]
				if len(lastPart) > digitalLen {
					reportError(n+1, sourceFile, results)
				}
			}
		}
	}
	return updatedContent
}

func deleteByRegexes(str string, rList ...*regexp.Regexp) string {
	for _, r := range rList {
		str = r.ReplaceAllLiteralString(str, "")
	}
	return str
}

func reportError(lineNumber int, filepath string, results *pb.ResultsList) {
	results.Results = append(results.Results, &pb.Result{
		Path:         filepath,
		LineNumber:   int32(lineNumber),
		ErrorMessage: "Digit sequences separators ' shall only be used as follows: (1) for decimal, every 3 digits, (2) for hexadecimal, every 2 digits, (3) for binary, every 4 digits.",
	})
}
