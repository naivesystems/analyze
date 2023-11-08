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

package analyzer

import (
	"fmt"
	"os"
	"path/filepath"

	"google.golang.org/protobuf/encoding/prototext"
	"google.golang.org/protobuf/proto"
	"naive.systems/analyzer/analyzer/podman"
	pb "naive.systems/analyzer/analyzer/proto"
)

type Analyzer struct {
	Definition *pb.Definition
	Rules      []*Rule
}

type Rule struct {
	Name        string
	JSONOptions string
}

func Init(definitionConfigPath string) (*pb.Definitions, error) {
	content, err := os.ReadFile(definitionConfigPath)
	if err != nil {
		return nil, err
	}
	var definitions pb.Definitions
	err = prototext.Unmarshal(content, &definitions)
	if err != nil {
		return nil, err
	}
	return &definitions, nil
}

func RunSingleAnalyzer(
	podmanBinPath,
	workingDir,
	logDir,
	imageName string,
	analyzer *Analyzer,
	checkerConfig *pb.CheckerConfiguration,
	srcDir string,
	ignorePatterns []string,
	projectType,
	qtProPath,
	scriptContents,
	projectName,
	checkRulesDir string,
) (*pb.ResultsList, error) {
	invocationPath := analyzer.Definition.InvocationPath
	err := podman.Run(
		podmanBinPath,
		checkRulesDir,
		workingDir,
		logDir,
		imageName,
		invocationPath,
		checkerConfig,
		srcDir,
		ignorePatterns,
		projectType,
		qtProPath,
		scriptContents,
		projectName,
	)
	if err != nil {
		return nil, fmt.Errorf("podman.Run: %v", err)
	}
	resultList, err := getResult(filepath.Join(logDir, "results"))
	if err != nil {
		resultList, err = getResult(filepath.Join(logDir, "results.nsa_results"))
		if err != nil {
			return nil, fmt.Errorf("getResult(%s): %v", filepath.Join(logDir, "results"), err)
		}
	}
	return resultList, nil
}

func getResult(resultFile string) (*pb.ResultsList, error) {
	content, err := os.ReadFile(resultFile)
	if err != nil {
		return nil, err
	}
	var result pb.ResultsList
	err = proto.Unmarshal(content, &result)
	if err != nil {
		return nil, err
	}
	return &result, nil
}
