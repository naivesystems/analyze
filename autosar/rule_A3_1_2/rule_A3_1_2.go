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

package rule_A3_1_2

import (
	"bufio"
	"os"
	"regexp"

	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/misra/checker_integration/csa"
)

func checkHeader(buildActions *[]csa.BuildAction) (*pb.ResultsList, error) {
	resultsList := &pb.ResultsList{}

	for _, action := range *buildActions {
		path := action.Command.File
		file, err := os.Open(path)
		if err != nil {
			glog.Errorf("failed to open source file: %s, error: %v", path, err)
			return nil, err
		}

		scanner := bufio.NewScanner(file)
		lineno := 0

		regex := regexp.MustCompile(`^#include`)
		headersRegex := regexp.MustCompile(`"\w*\.(h|hpp|hxx)"`)
		for scanner.Scan() {
			line := scanner.Text()
			lineno++
			if regex.MatchString(line) {
				if !headersRegex.MatchString(line) {
					resultsList.Results = append(resultsList.Results, &pb.Result{
						Path:         path,
						LineNumber:   int32(lineno),
						ErrorMessage: "Header files, that are defined locally in the project, shall have a file name extension of one of: \".h\", \".hpp\" or \".hxx\".",
					})
				}
			}
		}
		err = file.Close()
		if err != nil {
			return nil, err
		}
	}

	return resultsList, nil
}

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	results, err := checkHeader(opts.EnvOption.BuildActions)
	if err != nil {
		glog.Error(err)
		return nil, err
	}
	return results, nil
}
