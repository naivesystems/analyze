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

package proto

type resultBlood struct {
	path         string
	lineNumber   int32
	errorMessage string
}

// ResultsSet is an alternative to ResultsList. When Add() is called, it checks
// ResultBlood to identify unique Results. It preserves Results' adding order.
type ResultsSet struct {
	// You can manipulate ResultsList beyond the limits.
	ResultsList
	stored map[resultBlood]struct{}
}

func NewResultsSet() *ResultsSet {
	set := ResultsSet{}
	set.stored = make(map[resultBlood]struct{})
	return &set
}

func NewResultsSetFromList(list *ResultsList) *ResultsSet {
	set := NewResultsSet()
	set.AddList(list)
	return set
}

func (rs *ResultsSet) Add(r *Result) {
	blood := resultBlood{
		path:         r.Path,
		lineNumber:   r.LineNumber,
		errorMessage: r.ErrorMessage,
	}
	if _, reported := rs.stored[blood]; !reported {
		rs.stored[blood] = struct{}{}
		rs.Results = append(rs.Results, r)
	}
}

func (rs *ResultsSet) AddList(list *ResultsList) {
	for _, r := range list.Results {
		rs.Add(r)
	}
}
