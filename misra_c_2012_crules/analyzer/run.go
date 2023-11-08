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
	"github.com/golang/glog"
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/misra_c_2012_crules/C7966"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_10"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_11"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_12"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_14"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_3"
	"naive.systems/analyzer/misra_c_2012_crules/dir_4_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_10_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_11_9"
	"naive.systems/analyzer/misra_c_2012_crules/rule_12_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_12_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_12_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_12_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_12_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_13_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_13_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_13_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_13_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_13_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_13_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_14_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_14_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_14_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_14_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_15_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_16_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_17_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_18_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_19_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_19_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_1_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_1_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_1_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_1_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_10"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_11"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_12"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_13"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_14"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_20_9"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_10"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_11"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_12"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_13"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_14"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_15"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_16"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_17"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_18"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_19"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_20"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_21"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_21_9"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_10"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_22_9"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_2_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_3_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_3_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_4_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_4_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_5_9"
	"naive.systems/analyzer/misra_c_2012_crules/rule_6_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_6_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_7_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_7_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_7_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_7_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_10"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_11"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_12"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_13"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_14"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_5"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_6"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_7"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_8"
	"naive.systems/analyzer/misra_c_2012_crules/rule_8_9"
	"naive.systems/analyzer/misra_c_2012_crules/rule_9_1"
	"naive.systems/analyzer/misra_c_2012_crules/rule_9_2"
	"naive.systems/analyzer/misra_c_2012_crules/rule_9_3"
	"naive.systems/analyzer/misra_c_2012_crules/rule_9_4"
	"naive.systems/analyzer/misra_c_2012_crules/rule_9_5"
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
		switch rule.Name {
		case "misra_c_2012/dir_4_3":
			x(dir_4_3.Analyze)
		case "misra_c_2012/dir_4_7":
			x(dir_4_7.Analyze)
		case "misra_c_2012/dir_4_10":
			x(dir_4_10.Analyze)
		case "misra_c_2012/dir_4_11":
			x(dir_4_11.Analyze)
		case "misra_c_2012/dir_4_12":
			x(dir_4_12.Analyze)
		case "misra_c_2012/dir_4_14":
			x(dir_4_14.Analyze)
		case "misra_c_2012/rule_1_1":
			x(rule_1_1.Analyze)
		case "misra_c_2012/rule_1_2":
			x(rule_1_2.Analyze)
		case "misra_c_2012/rule_1_3":
			x(rule_1_3.Analyze)
		case "misra_c_2012/rule_1_4":
			x(rule_1_4.Analyze)
		case "misra_c_2012/rule_2_1":
			x(rule_2_1.Analyze)
		case "misra_c_2012/rule_2_2":
			x(rule_2_2.Analyze)
		case "misra_c_2012/rule_2_3":
			x(rule_2_3.Analyze)
		case "misra_c_2012/rule_2_4":
			x(rule_2_4.Analyze)
		case "misra_c_2012/rule_2_5":
			x(rule_2_5.Analyze)
		case "misra_c_2012/rule_2_6":
			x(rule_2_6.Analyze)
		case "misra_c_2012/rule_2_7":
			x(rule_2_7.Analyze)
		case "misra_c_2012/rule_3_1":
			x(rule_3_1.Analyze)
		case "misra_c_2012/rule_3_2":
			x(rule_3_2.Analyze)
		case "misra_c_2012/rule_4_1":
			x(rule_4_1.Analyze)
		case "misra_c_2012/rule_4_2":
			x(rule_4_2.Analyze)
		case "misra_c_2012/rule_5_1":
			x(rule_5_1.Analyze)
		case "misra_c_2012/rule_5_2":
			x(rule_5_2.Analyze)
		case "misra_c_2012/rule_5_3":
			x(rule_5_3.Analyze)
		case "misra_c_2012/rule_5_4":
			x(rule_5_4.Analyze)
		case "misra_c_2012/rule_5_5":
			x(rule_5_5.Analyze)
		case "misra_c_2012/rule_5_6":
			x(rule_5_6.Analyze)
		case "misra_c_2012/rule_5_7":
			x(rule_5_7.Analyze)
		case "misra_c_2012/rule_5_8":
			x(rule_5_8.Analyze)
		case "misra_c_2012/rule_5_9":
			x(rule_5_9.Analyze)
		case "misra_c_2012/rule_6_1":
			x(rule_6_1.Analyze)
		case "misra_c_2012/rule_6_2":
			x(rule_6_2.Analyze)
		case "misra_c_2012/rule_7_1":
			x(rule_7_1.Analyze)
		case "misra_c_2012/rule_7_2":
			x(rule_7_2.Analyze)
		case "misra_c_2012/rule_7_3":
			x(rule_7_3.Analyze)
		case "misra_c_2012/rule_7_4":
			x(rule_7_4.Analyze)
		case "misra_c_2012/rule_8_1":
			x(rule_8_1.Analyze)
		case "misra_c_2012/rule_8_2":
			x(rule_8_2.Analyze)
		case "misra_c_2012/rule_8_3":
			x(rule_8_3.Analyze)
		case "misra_c_2012/rule_8_4":
			x(rule_8_4.Analyze)
		case "misra_c_2012/rule_8_5":
			x(rule_8_5.Analyze)
		case "misra_c_2012/rule_8_6":
			x(rule_8_6.Analyze)
		case "misra_c_2012/rule_8_7":
			x(rule_8_7.Analyze)
		case "misra_c_2012/rule_8_8":
			x(rule_8_8.Analyze)
		case "misra_c_2012/rule_8_9":
			x(rule_8_9.Analyze)
		case "misra_c_2012/rule_8_10":
			x(rule_8_10.Analyze)
		case "misra_c_2012/rule_8_11":
			x(rule_8_11.Analyze)
		case "misra_c_2012/rule_8_12":
			x(rule_8_12.Analyze)
		case "misra_c_2012/rule_8_13":
			x(rule_8_13.Analyze)
		case "misra_c_2012/rule_8_14":
			x(rule_8_14.Analyze)
		case "misra_c_2012/rule_9_1":
			x(rule_9_1.Analyze)
		case "misra_c_2012/rule_9_2":
			x(rule_9_2.Analyze)
		case "misra_c_2012/rule_9_3":
			x(rule_9_3.Analyze)
		case "misra_c_2012/rule_9_4":
			x(rule_9_4.Analyze)
		case "misra_c_2012/rule_9_5":
			x(rule_9_5.Analyze)
		case "misra_c_2012/rule_10_1":
			x(rule_10_1.Analyze)
		case "misra_c_2012/rule_10_2":
			x(rule_10_2.Analyze)
		case "misra_c_2012/rule_10_3":
			x(rule_10_3.Analyze)
		case "misra_c_2012/rule_10_4":
			x(rule_10_4.Analyze)
		case "misra_c_2012/rule_10_5":
			x(rule_10_5.Analyze)
		case "misra_c_2012/rule_10_6":
			x(rule_10_6.Analyze)
		case "misra_c_2012/rule_10_7":
			x(rule_10_7.Analyze)
		case "misra_c_2012/rule_10_8":
			x(rule_10_8.Analyze)
		case "misra_c_2012/rule_11_1":
			x(rule_11_1.Analyze)
		case "misra_c_2012/rule_11_2":
			x(rule_11_2.Analyze)
		case "misra_c_2012/rule_11_3":
			x(rule_11_3.Analyze)
		case "misra_c_2012/rule_11_4":
			x(rule_11_4.Analyze)
		case "misra_c_2012/rule_11_5":
			x(rule_11_5.Analyze)
		case "misra_c_2012/rule_11_6":
			x(rule_11_6.Analyze)
		case "misra_c_2012/rule_11_7":
			x(rule_11_7.Analyze)
		case "misra_c_2012/rule_11_8":
			x(rule_11_8.Analyze)
		case "misra_c_2012/rule_11_9":
			x(rule_11_9.Analyze)
		case "misra_c_2012/rule_12_1":
			x(rule_12_1.Analyze)
		case "misra_c_2012/rule_12_2":
			x(rule_12_2.Analyze)
		case "misra_c_2012/rule_12_3":
			x(rule_12_3.Analyze)
		case "misra_c_2012/rule_12_4":
			x(rule_12_4.Analyze)
		case "misra_c_2012/rule_12_5":
			x(rule_12_5.Analyze)
		case "misra_c_2012/rule_13_1":
			x(rule_13_1.Analyze)
		case "misra_c_2012/rule_13_2":
			x(rule_13_2.Analyze)
		case "misra_c_2012/rule_13_3":
			x(rule_13_3.Analyze)
		case "misra_c_2012/rule_13_4":
			x(rule_13_4.Analyze)
		case "misra_c_2012/rule_13_5":
			x(rule_13_5.Analyze)
		case "misra_c_2012/rule_13_6":
			x(rule_13_6.Analyze)
		case "misra_c_2012/rule_14_1":
			x(rule_14_1.Analyze)
		case "misra_c_2012/rule_14_2":
			x(rule_14_2.Analyze)
		case "misra_c_2012/rule_14_3":
			x(rule_14_3.Analyze)
		case "misra_c_2012/rule_14_4":
			x(rule_14_4.Analyze)
		case "misra_c_2012/rule_15_1":
			x(rule_15_1.Analyze)
		case "misra_c_2012/rule_15_2":
			x(rule_15_2.Analyze)
		case "misra_c_2012/rule_15_3":
			x(rule_15_3.Analyze)
		case "misra_c_2012/rule_15_4":
			x(rule_15_4.Analyze)
		case "misra_c_2012/rule_15_5":
			x(rule_15_5.Analyze)
		case "misra_c_2012/rule_15_6":
			x(rule_15_6.Analyze)
		case "misra_c_2012/rule_15_7":
			x(rule_15_7.Analyze)
		case "misra_c_2012/rule_16_1":
			x(rule_16_1.Analyze)
		case "misra_c_2012/rule_16_2":
			x(rule_16_2.Analyze)
		case "misra_c_2012/rule_16_3":
			x(rule_16_3.Analyze)
		case "misra_c_2012/rule_16_4":
			x(rule_16_4.Analyze)
		case "misra_c_2012/rule_16_5":
			x(rule_16_5.Analyze)
		case "misra_c_2012/rule_16_6":
			x(rule_16_6.Analyze)
		case "misra_c_2012/rule_16_7":
			x(rule_16_7.Analyze)
		case "misra_c_2012/rule_17_1":
			x(rule_17_1.Analyze)
		case "misra_c_2012/rule_17_2":
			x(rule_17_2.Analyze)
		case "misra_c_2012/rule_17_3":
			x(rule_17_3.Analyze)
		case "misra_c_2012/rule_17_4":
			x(rule_17_4.Analyze)
		case "misra_c_2012/rule_17_5":
			x(rule_17_5.Analyze)
		case "misra_c_2012/rule_17_6":
			x(rule_17_6.Analyze)
		case "misra_c_2012/rule_17_7":
			x(rule_17_7.Analyze)
		case "misra_c_2012/rule_17_8":
			x(rule_17_8.Analyze)
		case "misra_c_2012/rule_18_1":
			x(rule_18_1.Analyze)
		case "misra_c_2012/rule_18_2":
			x(rule_18_2.Analyze)
		case "misra_c_2012/rule_18_3":
			x(rule_18_3.Analyze)
		case "misra_c_2012/rule_18_4":
			x(rule_18_4.Analyze)
		case "misra_c_2012/rule_18_5":
			x(rule_18_5.Analyze)
		case "misra_c_2012/rule_18_6":
			x(rule_18_6.Analyze)
		case "misra_c_2012/rule_18_7":
			x(rule_18_7.Analyze)
		case "misra_c_2012/rule_18_8":
			x(rule_18_8.Analyze)
		case "misra_c_2012/rule_19_1":
			x(rule_19_1.Analyze)
		case "misra_c_2012/rule_19_2":
			x(rule_19_2.Analyze)
		case "misra_c_2012/rule_20_1":
			x(rule_20_1.Analyze)
		case "misra_c_2012/rule_20_2":
			x(rule_20_2.Analyze)
		case "misra_c_2012/rule_20_3":
			x(rule_20_3.Analyze)
		case "misra_c_2012/rule_20_4":
			x(rule_20_4.Analyze)
		case "misra_c_2012/rule_20_5":
			x(rule_20_5.Analyze)
		case "misra_c_2012/rule_20_6":
			x(rule_20_6.Analyze)
		case "misra_c_2012/rule_20_7":
			x(rule_20_7.Analyze)
		case "misra_c_2012/rule_20_8":
			x(rule_20_8.Analyze)
		case "misra_c_2012/rule_20_9":
			x(rule_20_9.Analyze)
		case "misra_c_2012/rule_20_10":
			x(rule_20_10.Analyze)
		case "misra_c_2012/rule_20_11":
			x(rule_20_11.Analyze)
		case "misra_c_2012/rule_20_12":
			x(rule_20_12.Analyze)
		case "misra_c_2012/rule_20_13":
			x(rule_20_13.Analyze)
		case "misra_c_2012/rule_20_14":
			x(rule_20_14.Analyze)
		case "misra_c_2012/rule_21_1":
			x(rule_21_1.Analyze)
		case "misra_c_2012/rule_21_2":
			x(rule_21_2.Analyze)
		case "misra_c_2012/rule_21_3":
			x(rule_21_3.Analyze)
		case "misra_c_2012/rule_21_4":
			x(rule_21_4.Analyze)
		case "misra_c_2012/rule_21_5":
			x(rule_21_5.Analyze)
		case "misra_c_2012/rule_21_6":
			x(rule_21_6.Analyze)
		case "misra_c_2012/rule_21_7":
			x(rule_21_7.Analyze)
		case "misra_c_2012/rule_21_8":
			x(rule_21_8.Analyze)
		case "misra_c_2012/rule_21_9":
			x(rule_21_9.Analyze)
		case "misra_c_2012/rule_21_10":
			x(rule_21_10.Analyze)
		case "misra_c_2012/rule_21_11":
			x(rule_21_11.Analyze)
		case "misra_c_2012/rule_21_12":
			x(rule_21_12.Analyze)
		case "misra_c_2012/rule_21_13":
			x(rule_21_13.Analyze)
		case "misra_c_2012/rule_21_14":
			x(rule_21_14.Analyze)
		case "misra_c_2012/rule_21_15":
			x(rule_21_15.Analyze)
		case "misra_c_2012/rule_21_16":
			x(rule_21_16.Analyze)
		case "misra_c_2012/rule_21_17":
			x(rule_21_17.Analyze)
		case "misra_c_2012/rule_21_18":
			x(rule_21_18.Analyze)
		case "misra_c_2012/rule_21_19":
			x(rule_21_19.Analyze)
		case "misra_c_2012/rule_21_20":
			x(rule_21_20.Analyze)
		case "misra_c_2012/rule_21_21":
			x(rule_21_21.Analyze)
		case "misra_c_2012/rule_22_1":
			x(rule_22_1.Analyze)
		case "misra_c_2012/rule_22_2":
			x(rule_22_2.Analyze)
		case "misra_c_2012/rule_22_3":
			x(rule_22_3.Analyze)
		case "misra_c_2012/rule_22_4":
			x(rule_22_4.Analyze)
		case "misra_c_2012/rule_22_5":
			x(rule_22_5.Analyze)
		case "misra_c_2012/rule_22_6":
			x(rule_22_6.Analyze)
		case "misra_c_2012/rule_22_7":
			x(rule_22_7.Analyze)
		case "misra_c_2012/rule_22_8":
			x(rule_22_8.Analyze)
		case "misra_c_2012/rule_22_9":
			x(rule_22_9.Analyze)
		case "misra_c_2012/rule_22_10":
			x(rule_22_10.Analyze)
		case "naivesystems/C7966":
			x(C7966.Analyze)
		default:
			glog.Errorf("Unknown rule name: %s", rule.Name)
		}
	}
	return paraTaskRunner.CollectResultsAndErrors()
}
