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

package rule_5_2_7

import (
	"strings"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func MisraCXXCheckRule5_2_7(reportJson *csa.CSAReport) *pb.ResultsList {
	resultsList := &pb.ResultsList{}
	for _, run := range reportJson.Runs {
		for _, resultsContent := range run.Results {
			if resultsContent.RuleId == "misra_cxx_2008.PointerCast" {
				result := &pb.Result{}
				result.Path = strings.Replace(resultsContent.Locations[0].PhysicalLocation.ArtifactLocation.Uri, "file://", "", 1)
				result.LineNumber = resultsContent.Locations[0].PhysicalLocation.Region.StartLine
				result.ErrorMessage = "一个具有指针类型的对象不得被直接或间接转换为不相关的指针类型"
				result.ErrorKind = pb.Result_MISRA_CPP_2008_RULE_5_2_7
				resultsList.Results = append(resultsList.Results, result)
				glog.Info(result.ErrorMessage)
			}
		}
	}
	return resultsList
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := runner.RunCSA(srcdir, "-analyzer-checker=misra_cxx_2008.PointerCast", opts)
	if err != nil {
		glog.Errorf("failed to execute libtooling: %v", err)
		return nil, err
	}
	return MisraCXXCheckRule5_2_7(results), nil
}
