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

// Clang Sema XML

// For example, hello.c:

// 	int main(){
// 		int i;
// 		if ((i > -1) || (i < 1)){}
// 	}

// Using `clang -cc1 -fsyntax-only -diagnostic-log-file outxml -Wall test.c` (outxml is an existed file), we get output xml

package clangsema

import (
	"encoding/xml"
	"strconv"
)

type DiagnosticKV struct {
	Keys         []string `xml:"key"`
	StringValues []string `xml:",any"`
}

type CollectionKV struct {
	Keys          []string       `xml:"key"`
	MainFileValue string         `xml:"string"`
	Diagnostics   []DiagnosticKV `xml:"array>dict"`
}

func ParseSemaKV(blob []byte) (CollectionKV, error) {
	var collectionKV CollectionKV
	err := xml.Unmarshal(blob, &collectionKV)
	if err != nil {
		return collectionKV, err
	}
	return collectionKV, err
}

type Diagnostic struct {
	Level         string
	MainFilename  string
	Filename      string
	Line          int
	Column        int
	Message       string
	ID            int
	WarningOption string
}

func Parse(blob []byte) ([]Diagnostic, error) {
	var diagnostics []Diagnostic
	collectionKV, err := ParseSemaKV(blob)
	if err != nil {
		return diagnostics, err
	}
	for i := 0; i < len(collectionKV.Diagnostics); i++ {
		var diagnostic Diagnostic
		keys := collectionKV.Diagnostics[i].Keys
		values := collectionKV.Diagnostics[i].StringValues
		for j := 0; j < len(keys); j++ {
			switch keys[j] {
			case "level":
				diagnostic.Level = values[j]
			case "filename":
				diagnostic.Filename = values[j]
			case "line":
				intLine, _ := strconv.Atoi(values[j])
				diagnostic.Line = intLine
			case "column":
				intColumn, _ := strconv.Atoi(values[j])
				diagnostic.Column = intColumn
			case "message":
				diagnostic.Message = values[j]
			case "ID":
				intID, _ := strconv.Atoi(values[j])
				diagnostic.ID = intID
			case "WarningOption":
				diagnostic.WarningOption = values[j]
			default:
			}
		}
		diagnostic.MainFilename = collectionKV.MainFileValue
		diagnostics = append(diagnostics, diagnostic)
	}
	return diagnostics, err
}
