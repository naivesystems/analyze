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

package rule_A18_5_3

import (
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"strings"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	reportJson, err := runner.RunCSA(srcdir, "-analyzer-checker=unix.MismatchedDeallocator", opts)
	if err != nil {
		glog.Errorf("failed to execute csa: %v", err)
		return nil, err
	}
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		for _, resultsContent := range run.Results {
			if resultsContent.RuleId == "unix.MismatchedDeallocator" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorMessage = "The form of the delete expression shall match the form of the new expression used to allocate the memory."
				resultsList.Results = append(resultsList.Results, result)
				glog.Info(result.ErrorMessage)
			}
		}
	}
	return resultsList, nil
}
