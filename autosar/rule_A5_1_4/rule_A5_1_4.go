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

package rule_A5_1_4

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	// This rule implements mainly in three steps.
	// Firstly, get all referenced captures from the code by libtooling.
	results, err := runner.RunLibtooling(srcdir, "autosar/rule_A5_1_4", checker_integration.Libtooling_CTU, opts)
	if err != nil {
		return nil, err
	}
	// Secondly, use CSA to find all potential stack address escape errors.
	results_csa, err := runner.RunCSA(srcdir, "-analyzer-checker=core.StackAddressEscape", opts)
	if err != nil {
		return nil, err
	}
	final_csa := runner.KeepNeededErrorByFilterTargetMsgInCSAReports(results_csa, "Address of stack memory associated with local variable '\\S+' is still referred to by the stack variable '\\S+' upon returning to the caller.  This will be a dangling reference", "A lambda expression object shall not outlive any of its reference-captured objects.")
	// Finally, compare the above two results, so the checker could filter the
	// libtooling results based on the CSA results, to find the real location
	// where exists the risks of memory leakage.
	final_results_list := []*pb.Result{}
	for _, result := range results.Results {
		for _, csa_result := range final_csa.Results {
			if result.GetFilename() == csa_result.GetFilename() && result.GetLineNumber() == csa_result.GetLineNumber() {
				final_results_list = append(final_results_list, result)
				break
			}
		}
	}
	results.Results = final_results_list
	return results, nil
}
