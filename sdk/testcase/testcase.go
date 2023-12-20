/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2022-2023  Naive Systems Ltd.

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

package testcase

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"google.golang.org/protobuf/encoding/prototext"
	"google.golang.org/protobuf/proto"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/testlib"
)

type TestCase struct {
	t       *testing.T
	Srcdir  string
	Options *options.CheckOptions
}

func New(t *testing.T, dirname string) TestCase {
	srcdir, err := filepath.Abs(dirname)
	if err != nil {
		t.Fatalf("filepath.Abs(%s): %v", dirname, err)
	}
	// Note that testlib.MakeTestOption is location sensitive.
	options, err := testlib.MakeTestOption(srcdir)
	if err != nil {
		t.Fatalf("failed to init testing env: %v", err)
	}
	return TestCase{t, srcdir, options}
}

func NewWithSystemHeader(t *testing.T, dirname string) TestCase {
	srcdir, err := filepath.Abs(dirname)
	if err != nil {
		t.Fatalf("filepath.Abs(%s): %v", dirname, err)
	}
	// Note that testlib.MakeTestOption is location sensitive.
	options, err := testlib.MakeTestOption(srcdir)
	if err != nil {
		t.Fatalf("failed to init testing env: %v", err)
	}
	testlib.AddSystemHeader(options)
	return TestCase{t, srcdir, options}
}

func NewWithoutOptions(t *testing.T, dirname string) TestCase {
	srcdir, err := filepath.Abs(dirname)
	if err != nil {
		t.Fatalf("filepath.Abs(%s): %v", dirname, err)
	}
	options := options.CheckOptions{}
	return TestCase{t, srcdir, &options}
}

func (tc *TestCase) expectedEquals(actual *pb.ResultsList) bool {
	path := filepath.Join(tc.Srcdir, "expected.textproto")
	bytes, err := os.ReadFile(path)
	if err != nil {
		tc.t.Fatalf("os.ReadFile(%s): %v", path, err)
	}
	expected := &pb.ResultsList{}
	err = prototext.Unmarshal(bytes, expected)
	if err != nil {
		tc.t.Fatalf("prototext.Unmarshal(%s): %v", path, err)
	}
	// ignore misra fields
	cleaned_actual := &pb.ResultsList{}
	for _, result := range actual.Results {
		cleaned_result := &pb.Result{}
		cleaned_result.Path = result.Path
		cleaned_result.LineNumber = result.LineNumber
		cleaned_result.Locations = result.Locations
		// TODO(r/13482): remove this hack and compare the whole error message
		cleaned_result.ErrorMessage, _, _ = strings.Cut(result.ErrorMessage, "\n")
		cleaned_result.FalsePositive = result.FalsePositive
		cleaned_actual.Results = append(cleaned_actual.Results, cleaned_result)
	}
	for _, result := range expected.Results {
		result.Path = filepath.Join(tc.Srcdir, result.Path)
	}
	return proto.Equal(expected, cleaned_actual)
}

func (tc *TestCase) dumpProto(m proto.Message) {
	bytes, err := prototext.Marshal(m)
	if err == nil {
		tc.t.Log(string(bytes))
	} else {
		tc.t.Errorf("prototext.Marshal: %v", err)
	}
}

func (tc *TestCase) ExpectOK(actual *pb.ResultsList, err error) {
	if err != nil {
		tc.t.Fatalf("checker returned error: %v", err)
	}
	if !tc.expectedEquals(actual) {
		tc.dumpProto(actual)
		tc.t.Fatal("checker is expected to be OK")
	}
}

func (tc *TestCase) ExpectFailure(actual *pb.ResultsList, err error) {
	if err != nil {
		tc.t.Fatalf("checker returned error: %v", err)
	}
	if tc.expectedEquals(actual) {
		tc.dumpProto(actual)
		tc.t.Fatal("checker is expected to fail")
	}
}

func (tc *TestCase) ExpectError(_ *pb.ResultsList, err error) {
	if err == nil {
		tc.t.Fatal("checker is expected to return an error")
	}
	tc.t.Logf("checker returned error: %v", err)
}
