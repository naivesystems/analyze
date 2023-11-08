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

package checkrule

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
	"gopkg.in/yaml.v2"
)

type FilterAndSinkFuncList []FilterAndSinkFunc
type SourceFuncList []SourceFunc

const emptyString = ""

type CheckRule struct {
	Name        string
	JSONOptions JSONOption
}

type JSONOption struct {
	CaseSensitive      *bool                  `json:"case-sensitive,omitempty" yaml:"-"`
	Limit              *int                   `json:"limit,omitempty" yaml:"-"`
	Standard           string                 `json:"standard,omitempty" yaml:"-"`
	MaxLoop            *int                   `json:"max-loop,omitempty" yaml:"-"`
	Filters            *FilterAndSinkFuncList `json:"taintFilters,omitempty" yaml:"Filters,omitempty"`
	Sources            *SourceFuncList        `json:"taintSources,omitempty" yaml:"Propagations,omitempty"`
	Sinks              *FilterAndSinkFuncList `json:"taintSinks,omitempty" yaml:"Sinks,omitempty"`
	CheckIncludeGuards *bool                  `json:"check-include-guards,omitempty" yaml:"-"`
	MaxReportNum       *int                   `json:"max-report-num,omitempty" yaml:"-"`
	AggressiveMode     *bool                  `json:"aggressive-mode,omitempty" yaml:"-"`
	// MaximumAllowedSize specifies the maximal allowed size of memory allocation in MiB. For CWE-789 only. Default to be 4096.
	MaximumAllowedSize *int `json:"maximum-allowed-size,omitempty" yaml:"-"`
	// CSAAnalyzerInliningMode determines the heuristic used to select functions for inlining.
	// This can take on two values as defined in third_party/llvm-project/clang/include/clang/StaticAnalyzer/Core/Analyses.def:
	// 1. all: analyze all functions as top-level.
	// 2. noredundancy: avoid analyzing a function that has already been inlined.
	CSAAnalyzerInliningMode            *string  `json:"csa-analyzer-inlining-mode,omitempty" yaml:"-"`
	UseInfer                           bool     `json:"use-infer,omitempty" yaml:"-"`
	UseCSA                             bool     `json:"use-csa,omitempty" yaml:"-"`
	UseZ3                              string   `json:"use-z3,omitempty" yaml:"-"`
	MaximumInlineFuncLine              *int     `json:"maximum-inline-func-line,omitempty" yaml:"-"`   //googlecpp/g1155
	ProjectName                        *string  `json:"project-name,omitempty" yaml:"-"`               //googlecpp/g1201
	MaximumAllowedFuncLine             *int     `json:"maximum-allowed-func-line,omitempty" yaml:"-"`  //googlecpp/g1204
	MaximumAllowedReturnNum            *int     `json:"maximum-allowed-return-num,omitempty" yaml:"-"` //googlecpp/g1204
	RootDir                            string   `json:"root-dir" yaml:"-"`                             //googlecpp/g1153
	ClangTidyArgsString                []string `json:"clang-tidy-args" yaml:"-"`
	OptionalInfoFile                   string   `json:"optional-info-file" yaml:"-"`
	ReportErrorInCallingSystemFunction *bool    `json:"report-error-in-calling-system-function,omitempty" yaml:"-"` // For CWE-686 only.
	Severity                           *string  `json:"severity" yaml:"-"`
	StructMemberLimit                  *int     `json:"struct-member-limit,omitempty" yaml:"-"`     //misra_c_2012/rule_1_1
	FunctionParmLimit                  *int     `json:"function-parm-limit,omitempty" yaml:"-"`     //misra_c_2012/rule_1_1
	FunctionArgLimit                   *int     `json:"function-arg-limit,omitempty" yaml:"-"`      //misra_c_2012/rule_1_1
	NestedRecordLimit                  *int     `json:"nested-record-limit,omitempty" yaml:"-"`     //misra_c_2012/rule_1_1
	NestedExprLimit                    *int     `json:"nested-expr-limit,omitempty" yaml:"-"`       //misra_c_2012/rule_1_1
	SwitchCaseLimit                    *int     `json:"switch-case-limit,omitempty" yaml:"-"`       //misra_c_2012/rule_1_1
	EnumConstantLimit                  *int     `json:"enum-constant-limit,omitempty" yaml:"-"`     //misra_c_2012/rule_1_1
	StringCharLimit                    *int     `json:"string-char-limit,omitempty" yaml:"-"`       //misra_c_2012/rule_1_1
	ExternIDLimit                      *int     `json:"extern-id-limit,omitempty" yaml:"-"`         //misra_c_2012/rule_1_1
	MacroIDLimit                       *int     `json:"macro-id-limit,omitempty" yaml:"-"`          //misra_c_2012/rule_1_1
	MacroParmLimit                     *int     `json:"macro-parm-limit,omitempty" yaml:"-"`        //misra_c_2012/rule_1_1
	MacroArgLimit                      *int     `json:"macro-arg-limit,omitempty" yaml:"-"`         //misra_c_2012/rule_1_1
	NestedBlockLimit                   *int     `json:"nested-block-limit,omitempty" yaml:"-"`      //misra_c_2012/rule_1_1
	NestedIncludeLimit                 *int     `json:"nested-include-limit,omitempty" yaml:"-"`    //misra_c_2012/rule_1_1
	IoMIDCharLimit                     *int     `json:"iom-id-char-limit,omitempty" yaml:"-"`       //misra_c_2012/rule_1_1
	NestedCondIncluLimit               *int     `json:"nested-cond-inclu-limit,omitempty" yaml:"-"` //misra_c_2012/rule_1_1
	BlockIDLimit                       *int     `json:"block-id-limit,omitempty" yaml:"-"`          //misra_c_2012/rule_1_1
}

