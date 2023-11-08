/*
Copyright 2023 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

package analyzer

import (
	"strings"

	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/googlecpp/g1149"
	"naive.systems/analyzer/googlecpp/g1150"
	"naive.systems/analyzer/googlecpp/g1151"
	"naive.systems/analyzer/googlecpp/g1152"
	"naive.systems/analyzer/googlecpp/g1153"
	"naive.systems/analyzer/googlecpp/g1154"
	"naive.systems/analyzer/googlecpp/g1155"
	"naive.systems/analyzer/googlecpp/g1156"
	"naive.systems/analyzer/googlecpp/g1157"
	"naive.systems/analyzer/googlecpp/g1158"
	"naive.systems/analyzer/googlecpp/g1159"
	"naive.systems/analyzer/googlecpp/g1160"
	"naive.systems/analyzer/googlecpp/g1161"
	"naive.systems/analyzer/googlecpp/g1162"
	"naive.systems/analyzer/googlecpp/g1163"
	"naive.systems/analyzer/googlecpp/g1164"
	"naive.systems/analyzer/googlecpp/g1165"
	"naive.systems/analyzer/googlecpp/g1166"
	"naive.systems/analyzer/googlecpp/g1167"
	"naive.systems/analyzer/googlecpp/g1168"
	"naive.systems/analyzer/googlecpp/g1169"
	"naive.systems/analyzer/googlecpp/g1170"
	"naive.systems/analyzer/googlecpp/g1171"
	"naive.systems/analyzer/googlecpp/g1172"
	"naive.systems/analyzer/googlecpp/g1173"
	"naive.systems/analyzer/googlecpp/g1174"
	"naive.systems/analyzer/googlecpp/g1175"
	"naive.systems/analyzer/googlecpp/g1176"
	"naive.systems/analyzer/googlecpp/g1177"
	"naive.systems/analyzer/googlecpp/g1178"
	"naive.systems/analyzer/googlecpp/g1179"
	"naive.systems/analyzer/googlecpp/g1180"
	"naive.systems/analyzer/googlecpp/g1181"
	"naive.systems/analyzer/googlecpp/g1182"
	"naive.systems/analyzer/googlecpp/g1183"
	"naive.systems/analyzer/googlecpp/g1184"
	"naive.systems/analyzer/googlecpp/g1185"
	"naive.systems/analyzer/googlecpp/g1186"
	"naive.systems/analyzer/googlecpp/g1187"
	"naive.systems/analyzer/googlecpp/g1188"
	"naive.systems/analyzer/googlecpp/g1189"
	"naive.systems/analyzer/googlecpp/g1190"
	"naive.systems/analyzer/googlecpp/g1191"
	"naive.systems/analyzer/googlecpp/g1192"
	"naive.systems/analyzer/googlecpp/g1193"
	"naive.systems/analyzer/googlecpp/g1194"
	"naive.systems/analyzer/googlecpp/g1195"
	"naive.systems/analyzer/googlecpp/g1196"
	"naive.systems/analyzer/googlecpp/g1197"
	"naive.systems/analyzer/googlecpp/g1198"
	"naive.systems/analyzer/googlecpp/g1199"
	"naive.systems/analyzer/googlecpp/g1200"
	"naive.systems/analyzer/googlecpp/g1201"
	"naive.systems/analyzer/googlecpp/g1202"
	"naive.systems/analyzer/googlecpp/g1203"
	"naive.systems/analyzer/googlecpp/g1204"
	"naive.systems/analyzer/googlecpp/g1205"
	"naive.systems/analyzer/googlecpp/g1206"
	"naive.systems/analyzer/googlecpp/g1207"
	"naive.systems/analyzer/googlecpp/g1208"
	"naive.systems/analyzer/googlecpp/g1209"
	"naive.systems/analyzer/googlecpp/g1210"
	"naive.systems/analyzer/googlecpp/g1211"
	"naive.systems/analyzer/googlecpp/g1212"
	"naive.systems/analyzer/googlecpp/g1213"
	"naive.systems/analyzer/googlecpp/g1214"
	"naive.systems/analyzer/googlecpp/g1215"
	"naive.systems/analyzer/googlecpp/g1216"
	"naive.systems/analyzer/googlecpp/g1269"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
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
		ruleName = strings.TrimPrefix(ruleName, "googlecpp/")
		switch ruleName {
		case "g1149":
			x(g1149.Analyze)
		case "g1150":
			x(g1150.Analyze)
		case "g1151":
			x(g1151.Analyze)
		case "g1152":
			x(g1152.Analyze)
		case "g1153":
			x(g1153.Analyze)
		case "g1154":
			x(g1154.Analyze)
		case "g1155":
			x(g1155.Analyze)
		case "g1156":
			x(g1156.Analyze)
		case "g1157":
			x(g1157.Analyze)
		case "g1158":
			x(g1158.Analyze)
		case "g1159":
			x(g1159.Analyze)
		case "g1160":
			x(g1160.Analyze)
		case "g1161":
			x(g1161.Analyze)
		case "g1162":
			x(g1162.Analyze)
		case "g1163":
			x(g1163.Analyze)
		case "g1164":
			x(g1164.Analyze)
		case "g1165":
			x(g1165.Analyze)
		case "g1166":
			x(g1166.Analyze)
		case "g1167":
			x(g1167.Analyze)
		case "g1168":
			x(g1168.Analyze)
		case "g1169":
			x(g1169.Analyze)
		case "g1170":
			x(g1170.Analyze)
		case "g1171":
			x(g1171.Analyze)
		case "g1172":
			x(g1172.Analyze)
		case "g1173":
			x(g1173.Analyze)
		case "g1174":
			x(g1174.Analyze)
		case "g1175":
			x(g1175.Analyze)
		case "g1176":
			x(g1176.Analyze)
		case "g1177":
			x(g1177.Analyze)
		case "g1178":
			x(g1178.Analyze)
		case "g1179":
			x(g1179.Analyze)
		case "g1180":
			x(g1180.Analyze)
		case "g1181":
			x(g1181.Analyze)
		case "g1182":
			x(g1182.Analyze)
		case "g1183":
			x(g1183.Analyze)
		case "g1184":
			x(g1184.Analyze)
		case "g1185":
			x(g1185.Analyze)
		case "g1186":
			x(g1186.Analyze)
		case "g1187":
			x(g1187.Analyze)
		case "g1188":
			x(g1188.Analyze)
		case "g1189":
			x(g1189.Analyze)
		case "g1190":
			x(g1190.Analyze)
		case "g1191":
			x(g1191.Analyze)
		case "g1192":
			x(g1192.Analyze)
		case "g1193":
			x(g1193.Analyze)
		case "g1194":
			x(g1194.Analyze)
		case "g1195":
			x(g1195.Analyze)
		case "g1196":
			x(g1196.Analyze)
		case "g1197":
			x(g1197.Analyze)
		case "g1198":
			x(g1198.Analyze)
		case "g1199":
			x(g1199.Analyze)
		case "g1200":
			x(g1200.Analyze)
		case "g1201":
			x(g1201.Analyze)
		case "g1202":
			x(g1202.Analyze)
		case "g1203":
			x(g1203.Analyze)
		case "g1204":
			x(g1204.Analyze)
		case "g1205":
			x(g1205.Analyze)
		case "g1206":
			x(g1206.Analyze)
		case "g1207":
			x(g1207.Analyze)
		case "g1208":
			x(g1208.Analyze)
		case "g1209":
			x(g1209.Analyze)
		case "g1210":
			x(g1210.Analyze)
		case "g1211":
			x(g1211.Analyze)
		case "g1212":
			x(g1212.Analyze)
		case "g1213":
			x(g1213.Analyze)
		case "g1214":
			x(g1214.Analyze)
		case "g1215":
			x(g1215.Analyze)
		case "g1216":
			x(g1216.Analyze)
		case "g1269":
			x(g1269.Analyze)
		}
	}
	return paraTaskRunner.CollectResultsAndErrors()
}
