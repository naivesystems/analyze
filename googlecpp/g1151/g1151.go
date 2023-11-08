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

package g1151

import (
	"os"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	final_results := &pb.ResultsList{}

	// Generate a random file name, and store it in the `JsonOption`. If it needs
	// to assign the file with a specific name, just replace the `OptionalInfoFile`
	// with the filename string you want.
	f, err := os.CreateTemp(opts.EnvOption.ResultsDir, "tmp-*.txt")
	if err != nil {
		return nil, err
	}
	opts.JsonOption.OptionalInfoFile = f.Name()
	f.Close()

	results_lib, err := runner.RunLibtooling(srcdir, "googlecpp/g1151", checker_integration.Libtooling_STU, opts)
	if err != nil {
		return nil, err
	}

	results_header_compiler, err := runner.RunHeaderCompile(srcdir, "googlecpp/g1151", opts)
	if err != nil {
		return nil, err
	}
	for _, r := range results_header_compiler.Results {
		r.ErrorMessage = "Header files should be self-contained (compile on their own) and end in .h"
		if r.ExternalMessage != "" {
			r.ErrorMessage += "\n" + r.ExternalMessage
		}
	}

	final_results.Results = runner.MergeResults(results_lib.Results, results_header_compiler.Results)
	return final_results, nil
}