type FilterAndSinkFunc struct {
	Function string `json:"function,omitempty" yaml:"Name"`
	Args     []int  `json:"args,omitempty" yaml:"Args,flow"`
	Scope    string `json:"scope,omitempty" yaml:"Scope,omitempty"`
}

type SourceFunc struct {
	Function string `json:"function,omitempty" yaml:"Name"`
	Args     []int  `json:"dstArgs,omitempty" yaml:"DstArgs,flow"`
	Scope    string `json:"scope,omitempty" yaml:"Scope,omitempty"`
}

var ValidStandard = map[string]bool{
	"c89": true,
	"c90": true,
	"c99": true,
	"c11": true,
}

func verifyStandard(standard string) bool {
	if _, ok := ValidStandard[standard]; !ok {
		return false
	}
	return true
}

func readStandard(input string) (string, error) {
	standard := strings.ToLower(input)
	if standard != emptyString {
		if !verifyStandard(standard) {
			return emptyString, fmt.Errorf("invalid standard: %s", standard)
		}
	} else {
		standard = "c99"
	}
	return standard, nil
}

func MakeCheckRule(name string, jsonOptions string) (*CheckRule, error) {
	checkRule := &CheckRule{}

	checkRule.Name = name
	err := json.Unmarshal([]byte(jsonOptions), &checkRule.JSONOptions)
	if err != nil {
		return nil, err
	}

	standard, err := readStandard(checkRule.JSONOptions.Standard)
	if err != nil {
		return nil, err
	}
	checkRule.JSONOptions.Standard = standard

	return checkRule, nil
}

func MakeCheckRuleWithoutError(name string, jsonOptions string) *CheckRule {
	checkRule, err := MakeCheckRule(name, jsonOptions)
	if err != nil {
		glog.Fatalf("can not make CheckRule without error: error: %v", err)
	}
	return checkRule
}

