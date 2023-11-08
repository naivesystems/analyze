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

package stats

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"time"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/atomic"
	"naive.systems/analyzer/cruleslib/severity"
)

// analysis stages
const (
	CC  int = iota // Compilation commands preparation
	PP             // Pre-processing
	CTU            // CTU information preparation
	STU            // STU information preparation
	AC             // Analysis check
	END
)

type Progress struct {
	StageID   int       `json:"stage_id"`
	DoneRatio string    `json:"done_ratio"`
	StartedAt time.Time `json:"started_at"`
}

type SeverityCount struct {
	Highest int `json:"highest"`
	High    int `json:"high"`
	Medium  int `json:"medium"`
	Low     int `json:"low"`
	Lowest  int `json:"lowest"`
	Unknown int `json:"unknown"`
}

func WriteLOC(resultDir string, linesCounter int) {
	path := filepath.Join(resultDir, "loc.nsa_metadata")
	err := atomic.Write(path, []byte(strconv.Itoa(linesCounter)))
	if err != nil {
		glog.Errorf("failed to write to file %s: %v", path, err)
	}
}

func WriteProgress(resultDir string, stageID int, doneRatio string, startedAt time.Time) {
	// skip writing it if resultDir does not exist
	_, err := os.Stat(resultDir)
	if os.IsNotExist(err) {
		glog.Warningf("result dir %s does not exist", resultDir)
		return
	}
	path := filepath.Join(resultDir, "progress.nsa_metadata")
	progress, err := json.Marshal(Progress{StageID: stageID, DoneRatio: doneRatio, StartedAt: startedAt})
	if err != nil {
		glog.Errorf("failed to marshal json stageID %d and doneRatio %s: %v", stageID, doneRatio, err)
		return
	}
	err = atomic.Write(path, progress)
	if err != nil {
		glog.Errorf("failed to write to file %s: %v", path, err)
	}
}

func AccumulateBySeverity(cnt *SeverityCount, resultSeverity int, resultID string) {
	switch resultSeverity {
	case int(severity.Unknown):
		cnt.Unknown++
	case int(severity.Highest):
		cnt.Highest++
	case int(severity.High):
		cnt.High++
	case int(severity.Medium):
		cnt.Medium++
	case int(severity.Low):
		cnt.Low++
	case int(severity.Lowest):
		cnt.Lowest++
	default:
		glog.Warningf("undefined severity of result %s", resultID)
	}
}

func GetSeverityCountBytes(resultsList *pb.ResultsList) ([]byte, error) {
	var cnt SeverityCount
	for _, result := range resultsList.Results {
		AccumulateBySeverity(&cnt, int(result.Severity), result.Id)
	}
	statsBytes, err := json.Marshal(cnt)
	if err != nil {
		return nil, fmt.Errorf("json.Marshal: %v", err)
	}
	return statsBytes, nil
}

func CountSeverityAndWrite(resultsList *pb.ResultsList, resultDir string) {
	statsBytes, err := GetSeverityCountBytes(resultsList)
	if err != nil {
		glog.Errorf("failed to get severity count bytes: %v", err)
	}
	statsFile := filepath.Join(resultDir, "severity_stats.nsa_metadata")
	err = atomic.Write(statsFile, statsBytes)
	if err != nil {
		glog.Errorf("failed to write to file %s: %v", statsFile, err)
	}
}
