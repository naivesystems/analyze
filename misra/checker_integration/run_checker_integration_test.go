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

package checker_integration

import (
	"strconv"
	"testing"

	pb "naive.systems/analyzer/analyzer/proto"
)

var pathOne = "/src"
var pathTwo = "/tmp"
var lineNumOne = int32(5)
var lineNumTwo = int32(6)
var errMsgOne = "[C0901 misra_c_2021]: Violation of 001"
var errMsgTwo = "[C0901 misra_c_2021]: Violation of 002"
var mapStrOne = pathOne + strconv.Itoa(int(lineNumOne)) + errMsgOne
var mapStrTwo = pathTwo + strconv.Itoa(int(lineNumOne)) + errMsgOne

var resultA = pb.Result{
	Path:         pathOne,
	LineNumber:   lineNumOne,
	ErrorMessage: errMsgOne,
}

var resultB = pb.Result{
	Path:         pathOne,
	LineNumber:   lineNumOne,
	ErrorMessage: errMsgOne,
}

var resultC = pb.Result{
	Path:         pathTwo,
	LineNumber:   lineNumOne,
	ErrorMessage: errMsgOne,
}

var resultD = pb.Result{
	Path:         pathOne,
	LineNumber:   lineNumTwo,
	ErrorMessage: errMsgOne,
}

var resultE = pb.Result{
	Path:         pathOne,
	LineNumber:   lineNumOne,
	ErrorMessage: errMsgTwo,
}

var resultF = pb.Result{
	Path:         pathTwo,
	LineNumber:   lineNumTwo,
	ErrorMessage: errMsgOne,
}

var resultG = pb.Result{
	Path:         pathTwo,
	LineNumber:   lineNumOne,
	ErrorMessage: errMsgTwo,
}

var resultH = pb.Result{
	Path:         pathOne,
	LineNumber:   lineNumTwo,
	ErrorMessage: errMsgTwo,
}

var resultI = pb.Result{
	Path:         pathTwo,
	LineNumber:   lineNumTwo,
	ErrorMessage: errMsgTwo,
}

func TestCheckAndCreateErrHashMap(t *testing.T) {
	var errHashMapTests = []struct {
		inMap    *map[string]*pb.Result
		inResult *pb.Result
		expected bool
	}{
		{
			&map[string]*pb.Result{mapStrOne: &resultA},
			&resultA,
			true,
		},
		{
			&map[string]*pb.Result{mapStrOne: &resultA},
			&resultB,
			true,
		},
		{
			&map[string]*pb.Result{mapStrOne: &resultA},
			&resultC,
			false,
		},
		{
			&map[string]*pb.Result{mapStrOne: &resultA, mapStrTwo: &resultC},
			&resultC,
			true,
		},
	}
	for _, tt := range errHashMapTests {
		actual := CheckAndCreateErrHashMap(tt.inMap, tt.inResult)
		if actual != tt.expected {
			t.Errorf("Expected: %v and actural: %v with result Path %s, Linenumber %d, ErrMsg %s",
				tt.expected, actual, tt.inResult.Path, tt.inResult.LineNumber, tt.inResult.ErrorMessage)
		}
	}
}

func TestDeleteRepeatedResults(t *testing.T) {
	var (
		in = pb.ResultsList{
			Results: []*pb.Result{
				&resultA,
				&resultB,
				&resultC,
				&resultD,
				&resultE,
				&resultF,
				&resultG,
				&resultH,
				&resultI,
			},
		}
		expected = pb.ResultsList{
			Results: []*pb.Result{
				&resultA,
				&resultC,
				&resultD,
				&resultE,
				&resultF,
				&resultG,
				&resultH,
				&resultI,
			},
		}
	)
	err := DeleteRepeatedResults(&in)
	if err != nil {
		t.Errorf("DeleteRepeatedResults: %v", err)
	}
	for i := 0; i < len(in.Results); i++ {
		if in.Results[i].Path != expected.Results[i].Path &&
			in.Results[i].LineNumber != expected.Results[i].LineNumber &&
			in.Results[i].ErrorMessage != expected.Results[i].ErrorMessage {
			t.Errorf("Wrong Result in position %d", i)
		}
	}
}