func (jsonOption JSONOption) GenerateTaintAnalyzerConf(reportDir string, yamlConfigPath string) (string, error) {
	if jsonOption.Filters == nil && jsonOption.Sources == nil && jsonOption.Sinks == nil {
		return emptyString, nil
	}

	yamlData, err := yaml.Marshal(&jsonOption)
	if err != nil {
		return emptyString, fmt.Errorf("parse struct to yaml: %v", err)
	}
	confPath := filepath.Join(reportDir, yamlConfigPath)
	err = os.WriteFile(confPath, yamlData, os.ModePerm)
	if err != nil {
		return emptyString, fmt.Errorf("write yaml data to file: %v", err)
	}
	return confPath, nil
}

func (jsonOption *JSONOption) Update(newOption JSONOption) {
	if newOption.CaseSensitive != nil {
		jsonOption.CaseSensitive = newOption.CaseSensitive
	}
	if newOption.Limit != nil {
		jsonOption.Limit = newOption.Limit
	}
	if newOption.Standard != "" {
		jsonOption.Standard = newOption.Standard
	}
	if newOption.Filters != nil {
		jsonOption.Filters = newOption.Filters
	}
	if newOption.MaxLoop != nil {
		jsonOption.MaxLoop = newOption.MaxLoop
	}
	if newOption.Sinks != nil {
		jsonOption.Sinks = newOption.Sinks
	}
	if newOption.Sources != nil {
		jsonOption.Sources = newOption.Sources
	}
	if newOption.CheckIncludeGuards != nil {
		jsonOption.CheckIncludeGuards = newOption.CheckIncludeGuards
	}
	if newOption.MaxReportNum != nil {
		jsonOption.MaxReportNum = newOption.MaxReportNum
	}
	if newOption.AggressiveMode != nil {
		jsonOption.AggressiveMode = newOption.AggressiveMode
	}
	if newOption.MaximumAllowedSize != nil {
		jsonOption.MaximumAllowedSize = newOption.MaximumAllowedSize
	}
	if newOption.CSAAnalyzerInliningMode != nil {
		jsonOption.CSAAnalyzerInliningMode = newOption.CSAAnalyzerInliningMode
	}
	if newOption.UseInfer {
		jsonOption.UseInfer = newOption.UseInfer
	}
	if newOption.UseCSA {
		jsonOption.UseCSA = newOption.UseCSA
	}
	if newOption.MaximumInlineFuncLine != nil {
		jsonOption.MaximumInlineFuncLine = newOption.MaximumInlineFuncLine
	}
	if newOption.ProjectName != nil {
		jsonOption.ProjectName = newOption.ProjectName
	}
	if newOption.MaximumAllowedReturnNum != nil {
		jsonOption.MaximumAllowedReturnNum = newOption.MaximumAllowedReturnNum
	}
	if newOption.MaximumAllowedFuncLine != nil {
		jsonOption.MaximumAllowedFuncLine = newOption.MaximumAllowedFuncLine
	}
	if newOption.RootDir != "" {
		jsonOption.RootDir = newOption.RootDir
	}
	if newOption.ReportErrorInCallingSystemFunction != nil {
		jsonOption.ReportErrorInCallingSystemFunction = newOption.ReportErrorInCallingSystemFunction
	}
}

func (jsonOption JSONOption) ToString() string {
	res, err := json.Marshal(jsonOption)
	if err != nil {
		glog.Errorf("failed to marshal json option: %v", jsonOption)
	}
	return string(res)
}

func UpdateOptions(checkRules []CheckRule, newOption JSONOption) *[]CheckRule {
	for idx := range checkRules {
		checkRules[idx].JSONOptions.Update(newOption)
	}
	return &checkRules
}

func ConstructStandardSet(checkRules []CheckRule) (map[string]bool, error) {
	standardSet := make(map[string]bool)
	for _, rule := range checkRules {
		standard := rule.JSONOptions.Standard
		_, ok := standardSet[standard]
		if !ok {
			standardSet[standard] = true
		}
	}
	return standardSet, nil
}
