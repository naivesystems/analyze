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

package csa

type ArtifactLocation struct {
	Uri string `json:"uri"`
}

type Region struct {
	StartLine int32 `json:"startLine"`
}

type PhysicalLocation struct {
	ArtifactLocation ArtifactLocation `json:"artifactLocation"`
	Region           Region           `json:"region"`
}

type Message struct {
	Text string `json:"text"`
}

type LocationsContent struct {
	PhysicalLocation PhysicalLocation `json:"physicalLocation"`
}

type ResultsContent struct {
	RuleId    string             `json:"ruleId"`
	Message   Message            `json:"message"`
	Locations []LocationsContent `json:"locations"`
}

type RunsContent struct {
	Results []ResultsContent `json:"results"`
}

type CSAReport struct {
	Runs []RunsContent `json:"runs"`
}
