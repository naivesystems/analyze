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

package infer

type TraceItem struct {
	Level        int32  `json:"level"`
	Filename     string `json:"filename"`
	LineNumber   int32  `json:"line_number"`
	ColumnNumber int32  `json:"column_number"`
	Description  string `json:"description"`
}

type InferReport struct {
	Bug_type  string      `json:"bug_type"`
	Qualifier string      `json:"qualifier"`
	Line      int32       `json:"line"`
	File      string      `json:"file"`
	Bug_trace []TraceItem `json:"bug_trace"`
}
