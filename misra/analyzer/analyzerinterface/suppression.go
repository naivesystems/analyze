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

package analyzerinterface

import (
	"os"
	"path/filepath"

	"google.golang.org/protobuf/encoding/protojson"
	pb "naive.systems/analyzer/analyzer/proto"
)

type suppressionAsKey struct {
	content  string
	ruleCode string
}

func visit(files *[]string) filepath.WalkFunc {
	return func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() || filepath.Ext(path) != ".nsa_suppression" {
			return nil
		}
		*files = append(*files, path)
		return nil
	}
}

func getSuppressionMap(suppressionFiles []string) (map[suppressionAsKey]*pb.Suppression, error) {
	suppressionMap := make(map[suppressionAsKey]*pb.Suppression)
	for _, suppressionFile := range suppressionFiles {
		bytes, err := os.ReadFile(suppressionFile)
		suppressions := &pb.SuppressionsList{}
		if err != nil {
			return nil, err
		}
		err = protojson.Unmarshal(bytes, suppressions)
		if err != nil {
			return nil, err
		}
		for _, suppression := range suppressions.Suppressions {
			key := suppressionAsKey{content: suppression.Content, ruleCode: suppression.RuleCode}
			suppressionMap[key] = suppression
		}
	}
	return suppressionMap, nil
}
