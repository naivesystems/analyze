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
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"naive.systems/analyzer/toy_rules/rule_1"
	"strings"
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
		ruleName = strings.TrimPrefix(ruleName, "toy_rules/")
		switch ruleName {
		case "rule_1":
			x(rule_1.Analyze)
		}
	}
	return paraTaskRunner.CollectResultsAndErrors()
}
