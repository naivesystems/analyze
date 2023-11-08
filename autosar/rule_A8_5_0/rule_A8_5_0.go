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

package rule_A8_5_0

import (
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
)

func Analyze(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error) {
	checkers := []string{
		"core.UndefinedBinaryOperatorResult",
		"core.CallAndMessage",
		"core.NullDereference",
		"core.uninitialized",
		"cplusplus.InnerPointer",
		"cwe.MissInit",
	} // refers to cwe-456 and misra-c2012-9.1

	result_csa, err := runner.RunCSA(srcdir, "-analyzer-checker="+strings.Join(checkers[:], ","), opts)
	if err != nil {
		return nil, err
	}
	final_csa := runner.KeepNeededErrorByFilterTargetMsgInCSAReports(result_csa, "[undefined|uninitialized|garbage]", "All memory shall be initialized before it is read.")
	return final_csa, nil
}
