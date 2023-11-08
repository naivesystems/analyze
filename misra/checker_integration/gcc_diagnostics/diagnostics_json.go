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

package gcc_diagnostics

import "encoding/json"

type Location struct {
	Column int    `json:"column"`
	Line   int    `json:"line"`
	File   string `json:"file"`
}

type Diagnostic struct {
	Kind      string `json:"kind"`
	Locations []struct {
		// If you need to read another field of `locations`,
		// just create it like `caret` below.
		// Note that the type of fields of `locations` are not all 'Location'
		// (maybe a field `label` of type 'string').
		Caret Location `json:"caret"`
	} `json:"locations"`
	Option  string `json:"option"`
	Message string `json:"message"`
}

func ParseDiagnosticsJson(output []byte) ([]Diagnostic, error) {
	var diagnostics []Diagnostic
	err := json.Unmarshal(output, &diagnostics)
	return diagnostics, err
}
