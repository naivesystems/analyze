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

package baseline

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"

	"github.com/golang/glog"
	git2go "github.com/libgit2/git2go/v33"
	pb "naive.systems/analyzer/analyzer/proto"
)

type Result struct {
	ErrorMessage string    `json:"errorMessage"`
	LineNumber   int32     `json:"lineNumber"`
	Locations    Locations `json:"locations"`
}

type Baseline struct {
	Results    []Result `json:"results"`
	CommitHash string   `json:"commitHash"`
}

type GitObject struct {
	Repo               *git2go.Repository
	CurrentCommitTree  *git2go.Tree
	BaselineCommitTree *git2go.Tree
}

func CreateBaselineFile(allResults *pb.ResultsList, resultsDir, currentCommitHash string) error {
	baslinePath := filepath.Join(resultsDir, "baseline.json")
	var baseline Baseline
	baseline.CommitHash = currentCommitHash
	baseline.Results = []Result{}
	for _, result := range allResults.Results {
		var currentResult Result
		currentResult.LineNumber = result.LineNumber
		currentResult.ErrorMessage = result.ErrorMessage
		currentResult.Locations = result.Locations
		baseline.Results = append(baseline.Results, currentResult)
	}
	out, err := json.MarshalIndent(baseline, "", "\t")
	if err != nil {
		return fmt.Errorf("Cannot stringify baseline")
	}

	err = os.WriteFile(baslinePath, out, os.ModePerm)
	if err != nil {
		return fmt.Errorf("Cannot write baseline.json")
	}

	return nil
}

func GetBaseline(baselinePath string) (Baseline, error) {
	var baseline Baseline
	baselineFile, err := os.Open(baselinePath)
	if err != nil {
		return baseline, fmt.Errorf("Cannot open baseline.json")
	}
	defer baselineFile.Close()
	baselineContent, err := io.ReadAll(baselineFile)
	if err != nil {
		return baseline, fmt.Errorf("Cannot read baseline.json")
	}
	err = json.Unmarshal(baselineContent, &baseline)
	if err != nil {
		return baseline, fmt.Errorf("Cannot parse baseline.json")
	}
	return baseline, nil
}

func GetHeadCommitHash(workingDir string) (string, error) {
	cmd := exec.Command("git", "rev-parse", "HEAD")
	cmd.Dir = workingDir
	out, err := cmd.CombinedOutput()
	strOut := string(out)
	if err != nil {
		return "", fmt.Errorf(string(strOut))
	}
	return strings.TrimSuffix(strOut, "\n"), nil
}

func GetGitObject(baseline Baseline, currentCommitHash, workingDir string) (*GitObject, error) {
	currentOid, err := git2go.NewOid(currentCommitHash)
	if err != nil {
		return nil, fmt.Errorf("git2go.NewOid failed: %v", err)
	}
	baselineOid, err := git2go.NewOid(baseline.CommitHash)
	if err != nil {
		return nil, fmt.Errorf("git2go.NewOid failed: %v", err)
	}
	repo, err := git2go.OpenRepository(workingDir)
	if err != nil {
		return nil, fmt.Errorf("git2go.OpenRepository failed: %v", err)
	}
	currentCommit, err := repo.LookupCommit(currentOid)
	if err != nil {
		return nil, fmt.Errorf("git2go.LookupCommit failed: %v", err)
	}
	baselineCommit, err := repo.LookupCommit(baselineOid)
	if err != nil {
		return nil, fmt.Errorf("git2go.LookupCommit failed: %v", err)
	}

	currentCommitTree, err := currentCommit.Tree()
	if err != nil {
		return nil, fmt.Errorf("currentCommit.Tree() failed: %v", err)
	}
	baselineCommitTree, err := baselineCommit.Tree()
	if err != nil {
		return nil, fmt.Errorf("baselineCommit.Tree() failed: %v", err)
	}
	gitObject := &GitObject{}
	gitObject.Repo = repo
	gitObject.CurrentCommitTree = currentCommitTree
	gitObject.BaselineCommitTree = baselineCommitTree
	return gitObject, nil
}

func GetHunksFromIssueDiff(issueDiff *git2go.Diff) []git2go.DiffHunk {
	issueHunks := make([]git2go.DiffHunk, 0)
	err := issueDiff.ForEach(func(file git2go.DiffDelta, progress float64) (git2go.DiffForEachHunkCallback, error) {
		return func(issueHunk git2go.DiffHunk) (git2go.DiffForEachLineCallback, error) {
			issueHunks = append(issueHunks, issueHunk)
			return func(issueLine git2go.DiffLine) error {
				return nil
			}, nil
		}, nil
	}, git2go.DiffDetailLines)
	if err != nil {
		glog.Error(err)
		return nil
	}
	return issueHunks
}

func inHunk(linenumber, start, lines int) bool {
	if linenumber >= start && linenumber < start+lines {
		return true
	}
	return false
}

func aboveHunk(linenumber, start, lines int) bool {
	if lines == 0 {
		return linenumber <= start
	}
	return linenumber < start
}

