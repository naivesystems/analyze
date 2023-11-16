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

package basic

import (
	"fmt"
	"reflect"

	"naive.systems/analyzer/misra/checker_integration/checkrule"
)

// Special args for some rules using Libtooling checker(related to checkrule.JSONOption)
const (
	kCaseSensitive                      string = "CaseSensitive"
	kLimit                              string = "Limit"
	kImplicitDecl                       string = "ImplicitDecl"
	kAggressiveMode                     string = "AggressiveMode"
	kMaximumInlineFuncLine              string = "MaximumInlineFuncLine"
	kProjectName                        string = "ProjectName"
	kMaximumAllowedFuncLine             string = "MaximumAllowedFuncLine"
	kMaximumAllowedReturnNum            string = "MaximumAllowedReturnNum"
	kReportErrorInCallingSystemFunction string = "ReportErrorInCallingSystemFunction"
	kStructMemberLimit                  string = "StructMemberLimit"
	kFunctionParmLimit                  string = "FunctionParmLimit"
	kFunctionArgLimit                   string = "FunctionArgLimit"
	kNestedRecordLimit                  string = "NestedRecordLimit"
	kNestedExprLimit                    string = "NestedExprLimit"
	kSwitchCaseLimit                    string = "SwitchCaseLimit"
	kEnumConstantLimit                  string = "EnumConstantLimit"
	kStringCharLimit                    string = "StringCharLimit"
	kExternIDLimit                      string = "ExternIDLimit"
	kMacroIDLimit                       string = "MacroIDLimit"
	kMacroParmLimit                     string = "MacroParmLimit"
	kMacroArgLimit                      string = "MacroArgLimit"
	kNestedBlockLimit                   string = "NestedBlockLimit"
	kNestedIncludeLimit                 string = "NestedIncludeLimit"
	kIoMIDCharLimit                     string = "IoMIDCharLimit" // IoM: Internal or Macro
	kNestedCondIncluLimit               string = "NestedCondIncluLimit"
	kBlockIDLimit                       string = "BlockIDLimit"
	kNestedDeclLimit                    string = "NestedDeclLimit"
	kModifyDeclLimit                    string = "ModifyDeclLimit"
)

var libtoolingExtraArgsMap = map[string][]string{
	"misra_c_2012/rule_1_1": {kStructMemberLimit, kFunctionParmLimit, kFunctionArgLimit,
		kNestedRecordLimit, kNestedExprLimit, kSwitchCaseLimit, kEnumConstantLimit,
		kStringCharLimit, kExternIDLimit, kCaseSensitive, kLimit, kMacroIDLimit,
		kMacroParmLimit, kMacroArgLimit, kNestedBlockLimit, kNestedIncludeLimit,
		kImplicitDecl, kIoMIDCharLimit, kNestedCondIncluLimit, kBlockIDLimit,
		kNestedDeclLimit, kModifyDeclLimit},
	"misra_c_2012/rule_5_1":  {kCaseSensitive, kLimit, kImplicitDecl},
	"misra_c_2012/rule_13_2": {kAggressiveMode},
	"misra_c_2012/rule_13_5": {kAggressiveMode},
	"misra/rule_1_1": {kStructMemberLimit, kFunctionParmLimit, kFunctionArgLimit,
		kNestedRecordLimit, kNestedExprLimit, kSwitchCaseLimit, kEnumConstantLimit,
		kStringCharLimit, kExternIDLimit, kCaseSensitive, kLimit, kMacroIDLimit,
		kMacroParmLimit, kMacroArgLimit, kNestedBlockLimit, kNestedIncludeLimit,
		kImplicitDecl, kIoMIDCharLimit, kNestedCondIncluLimit, kBlockIDLimit,
		kNestedDeclLimit, kModifyDeclLimit},
	"misra/rule_5_1":  {kCaseSensitive, kLimit, kImplicitDecl},
	"misra/rule_13_2": {kAggressiveMode},
	"misra/rule_13_5": {kAggressiveMode},
	"googlecpp/g1155": {kMaximumInlineFuncLine},
	"googlecpp/g1201": {kProjectName},
	"googlecpp/g1204": {kMaximumAllowedFuncLine, kMaximumAllowedReturnNum},
	"cwe/cwe_686":     {kReportErrorInCallingSystemFunction},
}

func FilterLibToolingExtraArgs(extraArgs *[]string, ruleName string, jsonOptions checkrule.JSONOption) {
	emptyArgsFlag := reflect.Value{} // Handling the "args: empty" case, FieldByName will generate a empty reflect.Value{}, but it can't be used in field.Elem()
	argsList := libtoolingExtraArgsMap[ruleName]
	v := reflect.ValueOf(jsonOptions)
	for _, argsName := range argsList {
		field := v.FieldByName(argsName)
		if field.IsValid() && field.Elem() != emptyArgsFlag {
			*extraArgs = append(*extraArgs, fmt.Sprintf("--%s=%v", argsName, field.Elem()))
		}
	}
}
