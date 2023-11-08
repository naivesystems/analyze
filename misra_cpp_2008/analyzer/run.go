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
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_10"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_11"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_12"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_7"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_8"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_1_9"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_4_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_0_4_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_1_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_10_3_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_11_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_12_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_12_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_12_1_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_12_8_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_12_8_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_5_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_5_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_6_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_6_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_7_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_7_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_7_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_8_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_14_8_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_0_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_0_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_1_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_3_7"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_5_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_15_5_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_7"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_0_8"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_2_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_2_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_2_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_2_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_2_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_16_6_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_17_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_17_0_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_17_0_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_17_0_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_17_0_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_0_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_0_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_0_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_0_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_18_7_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_19_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_1_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_1_0_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_1_0_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_27_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_10_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_10_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_10_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_10_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_10_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_10_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_13_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_13_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_13_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_13_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_13_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_7_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_7_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_2_7_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_1_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_2_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_2_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_2_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_9_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_9_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_3_9_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_4_10_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_4_10_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_4_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_4_5_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_4_5_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_10"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_11"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_12"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_13"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_14"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_15"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_16"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_17"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_18"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_19"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_20"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_21"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_7"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_8"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_0_9"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_14_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_17_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_18_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_19_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_10"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_11"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_12"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_7"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_8"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_2_9"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_3_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_3_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_5_8_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_2_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_2_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_7"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_4_8"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_5_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_5_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_5_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_5_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_5_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_6_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_6_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_6_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_6_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_6_6_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_1_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_1_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_2_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_3_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_3_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_3_5"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_3_6"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_4_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_4_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_5_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_5_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_7_5_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_0_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_4_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_4_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_4_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_4_4"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_5_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_8_5_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_3_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_3_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_3_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_5_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_6_1"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_6_2"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_6_3"
	"naive.systems/analyzer/misra_cpp_2008/rule_9_6_4"
)

