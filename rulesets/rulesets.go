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

package rulesets

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"regexp"
	"strings"

	"github.com/golang/glog"
	"golang.org/x/text/encoding/ianaindex"
	"golang.org/x/text/transform"
)

var GUIDELINES = map[string]string{
	"MISRA C:2012":   "misra-c2012",
	"MISRA C++:2008": "misra-cpp2008",
	"GJB":            "gjb",
	"CWE":            "cwe",
	"AUTOSAR":        "autosar",
}

// to fix the sequence in GUIDELINES map
var GUIDELINE_NAMES = []string{"MISRA C:2012", "MISRA C++:2008", "GJB", "CWE", "AUTOSAR"}

func GetRuleFullName(displayGuideline, errorMessage string) string {
	re := regexp.MustCompile(`(dir\-)?(A\.)?\d+\.\d+(\.\d+)?`)
	ruleName := re.FindString(errorMessage)
	// match XXX in [cwe-cwe_XXX]
	var cweRuleName string
	cweRe := regexp.MustCompile(`cwe-cwe_(\d+)?`)
	matches := cweRe.FindAllStringSubmatch(errorMessage, -1)
	if len(matches) > 0 {
		cweRuleName = matches[0][1]
	}
	// match AX.X.X in [autosar-AX.X.X] or MX.X.X in [autosar-MX.X.X]
	autosarRe := regexp.MustCompile(`[AM]\d+\.\d+\.\d+`)
	autosarRuleName := autosarRe.FindString(errorMessage)
	// TODO: accommodate more rule format of new guidelines
	if ruleName == "" && cweRuleName == "" && autosarRuleName == "" {
		return ""
	}
	var fullName string
	if ruleName != "" {
		// format as 'MISRA C:2012 Rule 19.2' or 'MISRA C:2012 Dir 4.3'
		// or 'MISRA CPP:2008 Rule 10.3.2' or 'GJB 5369 Rule 1.1.1'
		// or 'GJB 8114 Rule A.1.1.1'
		ruleType := "Rule"
		dirPrefix := "dir-"
		if strings.HasPrefix(ruleName, dirPrefix) {
			ruleType = "Dir"
			ruleName = strings.TrimLeft(ruleName, dirPrefix)
		}
		fullName = fmt.Sprintf("%s %s %s", displayGuideline, ruleType, ruleName)
	}
	if cweRuleName != "" {
		// format as 'CWE-758'
		fullName = "CWE-" + cweRuleName
	}
	if autosarRuleName != "" {
		// format as 'AUTOSAR Rule A7-2-3'
		fullName = "AUTOSAR Rule " + strings.ReplaceAll(autosarRuleName, ".", "-")
	}
	return fullName
}

func convertCharset(b []byte, charset string) string {
	/*
		The function aims at detecting the encoding and convert it to UTF-8,
		but charset.DetermineEncoding() may not always able to detect the source
		text right.
	*/
	byteReader := bytes.NewReader(b)
	e, err := ianaindex.MIME.Encoding(charset)
	if err != nil {
		glog.Warning("ianaindex.MIME.Encoding err, the charset is considered as UTF-8 by default")
		return string(b)
	}
	if e == nil {
		glog.Warning("charset not found, the charset is considered as UTF-8 by default")
		return string(b)
	}
	reader := transform.NewReader(byteReader, e.NewDecoder())
	bytes, err := io.ReadAll(reader)
	if err != nil {
		glog.Warning("ioutil.ReadAll err, the charset is considered as UTF-8 by default")
		return string(b)
	}
	return string(bytes)
}

func GetCode(path string, lineNumber int32, charset string) (string, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	lower := lineNumber - 2
	upper := lineNumber + 2
	var lineCount int32 = 0
	var output string = ""
	for scanner.Scan() {
		lineCount++
		if lineCount < lower {
			continue
		} else if lineCount > upper {
			break
		}
		var text string
		if charset == "utf8" {
			text = scanner.Text()
		} else {
			text = convertCharset(scanner.Bytes(), charset)
		}
		if lineCount == lineNumber {
			output = output + fmt.Sprintf("> %d| %s\n", lineCount, text)
		} else {
			output = output + fmt.Sprintf("%d| %s\n", lineCount, text)
		}
	}
	if err = scanner.Err(); err != nil {
		return "", err
	}
	return output, err
}
