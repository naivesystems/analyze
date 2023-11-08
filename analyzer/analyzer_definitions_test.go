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
	"os"
	"testing"

	pb "naive.systems/analyzer/analyzer/proto"

	"google.golang.org/protobuf/encoding/prototext"
)

func TestLoadCheckerConfigFromExample(t *testing.T) {
	path := "analyzer_definitions.textproto.example"
	contents, err := os.ReadFile(path)
	if err != nil {
		t.Error(err)
	}
	defs := pb.Definitions{}
	err = prototext.Unmarshal(contents, &defs)
	if err != nil {
		t.Error(err)
	}
	if defs.CheckerConfig.InferBin != "infer" {
		t.Error(defs.CheckerConfig.InferBin)
	}
}