func Run(rules []checkrule.CheckRule, srcdir string, envOpts *options.EnvOptions) (*pb.ResultsList, []error) {
	taskNums := len(rules)
	numWorkers := envOpts.NumWorkers
	paraTaskRunner := runner.NewParaTaskRunner(numWorkers, taskNums, envOpts.CheckProgress, envOpts.Lang)

	for i, rule := range rules {
		exiting_results, exiting_errors := paraTaskRunner.CheckSignalExiting()
		if exiting_results != nil {
			return exiting_results, exiting_errors
		}

		ruleSpecific := options.NewRuleSpecificOptions(rule.Name, envOpts.ResultsDir)
		ruleOptions := options.MakeCheckOptions(&rule.JSONOptions, envOpts, ruleSpecific)
		x := func(analyze func(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error)) {
			paraTaskRunner.AddTask(runner.AnalyzerTask{Id: i, Srcdir: srcdir, Opts: &ruleOptions, Rule: rule.Name, Analyze: analyze})
		}
		ruleName := rule.Name
		ruleName = strings.TrimPrefix(ruleName, "misra_cpp_2008/")
		switch ruleName {
		case "rule_0_1_1":
			x(rule_0_1_1.Analyze)
		case "rule_0_1_2":
			x(rule_0_1_2.Analyze)
		case "rule_0_1_3":
			x(rule_0_1_3.Analyze)
		case "rule_0_1_4":
			x(rule_0_1_4.Analyze)
		case "rule_0_1_5":
			x(rule_0_1_5.Analyze)
		case "rule_0_1_6":
			x(rule_0_1_6.Analyze)
		case "rule_0_1_7":
			x(rule_0_1_7.Analyze)
		case "rule_0_1_8":
			x(rule_0_1_8.Analyze)
		case "rule_0_1_9":
			x(rule_0_1_9.Analyze)
		case "rule_0_1_10":
			x(rule_0_1_10.Analyze)
		case "rule_0_1_11":
			x(rule_0_1_11.Analyze)
		case "rule_0_1_12":
			x(rule_0_1_12.Analyze)
		case "rule_0_2_1":
			x(rule_0_2_1.Analyze)
		case "rule_0_3_1":
			x(rule_0_3_1.Analyze)
		case "rule_0_3_2":
			x(rule_0_3_2.Analyze)
		case "rule_0_4_1":
			x(rule_0_4_1.Analyze)
		case "rule_0_4_2":
			x(rule_0_4_2.Analyze)
		case "rule_0_4_3":
			x(rule_0_4_3.Analyze)
		case "rule_1_0_1":
			x(rule_1_0_1.Analyze)
		case "rule_1_0_2":
			x(rule_1_0_2.Analyze)
		case "rule_1_0_3":
			x(rule_1_0_3.Analyze)
		case "rule_2_2_1":
			x(rule_2_2_1.Analyze)
		case "rule_2_3_1":
			x(rule_2_3_1.Analyze)
		case "rule_2_5_1":
			x(rule_2_5_1.Analyze)
		case "rule_2_7_1":
			x(rule_2_7_1.Analyze)
		case "rule_2_7_2":
			x(rule_2_7_2.Analyze)
		case "rule_2_7_3":
			x(rule_2_7_3.Analyze)
		case "rule_2_10_1":
			x(rule_2_10_1.Analyze)
		case "rule_2_10_2":
			x(rule_2_10_2.Analyze)
		case "rule_2_10_3":
			x(rule_2_10_3.Analyze)
		case "rule_2_10_4":
			x(rule_2_10_4.Analyze)
		case "rule_2_10_5":
			x(rule_2_10_5.Analyze)
		case "rule_2_10_6":
			x(rule_2_10_6.Analyze)
		case "rule_2_13_1":
			x(rule_2_13_1.Analyze)
		case "rule_2_13_2":
			x(rule_2_13_2.Analyze)
		case "rule_2_13_3":
			x(rule_2_13_3.Analyze)
		case "rule_2_13_4":
			x(rule_2_13_4.Analyze)
		case "rule_2_13_5":
			x(rule_2_13_5.Analyze)
		case "rule_3_1_1":
			x(rule_3_1_1.Analyze)
		case "rule_3_1_2":
			x(rule_3_1_2.Analyze)
		case "rule_3_1_3":
			x(rule_3_1_3.Analyze)
		case "rule_3_2_1":
			x(rule_3_2_1.Analyze)
		case "rule_3_2_2":
			x(rule_3_2_2.Analyze)
		case "rule_3_2_3":
			x(rule_3_2_3.Analyze)
		case "rule_3_2_4":
			x(rule_3_2_4.Analyze)
		case "rule_3_3_1":
			x(rule_3_3_1.Analyze)
		case "rule_3_3_2":
			x(rule_3_3_2.Analyze)
		case "rule_3_4_1":
			x(rule_3_4_1.Analyze)
		case "rule_3_9_1":
			x(rule_3_9_1.Analyze)
		case "rule_3_9_2":
			x(rule_3_9_2.Analyze)
		case "rule_3_9_3":
			x(rule_3_9_3.Analyze)
		case "rule_4_5_1":
			x(rule_4_5_1.Analyze)
		case "rule_4_5_2":
			x(rule_4_5_2.Analyze)
		case "rule_4_5_3":
			x(rule_4_5_3.Analyze)
		case "rule_4_10_1":
			x(rule_4_10_1.Analyze)
		case "rule_4_10_2":
			x(rule_4_10_2.Analyze)
		case "rule_5_0_1":
			x(rule_5_0_1.Analyze)
		case "rule_5_0_2":
			x(rule_5_0_2.Analyze)
		case "rule_5_0_3":
			x(rule_5_0_3.Analyze)
		case "rule_5_0_4":
			x(rule_5_0_4.Analyze)
		case "rule_5_0_5":
			x(rule_5_0_5.Analyze)
		case "rule_5_0_6":
			x(rule_5_0_6.Analyze)
		case "rule_5_0_7":
			x(rule_5_0_7.Analyze)
		case "rule_5_0_8":
			x(rule_5_0_8.Analyze)
		case "rule_5_0_9":
			x(rule_5_0_9.Analyze)
		case "rule_5_0_10":
			x(rule_5_0_10.Analyze)
		case "rule_5_0_11":
			x(rule_5_0_11.Analyze)
		case "rule_5_0_12":
			x(rule_5_0_12.Analyze)
		case "rule_5_0_13":
			x(rule_5_0_13.Analyze)
		case "rule_5_0_14":
			x(rule_5_0_14.Analyze)
		case "rule_5_0_15":
			x(rule_5_0_15.Analyze)
		case "rule_5_0_16":
			x(rule_5_0_16.Analyze)
		case "rule_5_0_17":
			x(rule_5_0_17.Analyze)
		case "rule_5_0_18":
			x(rule_5_0_18.Analyze)
		case "rule_5_0_19":
			x(rule_5_0_19.Analyze)
		case "rule_5_0_20":
			x(rule_5_0_20.Analyze)
		case "rule_5_0_21":
			x(rule_5_0_21.Analyze)
		case "rule_5_2_1":
			x(rule_5_2_1.Analyze)
		case "rule_5_2_2":
			x(rule_5_2_2.Analyze)
		case "rule_5_2_3":
			x(rule_5_2_3.Analyze)
		case "rule_5_2_4":
			x(rule_5_2_4.Analyze)
		case "rule_5_2_5":
			x(rule_5_2_5.Analyze)
		case "rule_5_2_6":
			x(rule_5_2_6.Analyze)
		case "rule_5_2_7":
			x(rule_5_2_7.Analyze)
		case "rule_5_2_8":
			x(rule_5_2_8.Analyze)
		case "rule_5_2_9":
			x(rule_5_2_9.Analyze)
		case "rule_5_2_10":
			x(rule_5_2_10.Analyze)
		case "rule_5_2_11":
			x(rule_5_2_11.Analyze)
		case "rule_5_2_12":
			x(rule_5_2_12.Analyze)
		case "rule_5_3_1":
			x(rule_5_3_1.Analyze)
		case "rule_5_3_2":
			x(rule_5_3_2.Analyze)
		case "rule_5_3_3":
			x(rule_5_3_3.Analyze)
		case "rule_5_3_4":
			x(rule_5_3_4.Analyze)
		case "rule_5_8_1":
			x(rule_5_8_1.Analyze)
		case "rule_5_14_1":
			x(rule_5_14_1.Analyze)
		case "rule_5_17_1":
			x(rule_5_17_1.Analyze)
		case "rule_5_18_1":
			x(rule_5_18_1.Analyze)
		case "rule_5_19_1":
			x(rule_5_19_1.Analyze)
		case "rule_6_2_1":
			x(rule_6_2_1.Analyze)
		case "rule_6_2_2":
			x(rule_6_2_2.Analyze)
		case "rule_6_2_3":
			x(rule_6_2_3.Analyze)
		case "rule_6_3_1":
			x(rule_6_3_1.Analyze)
		case "rule_6_4_1":
			x(rule_6_4_1.Analyze)
		case "rule_6_4_2":
			x(rule_6_4_2.Analyze)
		case "rule_6_4_3":
			x(rule_6_4_3.Analyze)
		case "rule_6_4_4":
			x(rule_6_4_4.Analyze)
		case "rule_6_4_5":
			x(rule_6_4_5.Analyze)
		case "rule_6_4_6":
			x(rule_6_4_6.Analyze)
		case "rule_6_4_7":
			x(rule_6_4_7.Analyze)
		case "rule_6_4_8":
			x(rule_6_4_8.Analyze)
		case "rule_6_5_1":
			x(rule_6_5_1.Analyze)
		case "rule_6_5_2":
			x(rule_6_5_2.Analyze)
		case "rule_6_5_3":
			x(rule_6_5_3.Analyze)
		case "rule_6_5_4":
			x(rule_6_5_4.Analyze)
		case "rule_6_5_5":
			x(rule_6_5_5.Analyze)
		case "rule_6_5_6":
			x(rule_6_5_6.Analyze)
		case "rule_6_6_1":
			x(rule_6_6_1.Analyze)
		case "rule_6_6_2":
			x(rule_6_6_2.Analyze)
		case "rule_6_6_3":
			x(rule_6_6_3.Analyze)
		case "rule_6_6_4":
			x(rule_6_6_4.Analyze)
		case "rule_6_6_5":
			x(rule_6_6_5.Analyze)
		case "rule_7_1_1":
			x(rule_7_1_1.Analyze)
		case "rule_7_1_2":
			x(rule_7_1_2.Analyze)
		case "rule_7_2_1":
			x(rule_7_2_1.Analyze)
		case "rule_7_3_1":
			x(rule_7_3_1.Analyze)
		case "rule_7_3_2":
			x(rule_7_3_2.Analyze)
		case "rule_7_3_3":
			x(rule_7_3_3.Analyze)
		case "rule_7_3_4":
			x(rule_7_3_4.Analyze)
		case "rule_7_3_5":
			x(rule_7_3_5.Analyze)
		case "rule_7_3_6":
			x(rule_7_3_6.Analyze)
		case "rule_7_4_1":
			x(rule_7_4_1.Analyze)
		case "rule_7_4_2":
			x(rule_7_4_2.Analyze)
		case "rule_7_4_3":
			x(rule_7_4_3.Analyze)
		case "rule_7_5_1":
			x(rule_7_5_1.Analyze)
		case "rule_7_5_2":
			x(rule_7_5_2.Analyze)
		case "rule_7_5_3":
			x(rule_7_5_3.Analyze)
		case "rule_7_5_4":
			x(rule_7_5_4.Analyze)
		case "rule_8_0_1":
			x(rule_8_0_1.Analyze)
		case "rule_8_3_1":
			x(rule_8_3_1.Analyze)
		case "rule_8_4_1":
			x(rule_8_4_1.Analyze)
		case "rule_8_4_2":
			x(rule_8_4_2.Analyze)
		case "rule_8_4_3":
			x(rule_8_4_3.Analyze)
		case "rule_8_4_4":
			x(rule_8_4_4.Analyze)
		case "rule_8_5_1":
			x(rule_8_5_1.Analyze)
		case "rule_8_5_2":
			x(rule_8_5_2.Analyze)
		case "rule_8_5_3":
			x(rule_8_5_3.Analyze)
		case "rule_9_3_1":
			x(rule_9_3_1.Analyze)
		case "rule_9_3_2":
			x(rule_9_3_2.Analyze)
		case "rule_9_3_3":
			x(rule_9_3_3.Analyze)
		case "rule_9_5_1":
			x(rule_9_5_1.Analyze)
		case "rule_9_6_1":
			x(rule_9_6_1.Analyze)
		case "rule_9_6_2":
			x(rule_9_6_2.Analyze)
		case "rule_9_6_3":
			x(rule_9_6_3.Analyze)
		case "rule_9_6_4":
			x(rule_9_6_4.Analyze)
		case "rule_10_1_1":
			x(rule_10_1_1.Analyze)
		case "rule_10_1_2":
			x(rule_10_1_2.Analyze)
		case "rule_10_1_3":
			x(rule_10_1_3.Analyze)
		case "rule_10_2_1":
			x(rule_10_2_1.Analyze)
		case "rule_10_3_1":
			x(rule_10_3_1.Analyze)
		case "rule_10_3_2":
			x(rule_10_3_2.Analyze)
		case "rule_10_3_3":
			x(rule_10_3_3.Analyze)
		case "rule_11_0_1":
			x(rule_11_0_1.Analyze)
		case "rule_12_1_1":
			x(rule_12_1_1.Analyze)
		case "rule_12_1_2":
			x(rule_12_1_2.Analyze)
		case "rule_12_1_3":
			x(rule_12_1_3.Analyze)
		case "rule_12_8_1":
			x(rule_12_8_1.Analyze)
		case "rule_12_8_2":
			x(rule_12_8_2.Analyze)
		case "rule_14_5_1":
			x(rule_14_5_1.Analyze)
		case "rule_14_5_2":
			x(rule_14_5_2.Analyze)
		case "rule_14_5_3":
			x(rule_14_5_3.Analyze)
		case "rule_14_6_1":
			x(rule_14_6_1.Analyze)
		case "rule_14_6_2":
			x(rule_14_6_2.Analyze)
		case "rule_14_7_1":
			x(rule_14_7_1.Analyze)
		case "rule_14_7_2":
			x(rule_14_7_2.Analyze)
		case "rule_14_7_3":
			x(rule_14_7_3.Analyze)
		case "rule_14_8_1":
			x(rule_14_8_1.Analyze)
		case "rule_14_8_2":
			x(rule_14_8_2.Analyze)
		case "rule_15_0_1":
			x(rule_15_0_1.Analyze)
		case "rule_15_0_2":
			x(rule_15_0_2.Analyze)
		case "rule_15_0_3":
			x(rule_15_0_3.Analyze)
		case "rule_15_1_1":
			x(rule_15_1_1.Analyze)
		case "rule_15_1_2":
			x(rule_15_1_2.Analyze)
		case "rule_15_1_3":
			x(rule_15_1_3.Analyze)
		case "rule_15_3_1":
			x(rule_15_3_1.Analyze)
		case "rule_15_3_2":
			x(rule_15_3_2.Analyze)
		case "rule_15_3_3":
			x(rule_15_3_3.Analyze)
		case "rule_15_3_4":
			x(rule_15_3_4.Analyze)
		case "rule_15_3_5":
			x(rule_15_3_5.Analyze)
		case "rule_15_3_6":
			x(rule_15_3_6.Analyze)
		case "rule_15_3_7":
			x(rule_15_3_7.Analyze)
		case "rule_15_4_1":
			x(rule_15_4_1.Analyze)
		case "rule_15_5_1":
			x(rule_15_5_1.Analyze)
		case "rule_15_5_2":
			x(rule_15_5_2.Analyze)
		case "rule_15_5_3":
			x(rule_15_5_3.Analyze)
		case "rule_16_0_1":
			x(rule_16_0_1.Analyze)
		case "rule_16_0_2":
			x(rule_16_0_2.Analyze)
		case "rule_16_0_3":
			x(rule_16_0_3.Analyze)
		case "rule_16_0_4":
			x(rule_16_0_4.Analyze)
		case "rule_16_0_5":
			x(rule_16_0_5.Analyze)
		case "rule_16_0_6":
			x(rule_16_0_6.Analyze)
		case "rule_16_0_7":
			x(rule_16_0_7.Analyze)
		case "rule_16_0_8":
			x(rule_16_0_8.Analyze)
		case "rule_16_1_1":
			x(rule_16_1_1.Analyze)
		case "rule_16_1_2":
			x(rule_16_1_2.Analyze)
		case "rule_16_2_1":
			x(rule_16_2_1.Analyze)
		case "rule_16_2_2":
			x(rule_16_2_2.Analyze)
		case "rule_16_2_3":
			x(rule_16_2_3.Analyze)
		case "rule_16_2_4":
			x(rule_16_2_4.Analyze)
		case "rule_16_2_5":
			x(rule_16_2_5.Analyze)
		case "rule_16_2_6":
			x(rule_16_2_6.Analyze)
		case "rule_16_3_1":
			x(rule_16_3_1.Analyze)
		case "rule_16_3_2":
			x(rule_16_3_2.Analyze)
		case "rule_16_6_1":
			x(rule_16_6_1.Analyze)
		case "rule_17_0_1":
			x(rule_17_0_1.Analyze)
		case "rule_17_0_2":
			x(rule_17_0_2.Analyze)
		case "rule_17_0_3":
			x(rule_17_0_3.Analyze)
		case "rule_17_0_4":
			x(rule_17_0_4.Analyze)
		case "rule_17_0_5":
			x(rule_17_0_5.Analyze)
		case "rule_18_0_1":
			x(rule_18_0_1.Analyze)
		case "rule_18_0_2":
			x(rule_18_0_2.Analyze)
		case "rule_18_0_3":
			x(rule_18_0_3.Analyze)
		case "rule_18_0_4":
			x(rule_18_0_4.Analyze)
		case "rule_18_0_5":
			x(rule_18_0_5.Analyze)
		case "rule_18_2_1":
			x(rule_18_2_1.Analyze)
		case "rule_18_4_1":
			x(rule_18_4_1.Analyze)
		case "rule_18_7_1":
			x(rule_18_7_1.Analyze)
		case "rule_19_3_1":
			x(rule_19_3_1.Analyze)
		case "rule_27_0_1":
			x(rule_27_0_1.Analyze)
		}
	}
	return paraTaskRunner.CollectResultsAndErrors()
}
