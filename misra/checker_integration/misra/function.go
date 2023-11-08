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

package misra

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/misra/analyzer/analyzerinterface"
	"naive.systems/analyzer/misra/checker_integration/infer"
)

// Get CallGraph of source files
func GetImpureFunctions(srcDir, compileCommandsPath string, resultsDir string, config *pb.CheckerConfiguration, limitMemory bool, timeoutNormal, timeoutOom int) ([]string, error) {
	plugin := "--impurity-only --function-pointer-specialization"
	filteredCompileCommandsFolder, err := analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile(compileCommandsPath)
	if err != nil {
		return nil, fmt.Errorf("analyzerinterface.CreateTempFolderContainsFilteredCompileCommandsJsonFile: %v", err)
	}
	defer os.RemoveAll(filteredCompileCommandsFolder)
	reportJson, err := infer.GetInferReport(srcDir, filepath.Join(filteredCompileCommandsFolder, analyzerinterface.CCJson), plugin, resultsDir, config, limitMemory, timeoutNormal, timeoutOom)
	if err != nil {
		return nil, err
	}
	re := regexp.MustCompile(`Impure function (.*).`)
	impureFunctions := make([]string, 0)
	for _, report := range reportJson {
		if report.Bug_type == "IMPURE_FUNCTION" {
			match := re.FindStringSubmatch(report.Qualifier)
			impureFunctions = append(impureFunctions, match[1])
		}
	}
	return impureFunctions, nil
}