func underHunk(linenumber, start, lines int) bool {
	if lines == 0 {
		return linenumber > start
	}
	return linenumber >= start+lines
}

func CompareIssuesThroughHunks(newline, oldline int, hunks []git2go.DiffHunk) bool {
	newPrev := 0 // the start line of previous same block
	oldPrev := 0 // the start line of previous same block
	for _, hunk := range hunks {
		if inHunk(newline, hunk.NewStart, hunk.NewLines) {
			return false
		} else if aboveHunk(newline, hunk.NewStart, hunk.NewLines) {
			if aboveHunk(oldline, hunk.OldStart, hunk.OldLines) && newline-newPrev == oldline-oldPrev {
				return true
			}
			return false
		} else if !underHunk(oldline, hunk.OldStart, hunk.OldLines) {
			return false
		}
		newPrev = hunk.NewStart + hunk.NewLines
		if hunk.NewLines > 0 {
			newPrev -= 1
		}
		oldPrev = hunk.OldStart + hunk.OldLines
		if hunk.OldLines > 0 {
			oldPrev -= 1
		}
	}
	return newline-newPrev == oldline-oldPrev
}

func IsSameCode(gitObject *GitObject, curLocations, oldLocations Locations, workingDir string) bool {
	for idx, curLoc := range curLocations {
		options := &git2go.DiffOptions{}
		options.Pathspec = []string{strings.TrimPrefix(strings.TrimPrefix(curLoc.Path, workingDir), "/")}
		options.ContextLines = 0
		issueDiff, err := gitObject.Repo.DiffTreeToTree(gitObject.BaselineCommitTree, gitObject.CurrentCommitTree, options)
		if err != nil {
			glog.Errorf("DiffTreeToTree failed: %v", err)
			return false
		}
		issueHunks := GetHunksFromIssueDiff(issueDiff)
		if !CompareIssuesThroughHunks(int(curLoc.LineNumber), int(oldLocations[idx].LineNumber), issueHunks) {
			return false
		}
	}

	return true
}

func IsSameRule(curErrorMessage, oldErrorMessage string) bool {
	return strings.Split(curErrorMessage, "]")[0] == strings.Split(oldErrorMessage, "]")[0]
}

func IsSamePath(curLocations, oldLocations Locations) bool {
	for idx, curLoc := range curLocations {
		if curLoc.Path != oldLocations[idx].Path {
			return false
		}
	}
	return true
}

func RemoveDuplicatedResults(allResults *pb.ResultsList, workingDir, configDir, resultsDir string) *pb.ResultsList {
	baselinePath := filepath.Join(configDir, "baseline.json")

	cmd := exec.Command("git", "--version")
	if err := cmd.Run(); err != nil {
		glog.Warningf("Cannot find git. Add git to PATH or set option use_git to false")
		return allResults
	}

	cmd = exec.Command("git", "log")
	if err := cmd.Run(); err != nil {
		glog.Warningf("This is not a git repo.")
		return allResults
	}

	currentCommitHash, err := GetHeadCommitHash(workingDir)
	if err != nil {
		glog.Errorf("%v", err)
		return allResults
	}
	_, err = os.Stat(baselinePath)
	if err != nil {
		if os.IsNotExist(err) {
			err = CreateBaselineFile(allResults, resultsDir, currentCommitHash)
			if err != nil {
				glog.Errorf("%v", err)
			}
		} else {
			glog.Errorf("%v", err)
		}
		return allResults
	}

	baseline, err := GetBaseline(baselinePath)
	if err != nil {
		glog.Errorf("%v", err)
		return allResults
	}

	gitObject, err := GetGitObject(baseline, currentCommitHash, workingDir)
	if err != nil {
		glog.Errorf("%v", err)
		return allResults
	}
	newResults := make([]*pb.Result, 0)
	for _, currentResult := range allResults.Results {
		var curLocations Locations = currentResult.Locations
		sort.Sort(curLocations)
		isDuplicated := false
		for _, baselineResult := range baseline.Results {
			oldLocations := baselineResult.Locations
			sort.Sort(oldLocations)
			if len(oldLocations) != len(curLocations) {
				continue
			}
			if IsSameRule(currentResult.ErrorMessage, baselineResult.ErrorMessage) &&
				IsSamePath(curLocations, oldLocations) &&
				IsSameCode(gitObject, curLocations, oldLocations, workingDir) {
				isDuplicated = true
				break
			}
		}
		if !isDuplicated {
			newResults = append(newResults, currentResult)
		}
	}
	allResults.Results = newResults
	return allResults
}

func SortResults(allResults *pb.ResultsList) {
	sort.Slice(allResults.Results, func(i, j int) bool {
		x := allResults.Results[i]
		y := allResults.Results[j]
		if x.Path < y.Path {
			return true
		}
		if x.Path > y.Path {
			return false
		}
		if x.LineNumber < y.LineNumber {
			return true
		}
		if x.LineNumber > y.LineNumber {
			return false
		}
		return x.ErrorMessage < y.ErrorMessage
	})
}
