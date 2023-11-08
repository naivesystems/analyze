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

package rule_A18_5_10

import (
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	reportJson, err := runner.RunCSA(srcdir, "-analyzer-checker=cplusplus.PlacementNew", opts)
	if err != nil {
		return nil, err
	}
	results := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		for _, resultsContent := range run.Results {
			if resultsContent.RuleId == "cplusplus.PlacementNew" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorMessage = "Placement new shall be used only with properly aligned pointers to sufficient storage capacity."
				results.Results = append(results.Results, result)
			}
		}
	}
	return results, nil
}
