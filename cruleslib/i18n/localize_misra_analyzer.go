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

package i18n

import (
	"strings"

	"golang.org/x/text/language"
	"golang.org/x/text/message"
	pb "naive.systems/analyzer/analyzer/proto"
)

var languageMap = map[string]language.Tag{"en": language.English, "zh": language.Chinese}

func localizeErrorMessage(result *pb.Result, p *message.Printer) string {
	switch result.ErrorKind {
	case pb.Result_MISRA_C_2012_RULE_1_1_STRUCT_MEMBER:
		return p.Sprintf("[C2201][misra-c2012-1.1]: violation of misra-c2012-1.1\n%s%s%s", result.Name, result.StructMemberCount, result.StructMemberLimit)
	case pb.Result_MISRA_C_2012_RULE_1_3:
		return p.Sprintf("[C2203][misra-c2012-1.3]: violation of misra-c2012-1.3\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_1_4:
		return p.Sprintf("[C2204][misra-c2012-1.4]: violation of misra-c2012-1.4")
	case pb.Result_MISRA_C_2012_RULE_2_1:
		return p.Sprintf("[C2007][misra-c2012-2.1]: violation of misra-c2012-2.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_2_2:
		return p.Sprintf("[C2006][misra-c2012-2.2]: A call to empty function is dead code")
	case pb.Result_MISRA_C_2012_RULE_2_2_EXTERNAL:
		return p.Sprintf("[C2006][misra-c2012-2.2]: violation of misra-c2012-2.2\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_2_3:
		return p.Sprintf("[C2005][misra-c2012-2.3]: violation of misra-c2012-2.3\nunused typedef name: %s", result.TypedefDeclName)
	case pb.Result_MISRA_C_2012_RULE_2_4:
		return p.Sprintf("[C2004][misra-c2012-2.4]: violation of misra-c2012-2.4")
	case pb.Result_MISRA_C_2012_RULE_2_5:
		return p.Sprintf("[C2003][misra-c2012-2.5]: violation of misra-c2012-2.5")
	case pb.Result_MISRA_C_2012_RULE_2_6:
		return p.Sprintf("[C2002][misra-c2012-2.6]: violation of misra-c2012-2.6")
	case pb.Result_MISRA_C_2012_RULE_2_7:
		return p.Sprintf("[C2001][misra-c2012-2.7]: violation of misra-c2012-2.7")
	case pb.Result_MISRA_C_2012_RULE_3_1:
		return p.Sprintf("[C2102][misra-c2012-3.1]: violation of misra-c2012-3.1")
	case pb.Result_MISRA_C_2012_RULE_3_2:
		return p.Sprintf("[C2101][misra-c2012-3.2]: violation of misra-c2012-3.2")
	case pb.Result_MISRA_C_2012_RULE_4_1:
		return p.Sprintf("[C1002][misra-c2012-4.1]: violation of misra-c2012-4.1")
	case pb.Result_MISRA_C_2012_RULE_4_2:
		return p.Sprintf("[C1001][misra-c2012-4.2]: violation of misra-c2012-4.2")
	case pb.Result_MISRA_C_2012_RULE_5_1_NON_ASCII_ERROR:
		return p.Sprintf("[C1109][misra-c2012-5.1]: violation of misra-contain non-ASCII characters\n%s: %s", result.Kind, result.Name)
	case pb.Result_MISRA_C_2012_RULE_5_1_DISTINCT_ERROR:
		return p.Sprintf("[C1109][misra-c2012-5.1]: violation of misra-c2012-5.1\n%s: %s\nFirst identifier location: %s\nDuplicated identifier '%s' location: %s", result.Kind, result.Name, result.Loc, result.ExternalMessage, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_5_2:
		return p.Sprintf("[C1108][misra-c2012-5.2]: violation of misra-c2012-5.2")
	case pb.Result_MISRA_C_2012_RULE_5_3:
		return p.Sprintf("[C1107][misra-c2012-5.3]: violation of misra-c2012-5.3")
	case pb.Result_MISRA_C_2012_RULE_5_4:
		return p.Sprintf("[C1106][misra-c2012-5.4]: violation of misra-c2012-5.4")
	case pb.Result_MISRA_C_2012_RULE_5_5:
		return p.Sprintf("[C1105][misra-c2012-5.5]: violation of misra-c2012-5.5")
	case pb.Result_MISRA_C_2012_RULE_5_6:
		return p.Sprintf("[C1104][misra-c2012-5.6]: violation of misra-c2012-5.6\nTypedef: %s\nFirst typedef location: %s\nDuplicated typedef location: %s", result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_5_7_DUPLICATE:
		return p.Sprintf("[C1103][misra-c2012-5.7]: error tag name is not unique\nDuplicated tag name: %s\nFirst identifier location: %s\nDuplicated identifier location: %s", result.TagName, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_5_7_INVALID:
		return p.Sprintf("[C1103][misra-c2012-5.7]: error tag name is not unique\nInvalid declaration: at %s", result.Loc)
	case pb.Result_MISRA_C_2012_RULE_5_8:
		return p.Sprintf("[C1102][misra-c2012-5.8]: %s with external linkage shall be unique\nName: %s\nLocation: %s\nOther location: %s", result.Kind, result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_5_9:
		return p.Sprintf("[C1101][misra-c2012-5.9]: %s with internal linkage shall be unique\nName: %s\nLocation: %s\nOther location: %s", result.Kind, result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_6_1:
		return p.Sprintf("[C0702][misra-c2012-6.1]: violation of misra-c2012-6.1")
	case pb.Result_MISRA_C_2012_RULE_6_2:
		return p.Sprintf("[C0701][misra-c2012-6.2]: violation of misra-c2012-6.2")
	case pb.Result_MISRA_C_2012_RULE_7_1:
		return p.Sprintf("[C0904][misra-c2012-7.1]: violation of misra-c2012-7.1")
	case pb.Result_MISRA_C_2012_RULE_7_2:
		return p.Sprintf("[C0903][misra-c2012-7.2]: violation of misra-c2012-7.2")
	case pb.Result_MISRA_C_2012_RULE_7_3:
		return p.Sprintf("[C0902][misra-c2012-7.3]: violation of misra-c2012-7.3")
	case pb.Result_MISRA_C_2012_RULE_7_4_CPPCHECK:
		return p.Sprintf("[C0901][misra-c2012-7.4]: violation of misra-c2012-7.4")
	case pb.Result_MISRA_C_2012_RULE_7_4:
		return p.Sprintf("[C0901][misra-c2012-7.4]: Assignment violation of misra-c2012-7.4\ntry to assign string literal to object with improper type\nLocation: %s", result.Loc)
	case pb.Result_MISRA_C_2012_RULE_7_4_EXTERNAL:
		return p.Sprintf("[C0901][misra-c2012-7.4]: violation of misra-c2012-7.4\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_8_1:
		return p.Sprintf("[C0514][misra-c2012-8.1]: violation of misra-c2012-8.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_8_2_FUNC_DECL_PARM_NOT_NAMED_ERROR:
		return p.Sprintf("[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\nunnamed parameter.\nfunction name: %s", result.Name)
	case pb.Result_MISRA_C_2012_RULE_8_2_FUNC_POINTER_PARM_NOT_NAMED_ERROR:
		return p.Sprintf("[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\nfunction pointer with unnamed parameter.")
	case pb.Result_MISRA_C_2012_RULE_8_2_FUNC_DECL_KR_STYLE_ERROR:
		return p.Sprintf("[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\nK&R style is fobidden.\nfunction name: %s", result.Name)
	case pb.Result_MISRA_C_2012_RULE_8_2_FUNC_DECL_VOID_ERROR:
		return p.Sprintf("[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\nMissing 'void'.\nfunction name: %s", result.Name)
	case pb.Result_MISRA_C_2012_RULE_8_3_INVALID_DECL_ERROR:
		return p.Sprintf("[C0512][misra-c2012-8.3]: violation of misra-c2012-8.3\nName: %s", result.Name)
	case pb.Result_MISRA_C_2012_RULE_8_3_ERROR:
		return p.Sprintf("[C0512][misra-c2012-8.3]: violation of misra-c2012-8.3\nName: %s\nLocation: %s\nOther location: %s", result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_8_4_NO_FUNC_DECL_ERROR:
		return p.Sprintf("[C0511][misra-c2012-8.4]: violation of misra-c2012-8.4\nMissing function declaration\nfunction name: %s\nlocation: %s", result.Name, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_8_4_NO_VAR_DECL_ERROR:
		return p.Sprintf("[C0511][misra-c2012-8.4]: violation of misra-c2012-8.4\nMissing variable declaration\nvariable name: %s\nlocation: %s", result.Name, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_8_4_FUNC_PARM_NOT_MATCH_ERROR:
		return p.Sprintf("[C0511][misra-c2012-8.4]: violation of misra-c2012-8.4\nFunction declaration and definition type do not match\nfunction name: %s\ndefinition location: %s", result.Name, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_8_5_EXT_VD_IN_C_ERROR:
		return p.Sprintf("[C0510][misra-c2012-8.5]: External object shall be declared in one header file.\n, Name: %s\n, Location: %s", result.Name, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_8_5_EXT_FD_IN_C_ERROR:
		return p.Sprintf("[C0510][misra-c2012-8.5]: External function shall be declared in one header file.\nName: %s\nLocation: %s", result.Name, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_8_5_EXT_VD_DUP_ERROR:
		return p.Sprintf("[C0510][misra-c2012-8.5]: External object shall be declared once in one and only one file.\nName: %s\nLocation: %s\nOther Location: %s", result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_8_5_EXT_FD_DUP_ERROR:
		return p.Sprintf("[C0510][misra-c2012-8.5]: External function shall be declared once in one and only one file.\nName: %s\nLocation: %s\nOther Location: %s", result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_8_6:
		return p.Sprintf("[C0509][misra-c2012-8.6]: error duplicated definition\nduplicated variable name: %s\nfirst definition location: %s\nduplicated definition location: %s", result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_8_7:
		return p.Sprintf("[C0508][misra-c2012-8.7]: violation of misra-c2012-8.7\nExtern function or variable is only called at one translation unit\nfunction name: %s\nused location: %s\ndeclared location: %s", result.Name, result.Loc, result.OtherLoc)
	case pb.Result_MISRA_C_2012_RULE_8_8:
		return p.Sprintf("[C0507][misra-c2012-8.8]: violation of misra-c2012-8.8")
	case pb.Result_MISRA_C_2012_RULE_8_9:
		return p.Sprintf("[C0506][misra-c2012-8.9]: violation of misra-c2012-8.9")
	case pb.Result_MISRA_C_2012_RULE_8_10:
		return p.Sprintf("[C0505][misra-c2012-8.10]: violation of misra-c2012-8.10")
	case pb.Result_MISRA_C_2012_RULE_8_11:
		return p.Sprintf("[C0504][misra-c2012-8.11]: violation of misra-c2012-8.11")
	case pb.Result_MISRA_C_2012_RULE_8_12:
		return p.Sprintf("[C0503][misra-c2012-8.12]: violation of misra-c2012-8.12")
	case pb.Result_MISRA_C_2012_RULE_8_14:
		return p.Sprintf("[C0501][misra-c2012-8.14]: violation of misra-c2012-8.14")
	case pb.Result_MISRA_C_2012_RULE_9_1:
		return p.Sprintf("[C1205][misra-c2012-9.1]: violation of misra-c2012-9.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_9_2:
		return p.Sprintf("[C1204][misra-c2012-9.2]: violation of misra-c2012-9.2")
	case pb.Result_MISRA_C_2012_RULE_9_3:
		return p.Sprintf("[C1203][misra-c2012-9.3]: violation of misra-c2012-9.3")
	case pb.Result_MISRA_C_2012_RULE_9_4:
		return p.Sprintf("[C1202][misra-c2012-9.4]: violation of misra-c2012-9.4")
	case pb.Result_MISRA_C_2012_RULE_9_5:
		return p.Sprintf("[C1201][misra-c2012-9.5]: violation of misra-c2012-9.5")
	case pb.Result_MISRA_C_2012_RULE_10_1:
		return p.Sprintf("[C0808][misra-c2012-10.1]: violation of misra-c2012-10.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_10_2:
		return p.Sprintf("[C0807][misra-c2012-10.2]: violation of misra-c2012-10.2")
	case pb.Result_MISRA_C_2012_RULE_10_3:
		return p.Sprintf("[C0806][misra-c2012-10.3]: violation of misra-c2012-10.3")
	case pb.Result_MISRA_C_2012_RULE_10_4:
		return p.Sprintf("[C0805][misra-c2012-10.4]: violation of misra-c2012-10.4")
	case pb.Result_MISRA_C_2012_RULE_10_5:
		return p.Sprintf("[C0804][misra-c2012-10.5]: violation of misra-c2012-10.5")
	case pb.Result_MISRA_C_2012_RULE_10_6:
		return p.Sprintf("[C0803][misra-c2012-10.6]: violation of misra-c2012-10.6")
	case pb.Result_MISRA_C_2012_RULE_10_7:
		return p.Sprintf("[C0802][misra-c2012-10.7]: violation of misra-c2012-10.7")
	case pb.Result_MISRA_C_2012_RULE_10_8:
		return p.Sprintf("[C0801][misra-c2012-10.8]: violation of misra-c2012-10.8")
	case pb.Result_MISRA_C_2012_RULE_11_1:
		return p.Sprintf("[C1409][misra-c2012-11.1]: violation of misra-conversions violation of misra-c2012-11.1\nobject name: %s\nsource type: %s\ndestination type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_1_EXTERNAL:
		return p.Sprintf("[C1409][misra-c2012-11.1]: violation of misra-conversions violation of misra-c2012-11.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_11_2:
		return p.Sprintf("[C1408][misra-c2012-11.2]: violation of misra-conversions violation of misra-c2012-11.2\nobject name: %s\nsource pointer object type: %s\ndestination pointer object type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_3:
		return p.Sprintf("[C1407][misra-c2012-11.3]: violation of misra-conversions violation of misra-c2012-11.3\nobject name: %s\nsource pointer object type: %s\ndestination pointer object type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_4_CPPCHECK:
		return p.Sprintf("[C1406][misra-c2012-11.4]: violation of misra-c2012-11.4")
	case pb.Result_MISRA_C_2012_RULE_11_4_INT_TO_POINTER:
		return p.Sprintf("[C1406][misra-c2012-11.4]: violation of misra-conversions violation of misra-c2012-11.4\nobject name: %s\nsource type: %s\ndestination pointer object type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_4:
		return p.Sprintf("[C1406][misra-c2012-11.4]: violation of misra-conversions violation of misra-c2012-11.4\nobject name: %s\nsource pointer object type: %s\ndestination type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_5_CPPCHECK:
		return p.Sprintf("[C1405][misra-c2012-11.5]: violation of misra-c2012-11.5")
	case pb.Result_MISRA_C_2012_RULE_11_5:
		return p.Sprintf("[C1405][misra-c2012-11.5]: violation of misra-conversions violation of misra-c2012-11.5\nobject name: %s\nsource pointer object type: %s\ndestination pointer object type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_6:
		return p.Sprintf("[C1404][misra-c2012-11.6]: violation of misra-conversions violation of misra-c2012-11.6\nobject name: %s\nsource object type: %s\ndestination object type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_6_EXTERNAL:
		return p.Sprintf("[C1404][misra-c2012-11.6]: violation of misra-conversions violation of misra-c2012-11.6\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_11_7:
		return p.Sprintf("[C1403][misra-c2012-11.7]: violation of misra-conversions violation of misra-c2012-11.7\nobject name: %s\nsource type: %s\ndestination type: %s\nLocation: %s", result.Name, result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_11_7_EXTERNAL:
		return p.Sprintf("[C1403][misra-c2012-11.7]: violation of misra-conversions violation of misra-c2012-11.7\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_11_8:
		return p.Sprintf("[C1402][misra-c2012-11.8]: violation of misra-conversions violation of misra-c2012-11.8\nobject name: %s\nsource pointer object type: %s\ndestination pointer object type: %s", result.Name, result.SourceType, result.DestinationType)
	case pb.Result_MISRA_C_2012_RULE_11_9:
		return p.Sprintf("[C1401][misra-c2012-11.9]: violation of misra-c2012-11.9")
	case pb.Result_MISRA_C_2012_RULE_12_1:
		return p.Sprintf("[C0605][misra-c2012-12.1]: violation of misra-c2012-12.1")
	case pb.Result_MISRA_C_2012_RULE_12_2:
		return p.Sprintf("[C0604][misra-c2012-12.2]: violation of misra-c2012-12.2\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_12_3:
		return p.Sprintf("[C0603][misra-c2012-12.3]: violation of misra-comma operator should not be used")
	case pb.Result_MISRA_C_2012_RULE_12_4:
		return p.Sprintf("[C0602][misra-c2012-12.4]: violation of misra-c2012-12.4")
	case pb.Result_MISRA_C_2012_RULE_12_5:
		return p.Sprintf("[C0601][misra-c2012-12.5]: violation of misra-c2012-12.5")
	case pb.Result_MISRA_C_2012_RULE_13_1:
		return p.Sprintf("[C1606][misra-c2012-13.1]: Init list has volatile referenced member")
	case pb.Result_MISRA_C_2012_RULE_13_2:
		return p.Sprintf("[C1605][misra-c2012-13.2]: multiple related functions should not be called in the same expression")
	case pb.Result_MISRA_C_2012_RULE_13_2_EXTERNAL:
		return p.Sprintf("[C1605][misra-c2012-13.2]: violation of misra-c2012-13.2\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_13_3:
		return p.Sprintf("[C1604][misra-c2012-13.3]: expression has more than one side effects")
	case pb.Result_MISRA_C_2012_RULE_13_4:
		return p.Sprintf("[C1603][misra-c2012-13.4]: Result of assignment operator should not be used")
	case pb.Result_MISRA_C_2012_RULE_13_4_EXTERNAL:
		return p.Sprintf("[C1603][misra-c2012-13.4]: violation of rule_13_4\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_13_5:
		return p.Sprintf("[C1602][misra-c2012-13.5]: Right hand operand may have persistent side effect, Location: %s", result.Loc)
	case pb.Result_MISRA_C_2012_RULE_13_5_IMPURE_FUNCTION:
		return p.Sprintf("[C1602][misra-c2012-13.5]: violation of misra-c2012-13.5\nFunction %s may have persistent side effect", result.Name)
	case pb.Result_MISRA_C_2012_RULE_13_6:
		return p.Sprintf("[C1601][misra-c2012-13.6]: violation of misra-c2012-13.6")
	case pb.Result_MISRA_C_2012_RULE_14_1:
		return p.Sprintf("[C1704][misra-c2012-14.1]: loop counter cannot be float")
	case pb.Result_MISRA_C_2012_RULE_14_1_CSA:
		return p.Sprintf("[C1704][misra-c2012-14.1]: violation of misra-c2012-14.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_14_2_MAY_HAVE_PSE:
		return p.Sprintf("[C1703][misra-c2012-14.2]: %s clause in for loop may have persistent side effect", result.WhichExpr)
	case pb.Result_MISRA_C_2012_RULE_14_2_COUNTER_NOT_USED:
		return p.Sprintf("[C1703][misra-c2012-14.2]: loop counter '%s' is not used in %s clause", result.CounterName, result.WhichExpr)
	case pb.Result_MISRA_C_2012_RULE_14_2_COUNTER_MODIFIED_IN_BODY:
		return p.Sprintf("[C1703][misra-c2012-14.2]: loop counter '%s' is modified in loop body", result.CounterName)
	case pb.Result_MISRA_C_2012_RULE_14_2_REF_MODIFIED_IN_BODY:
		return p.Sprintf("[C1703][misra-c2012-14.2]: %s clause uses reference '%s' modified in loop body", result.WhichExpr, result.RefName)
	case pb.Result_MISRA_C_2012_RULE_14_2_CLAUSE_SET_OTHER_VALUE:
		return p.Sprintf("[C1703][misra-c2012-14.2]: %s clause should set and only set the value of loop counter", result.WhichExpr)
	case pb.Result_MISRA_C_2012_RULE_14_2_2ND_CLAUSE_USE_NON_BOOL:
		return p.Sprintf("[C1703][misra-c2012-14.2]: second clause uses non-boolean control flag")
	case pb.Result_MISRA_C_2012_RULE_14_3:
		return p.Sprintf("[C1702][misra-c2012-14.3]: violation of misra-c2012-14.3")
	case pb.Result_MISRA_C_2012_RULE_14_3_EXTERNAL:
		return p.Sprintf("[C1702][misra-c2012-14.3]: violation of misra-c2012-14.3\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_14_4:
		return p.Sprintf("[C1701][misra-c2012-14.4]: violation of misra-c2012-14.4")
	case pb.Result_MISRA_C_2012_RULE_15_1:
		return p.Sprintf("[C1807][misra-c2012-15.1]: violation of misra-c2012-15.1")
	case pb.Result_MISRA_C_2012_RULE_15_2:
		return p.Sprintf("[C1806][misra-c2012-15.2]: violation of misra-c2012-15.2")
	case pb.Result_MISRA_C_2012_RULE_15_3:
		return p.Sprintf("[C1805][misra-c2012-15.3]: violation of misra-c2012-15.3")
	case pb.Result_MISRA_C_2012_RULE_15_4:
		return p.Sprintf("[C1804][misra-c2012-15.4]: violation of misra-c2012-15.4")
	case pb.Result_MISRA_C_2012_RULE_15_5:
		return p.Sprintf("[C1803][misra-c2012-15.5]: violation of misra-c2012-15.5")
	case pb.Result_MISRA_C_2012_RULE_15_6:
		return p.Sprintf("[C1802][misra-c2012-15.6]: violation of misra-c2012-15.6")
	case pb.Result_MISRA_C_2012_RULE_15_7:
		return p.Sprintf("[C1801][misra-c2012-15.7]: violation of misra-c2012-15.7")
	case pb.Result_MISRA_C_2012_RULE_16_1:
		return p.Sprintf("[C1907][misra-c2012-16.1]: violation of misra-c2012-16.1")
	case pb.Result_MISRA_C_2012_RULE_16_2:
		return p.Sprintf("[C1906][misra-c2012-16.2]: violation of misra-c2012-16.2")
	case pb.Result_MISRA_C_2012_RULE_16_3:
		return p.Sprintf("[C1905][misra-c2012-16.3]: violation of misra-c2012-16.3")
	case pb.Result_MISRA_C_2012_RULE_16_4:
		return p.Sprintf("[C1904][misra-c2012-16.4]: violation of misra-c2012-16.4")
	case pb.Result_MISRA_C_2012_RULE_16_5:
		return p.Sprintf("[C1903][misra-c2012-16.5]: violation of misra-c2012-16.5")
	case pb.Result_MISRA_C_2012_RULE_16_6:
		return p.Sprintf("[C1902][misra-c2012-16.6]: violation of misra-c2012-16.6")
	case pb.Result_MISRA_C_2012_RULE_16_7:
		return p.Sprintf("[C1901][misra-c2012-16.7]: violation of misra-c2012-16.7")
	case pb.Result_MISRA_C_2012_RULE_17_1:
		return p.Sprintf("[C1508][misra-c2012-17.1]: violation of misra-c2012-17.1")
	case pb.Result_MISRA_C_2012_RULE_17_2:
		return p.Sprintf("[C1507][misra-c2012-17.2]: violation of misra-c2012-17.2\nfound a recursive chain %s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_17_2_EXTERNAL:
		return p.Sprintf("[C1507][misra-c2012-17.2]: violation of misra-c2012-17.2\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_17_3:
		return p.Sprintf("[C1506][misra-c2012-17.3]: implicit declaration of function\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_17_4_CPPCHECK:
		return p.Sprintf("[C1505][misra-c2012-17.4]: violation of misra-c2012-17.4")
	case pb.Result_MISRA_C_2012_RULE_17_4:
		return p.Sprintf("[C1505][misra-c2012-17.4]: violation of rule_17_4\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_17_5_NULL_POINTER_ERROR:
		return p.Sprintf("[C1504][misra-c2012-17.5]: null pointer argument used for parameter with array type")
	case pb.Result_MISRA_C_2012_RULE_17_5_ARRAY_SIZE_ERROR:
		return p.Sprintf("[C1504][misra-c2012-17.5]: argument with improper array size used for parameter with array type")
	case pb.Result_MISRA_C_2012_RULE_17_5_EXTERNAL:
		return p.Sprintf("[C1504][misra-c2012-17.5]: violation of misra-c2012-17.5\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_17_6:
		return p.Sprintf("[C1503][misra-c2012-17.6]: violation of misra-c2012-17.6")
	case pb.Result_MISRA_C_2012_RULE_17_7:
		return p.Sprintf("[C1502][misra-c2012-17.7]: violation of misra-c2012-17.7")
	case pb.Result_MISRA_C_2012_RULE_17_8_CPPCHECK:
		return p.Sprintf("[C1501][misra-c2012-17.8]: violation of misra-c2012-17.8")
	case pb.Result_MISRA_C_2012_RULE_17_8:
		return p.Sprintf("[C1501][misra-c2012-17.8]: parameters should not be modified")
	case pb.Result_MISRA_C_2012_RULE_18_1:
		return p.Sprintf("[C1308][misra-c2012-18.1]: Pointer arithmetic may cause array out of bound.\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_18_2:
		return p.Sprintf("[C1307][misra-c2012-18.2]: violation of misra-c2012-18.2\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_18_3:
		return p.Sprintf("[C1306][misra-c2012-18.3]: violation of misra-c2012-18.3\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_18_4:
		return p.Sprintf("[C1305][misra-c2012-18.4]: violation of misra-c2012-18.4")
	case pb.Result_MISRA_C_2012_RULE_18_5:
		return p.Sprintf("[C1304][misra-c2012-18.5]: violation of misra-c2012-18.5")
	case pb.Result_MISRA_C_2012_RULE_18_6:
		return p.Sprintf("[C1303][misra-c2012-18.6]: violation of misra-c2012-18.6\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_18_7:
		return p.Sprintf("[C1302][misra-c2012-18.7]: violation of misra-c2012-18.7")
	case pb.Result_MISRA_C_2012_RULE_18_8:
		return p.Sprintf("[C1301][misra-c2012-18.8]: violation of misra-c2012-18.8")
	case pb.Result_MISRA_C_2012_RULE_19_1:
		return p.Sprintf("[C0302][misra-c2012-19.1]: violation of misra-c2012-19.1, assign or copy is overlapped\n")
	case pb.Result_MISRA_C_2012_RULE_19_1_EXTERNAL:
		return p.Sprintf("[C0302][misra-c2012-19.1]: violation of misra-c2012-19.1, assign or copy is overlapped\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_19_2:
		return p.Sprintf("[C0301][misra-c2012-19.2]: violation of misra-c2012-19.2")
	case pb.Result_MISRA_C_2012_RULE_20_1:
		return p.Sprintf("[C0114][misra-c2012-20.1]: violation of misra-c2012-20.1")
	case pb.Result_MISRA_C_2012_RULE_20_2:
		return p.Sprintf("[C0113][misra-c2012-20.2]: violation of misra-c2012-20.2")
	case pb.Result_MISRA_C_2012_RULE_20_3:
		return p.Sprintf("[C0112][misra-c2012-20.3]: violation of misra-c2012-20.3")
	case pb.Result_MISRA_C_2012_RULE_20_4:
		return p.Sprintf("[C0111][misra-c2012-20.4]: violation of misra-c2012-20.4")
	case pb.Result_MISRA_C_2012_RULE_20_5:
		return p.Sprintf("[C0110][misra-c2012-20.5]: violation of misra-c2012-20.5")
	case pb.Result_MISRA_C_2012_RULE_20_6:
		return p.Sprintf("[C0109][misra-c2012-20.6]: violation of misra-c2012-20.6")
	case pb.Result_MISRA_C_2012_RULE_20_7:
		return p.Sprintf("[C0108][misra-c2012-20.7]: violation of misra-c2012-20.7")
	case pb.Result_MISRA_C_2012_RULE_20_8:
		return p.Sprintf("[C0107][misra-c2012-20.8]: violation of misra-c2012-20.8")
	case pb.Result_MISRA_C_2012_RULE_20_9:
		return p.Sprintf("[C0106][misra-c2012-20.9]: violation of misra-c2012-20.9")
	case pb.Result_MISRA_C_2012_RULE_20_10:
		return p.Sprintf("[C0105][misra-c2012-20.10]: violation of misra-c2012-20.10")
	case pb.Result_MISRA_C_2012_RULE_20_11:
		return p.Sprintf("[C0104][misra-c2012-20.11]: violation of misra-c2012-20.11")
	case pb.Result_MISRA_C_2012_RULE_20_12:
		return p.Sprintf("[C0103][misra-c2012-20.12]: violation of misra-c2012-20.12")
	case pb.Result_MISRA_C_2012_RULE_20_13:
		return p.Sprintf("[C0102][misra-c2012-20.13]: violation of misra-c2012-20.13")
	case pb.Result_MISRA_C_2012_RULE_20_14:
		return p.Sprintf("[C0101][misra-c2012-20.14]: violation of misra-c2012-20.14")
	case pb.Result_MISRA_C_2012_RULE_21_1:
		return p.Sprintf("[C0420][misra-c2012-21.1]: violation of misra-c2012-21.1")
	case pb.Result_MISRA_C_2012_RULE_21_2:
		return p.Sprintf("[C0419][misra-c2012-21.2]: violation of misra-c2012-21.2")
	case pb.Result_MISRA_C_2012_RULE_21_3:
		return p.Sprintf("[C0418][misra-c2012-21.3]: violation of misra-c2012-21.3")
	case pb.Result_MISRA_C_2012_RULE_21_4:
		return p.Sprintf("[C0417][misra-c2012-21.4]: violation of misra-c2012-21.4")
	case pb.Result_MISRA_C_2012_RULE_21_5:
		return p.Sprintf("[C0416][misra-c2012-21.5]: violation of misra-c2012-21.5")
	case pb.Result_MISRA_C_2012_RULE_21_6:
		return p.Sprintf("[C0415][misra-c2012-21.6]: violation of misra-c2012-21.6")
	case pb.Result_MISRA_C_2012_RULE_21_7:
		return p.Sprintf("[C0414][misra-c2012-21.7]: violation of misra-c2012-21.7")
	case pb.Result_MISRA_C_2012_RULE_21_8:
		return p.Sprintf("[C0413][misra-c2012-21.8]: violation of misra-c2012-21.8")
	case pb.Result_MISRA_C_2012_RULE_21_9:
		return p.Sprintf("[C0412][misra-c2012-21.9]: violation of misra-c2012-21.9")
	case pb.Result_MISRA_C_2012_RULE_21_10:
		return p.Sprintf("[C0411][misra-c2012-21.10]: violation of misra-c2012-21.10")
	case pb.Result_MISRA_C_2012_RULE_21_11:
		return p.Sprintf("[C0410][misra-c2012-21.11]: violation of misra-c2012-21.11")
	case pb.Result_MISRA_C_2012_RULE_21_12:
		return p.Sprintf("[C0409][misra-c2012-21.12]: violation of misra-c2012-21.12")
	case pb.Result_MISRA_C_2012_RULE_21_13:
		return p.Sprintf("[C0408][misra-c2012-21.13]: violation of misra-c2012-21.13\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_21_14:
		return p.Sprintf("[C0407][misra-c2012-21.14]: violation of misra-c2012-21.14\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_21_15:
		return p.Sprintf("[C0406][misra-c2012-21.15]: violation of misra-c2012-21.15")
	case pb.Result_MISRA_C_2012_RULE_21_16:
		return p.Sprintf("[C0405][misra-c2012-21.16]: violation of misra-c2012-21.16")
	case pb.Result_MISRA_C_2012_RULE_21_17:
		return p.Sprintf("[C0404][misra-c2012-21.17]: The cstring function call may cause read or write out of bound.\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_21_18:
		return p.Sprintf("[C0403][misra-c2012-21.18]: size_t value invalid as function argument.\nsource pointer object type: %s\ndestination object type: %s\nLocation: %s", result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_21_18_EXTERNAL:
		return p.Sprintf("[C0403][misra-c2012-21.18]: violation of misra-c2012-21.18\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_21_19:
		return p.Sprintf("[C0402][misra-c2012-21.19]: the return value of function is assigned to non-const qualified type\nsource pointer object type: %s\ndestination object type: %s\nLocation: %s", result.SourceType, result.DestinationType, result.Loc)
	case pb.Result_MISRA_C_2012_RULE_21_19_EXTERNAL:
		return p.Sprintf("[C0402][misra-c2012-21.19]: violation of misra-c2012-21.19\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_21_20:
		return p.Sprintf("[C0401][misra-c2012-21.20]: violation of misra-c2012-21.20\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_21_21:
		return p.Sprintf("[C0421][misra-c2012-21.21]: violation of misra-c2012-21.21")
	case pb.Result_MISRA_C_2012_RULE_22_1:
		return p.Sprintf("[C0210][misra-c2012-22.1]: violation of misra-c2012-22.1\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_2:
		return p.Sprintf("[C0209][misra-c2012-22.2]: violation of misra-c2012-22.2\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_3:
		return p.Sprintf("[C0208][misra-c2012-22.3]: violation of misra-c2012-22.3\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_4:
		return p.Sprintf("[C0207][misra-c2012-22.4]: violation of misra-c2012-22.4\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_5:
		return p.Sprintf("[C0206][misra-c2012-22.5]: violation of misra-c2012-22.5\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_6:
		return p.Sprintf("[C0205][misra-c2012-22.6]: violation of misra-c2012-22.6\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_7:
		return p.Sprintf("[C0204][misra-c2012-22.7]: violation of misra-c2012-22.7\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_8:
		return p.Sprintf("[C0203][misra-c2012-22.8]: violation of misra-c2012-22.8\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_9:
		return p.Sprintf("[C0202][misra-c2012-22.9]: violation of misra-c2012-22.9\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_RULE_22_10:
		return p.Sprintf("[C0201][misra-c2012-22.10]: violation of misra-c2012-22.10\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_DIR_4_3_ASM_SHOULD_BE_ISOLATED:
		return p.Sprintf("[C2306][misra-c2012-dir-4.3]: assembly code should be isolated")
	case pb.Result_MISRA_C_2012_DIR_4_3_ASM_SHOULD_BE_ENCAPSULATED:
		return p.Sprintf("[C2306][misra-c2012-dir-4.3]: assembly code should be encapsulated")
	case pb.Result_MISRA_C_2012_DIR_4_7_ERROR_SHOULD_BE_TESTED:
		return p.Sprintf("[C2310][misra-c2012-dir-4.7]: error should be tested")
	case pb.Result_MISRA_C_2012_DIR_4_10_HAS_NO_PRECAUTION:
		return p.Sprintf("[C2313][misra-c2012-dir-4.10]: %s has no precaution", result.Filename)
	case pb.Result_MISRA_C_2012_DIR_4_10_HAS_SAME_FILE_ID:
		return p.Sprintf("[C2313][misra-c2012-dir-4.10]: %s and %s has same file identifier", result.Filename, result.OtherFilename)
	case pb.Result_MISRA_C_2012_DIR_4_11:
		return p.Sprintf("[C2314][misra-c2012-dir-4.11]: violation of misra-c2012-dir-4.11\n%s", result.ExternalMessage)
	case pb.Result_MISRA_C_2012_DIR_4_12:
		return p.Sprintf("[C2315][misra-c2012-dir-4.12]: dynamic allocation should not be used")
	case pb.Result_MISRA_C_2012_DIR_4_14:
		return p.Sprintf("[C2317][misra-c2012-dir-4.14]: violation of misra-c2012-dir-4.14\n%s", result.ExternalMessage)
	case pb.Result_NAIVESYSTEMS_C7966:
		return p.Sprintf("[C7966][NAIVESYSTEMS_C7966]: violation of NAIVESYSTEMS_C7966\n%s", result.ExternalMessage)
	case pb.Result_MISRA_CPP_2008_RULE_0_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-1: A project shall not contain unreachable code.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-2: A project shall not contain infeasible paths.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-3: A project shall not contain unused variables.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-4: A project shall not contain non-volatile POD variables having only one use.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-5: A project shall not contain unused type declarations.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-6: A project shall not contain instances of non-volatile variables being given values that are never subsequently used.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_7:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-7: The value returned by a function having a non-void return type that is not an overloaded operator shall always be used.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_8:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-8: All functions with void return type shall have external side effect(s).")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_9:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-9: There shall be no dead code.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_10:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-10: Every defined function shall be called at least once.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_11:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-11: There shall be no unused parameters (named or unnamed) in non-virtual functions.")
	case pb.Result_MISRA_CPP_2008_RULE_0_1_12:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-1-12: There shall be no unused parameters (named or unnamed) in the set of parameters for a virtual function and all the functions that override it.")
	case pb.Result_MISRA_CPP_2008_RULE_0_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-2-1: An object shall not be assigned to an overlapping object.")
	case pb.Result_MISRA_CPP_2008_RULE_0_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-3-1: Minimization of run-time failures shall be ensured by the use of at least one of: (a) static analysis tools/techniques; (b) dynamic tools/techniques; (c) explicit coding of checks to handle run-time faults.")
	case pb.Result_MISRA_CPP_2008_RULE_0_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-3-2: If a function generates error information, then that error information shall be tested.")
	case pb.Result_MISRA_CPP_2008_RULE_0_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-4-1: Use of scaled-integer or fixed-point arithmetic shall be documented.")
	case pb.Result_MISRA_CPP_2008_RULE_0_4_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-4-2: Use of floating-point arithmetic shall be documented.")
	case pb.Result_MISRA_CPP_2008_RULE_0_4_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 0-4-3: Floating-point implementations shall comply with a defined floating-point standard.")
	case pb.Result_MISRA_CPP_2008_RULE_1_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 1-0-1: All code shall conform to ISO/IEC 14882:2003 \"The C++ Standard Incorporating Technical Corrigendum 1\".")
	case pb.Result_MISRA_CPP_2008_RULE_1_0_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 1-0-2: Multiple compilers shall only be used if they have a common, defined interface.")
	case pb.Result_MISRA_CPP_2008_RULE_1_0_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 1-0-3: The implementation of integer division in the chosen compiler shall be determined and documented.")
	case pb.Result_MISRA_CPP_2008_RULE_2_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-2-1: The character set and the corresponding encoding shall be documented.")
	case pb.Result_MISRA_CPP_2008_RULE_2_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-3-1: Trigraphs shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_2_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-5-1: Digraphs should not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_2_7_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-7-1: The character sequence /* shall not be used within a C-style comment.")
	case pb.Result_MISRA_CPP_2008_RULE_2_7_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-7-2: Sections of code shall not be \"commented out\" using C-style comments.")
	case pb.Result_MISRA_CPP_2008_RULE_2_7_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-7-3: Sections of code should not be \"commented out\" using C++ comments.")
	case pb.Result_MISRA_CPP_2008_RULE_2_10_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-10-1: Different identifiers shall be typographically unambiguous.")
	case pb.Result_MISRA_CPP_2008_RULE_2_10_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-10-2: Identifiers declared in an inner scope shall not hide an identifier declared in an outer scope.")
	case pb.Result_MISRA_CPP_2008_RULE_2_10_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-10-3: A typedef name (including qualification, if any) shall be a unique identifier.")
	case pb.Result_MISRA_CPP_2008_RULE_2_10_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-10-4: A class, union or enum name (including qualification, if any) shall be a unique identifier.")
	case pb.Result_MISRA_CPP_2008_RULE_2_10_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-10-5: The identifier name of a non-member object or function with static storage duration should not be reused.")
	case pb.Result_MISRA_CPP_2008_RULE_2_10_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-10-6: If an identifier refers to a type, it shall not also refer to an object or a function in the same scope.")
	case pb.Result_MISRA_CPP_2008_RULE_2_13_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-13-1: Only those escape sequences that are defined in ISO/IEC 14882:2003 shall be used.")
	case pb.Result_MISRA_CPP_2008_RULE_2_13_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-13-2: Octal constants (other than zero) and octal escape sequences (other than \"\\0\") shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_2_13_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-13-3: A \"U\" suffix shall be applied to all octal or hexadecimal integer literals of unsigned type.")
	case pb.Result_MISRA_CPP_2008_RULE_2_13_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-13-4: Literal suffixes shall be upper case.")
	case pb.Result_MISRA_CPP_2008_RULE_2_13_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 2-13-5: Narrow and wide string literals shall not be concatenated.")
	case pb.Result_MISRA_CPP_2008_RULE_3_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-1-1: It shall be possible to include any header file in multiple translation units without violaing the One Definition Rule.")
	case pb.Result_MISRA_CPP_2008_RULE_3_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-1-2: Functions shall not be declared at block scope.")
	case pb.Result_MISRA_CPP_2008_RULE_3_1_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-1-3: When an array is declared, its size shall either be stated explicitly or defined implicitly by initialization.")
	case pb.Result_MISRA_CPP_2008_RULE_3_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-2-1: All declarations of an object or function shall have compatible types.")
	case pb.Result_MISRA_CPP_2008_RULE_3_2_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-2-2: The One Definition Rule shall not be violated.")
	case pb.Result_MISRA_CPP_2008_RULE_3_2_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-2-3: A type, object or function that is used in multiple translation units shall be declared in one and only one file.")
	case pb.Result_MISRA_CPP_2008_RULE_3_2_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-2-4: An identifier with external linkage shall have exactly one definition.")
	case pb.Result_MISRA_CPP_2008_RULE_3_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-3-1: Objects or functions with external linkage shall be declared in a header file.")
	case pb.Result_MISRA_CPP_2008_RULE_3_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-3-2: If a function has internal linkage then all re-declarations shall include the static storage class specifier.")
	case pb.Result_MISRA_CPP_2008_RULE_3_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-4-1: An identfier declared to be an object or type shall be defined in a block that minimizes its visibility.")
	case pb.Result_MISRA_CPP_2008_RULE_3_9_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-9-1: The types used for an object, a function return type, or a function parameter shalll be token-for-token identical in all declarations and re-declarations.")
	case pb.Result_MISRA_CPP_2008_RULE_3_9_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-9-2: typedefs that indicate size and signedness should be used in place of the basic numerical types.")
	case pb.Result_MISRA_CPP_2008_RULE_3_9_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 3-9-3: The underlying bit representations of floating-point values shall not be used .")
	case pb.Result_MISRA_CPP_2008_RULE_4_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 4-5-1: Expressions with type bool shall not be used as operands to built-in operators other than the assignment operator =, the logical operators &&, ||, !, the equality operators == and !=, the unary & operator, and the conditional operator.")
	case pb.Result_MISRA_CPP_2008_RULE_4_5_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 4-5-2: Expressions with type enum shall not be used as operands to built-in operators other than the subscript operator [ ], the assignment operator =, the equality operators == and !=, the unary & operator, and the relational operators <, <=, >, >=.")
	case pb.Result_MISRA_CPP_2008_RULE_4_5_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 4-5-3: Expressions with type (plain) char and wchar_t shall not be used as operands to built-in operators other than the assignment operator =, the equality operators == and !=, and the unary & operator.")
	case pb.Result_MISRA_CPP_2008_RULE_4_10_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 4-10-1: NULL shall not be used as an integer value.")
	case pb.Result_MISRA_CPP_2008_RULE_4_10_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 4-10-2: Literal zero (0) shall not be used as the null-pointer-constant")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-1: The value of an expression shall be the same under any order of evaluation that the standard permits.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-2: Limited dependence should be placed on C++ operator precedence rules in expressions.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-3: A cvalue expression shall not be implicitly converted to a different underlying type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-4: An implicit integral conversion shall not change the signedness of the underlying type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-5: There shall be no implicit floating-integral conversions.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-6: An implicit integral or floating-point conversion shall not reduce the size of the underlying type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_7:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-7: There shall be no explicit floating-integral conversions of a cvalue expression.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_8:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-8: An explicit integral or floating-point conversion shall not increase the size of the underlying type of a cvalue expression.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_9:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-9: An explicit integral conversion shall not change the signedness of the underlying type of a cvalue expression.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_10:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-10: If the bitwise operators ~ and << are applied to an operand with an underlying type of unsigned char or unsigned short, the result shall be immediately cast to the underlying type of the operand.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_11:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-11: The plain char type shall only be used for the storage and use of character values.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_12:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-12: signed char and unsigned char type shall only be used for the storage and use of numeric values.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_13:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-13: The condition of an if-statement and the condition of an iteration-statement shall have type bool.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_14:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-14: The first operand of a conditional-operator shall have type bool.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_15:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-15: Array indexing shall be the only form of pointer arithmetic.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_16:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-16: A pointer operand and any pointer resulting from pointer arithmetic using that operand shall both address elements of the same array.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_17:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-17: Subtraction between pointers shall only be applied to pointers that address elements of the same array.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_18:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-18: >, >=, <, <= shall not be applied to objects of pointer type, except where they point to the same array.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_19:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-19: The declaration of objects shall contain no more than two levels of pointer indirection.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_20:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-20: Non-constant operands to a binary bitwise operator shall have the same underlying type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_0_21:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-0-21: Bitwise operators shall only be applied to operands of unsigned underlying type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-1: Each operand of a logical && or | | shall be a postfix-expression.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-2: A pointer to a virtual base class shall only be cast to a pointer to a derived class by means of dynamic_cast.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-3: Casts from a base class to a derived class should not be performed on polymorphic types.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-4: C-style casts (other than void casts) and functional notation casts (other than explicit constructor call) shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-5: A cast shall not remove any const or volatile qualification from the type of a pointer or reference.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-6: A cast shall not convert a pointer to a function to any other pointer type, including a pointer to function type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_7:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-7: An object with pointer type shall not be converted to an unrelated pointer type, either directly or indirectly.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_8:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-8: An object with integer type or pointer to void type shall not be converted to an object with poiner type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_9:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-9: A cast should not convert a pointer type to an integral type.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_10:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-10: The increment (++) and decrement (--) operators should not be mixed with other operators in an exression.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_11:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-11: The comma operator, && operator and the || operator shall not be overloaded.")
	case pb.Result_MISRA_CPP_2008_RULE_5_2_12:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-2-12: An identifier with array type passed as a function argument shall not decay to a pointer.")
	case pb.Result_MISRA_CPP_2008_RULE_5_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-3-1: Each operand of the ! operator, the logical && or the logical operators shall have type bool.")
	case pb.Result_MISRA_CPP_2008_RULE_5_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-3-2: The unary minus operator shall not be applied to an expression whose underlying type is unsigned.")
	case pb.Result_MISRA_CPP_2008_RULE_5_3_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-3-3: The unary & operator shall not be overloaded.")
	case pb.Result_MISRA_CPP_2008_RULE_5_3_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-3-4: Evaluation of the operand to the sizeof operator shall not contain side effects.")
	case pb.Result_MISRA_CPP_2008_RULE_5_8_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-8-1: The right hand operand of a shift operator shall lie between zero and one less than the width in bits of the underlying type of the left hand operand.")
	case pb.Result_MISRA_CPP_2008_RULE_5_14_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-14-1: The right hand operand of a logical && or || operator shall not contain side effects.")
	case pb.Result_MISRA_CPP_2008_RULE_5_17_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-17-1: The semantic equivalence between a binary operator and its asignment operator form shall be preserved.")
	case pb.Result_MISRA_CPP_2008_RULE_5_18_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-18-1: The comma operator shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_5_19_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 5-19-1: Evaluation of constant unsigned integer expressions should not lead to wrap-around.")
	case pb.Result_MISRA_CPP_2008_RULE_6_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-2-1: Assignment operators shall not be used in sub-expressions.")
	case pb.Result_MISRA_CPP_2008_RULE_6_2_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-2-2: Floating-point expressions shall not be directly or indirectly tested for equality or inequality.")
	case pb.Result_MISRA_CPP_2008_RULE_6_2_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-2-3: Before preprocessing, a null statement shall only occur on a line by itself; it may be followed by a comment, provided that the first character following the null statement is a white-space character.")
	case pb.Result_MISRA_CPP_2008_RULE_6_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-3-1: The statement forming the body of a switch, while, do ... while or for statement shall be a compound statement.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-1: An if (condition) construct shall be followed by a compound statement. The else keyword shall be followed by either a compound statement, or another if statement.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-2: All if ... else if constructs shall be terminated with an else clause.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-3: A switch statement shall be a well-formed switch statement.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-4: A switch-label shall only be used when the most closely-enclosing compound statement is the body of a switch statement.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-5: An unconditional throw or break statement shall terminate every non-empty switch-clause.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-6: The final clause of a switch statement shall be the default-clause.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_7:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-7: The condition of a switch statement shall not have bool type.")
	case pb.Result_MISRA_CPP_2008_RULE_6_4_8:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-4-8: Every switch statement shall have at least one case-clause,")
	case pb.Result_MISRA_CPP_2008_RULE_6_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-5-1: A for loop shall contain a single loop-counter which shall not have floating type.")
	case pb.Result_MISRA_CPP_2008_RULE_6_5_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-5-2: If loop-counter is not modified by -- or ++, then, within condition, the loop-counter shall only be used as an operand to <=, <, > or >=.")
	case pb.Result_MISRA_CPP_2008_RULE_6_5_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-5-3: The loop-counter shall not be modified within statement.")
	case pb.Result_MISRA_CPP_2008_RULE_6_5_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-5-4: The loop-counter shall be modified by one of: --, ++,-=n, or +=n; where n remains constant for the duration of the loop.")
	case pb.Result_MISRA_CPP_2008_RULE_6_5_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-5-5: A loop-control-variable other than the loop-counter shall not be modified within condition or expression.")
	case pb.Result_MISRA_CPP_2008_RULE_6_5_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-5-6: A loop-control-variable other than the loop-counter which is modified in statement shall have type bool.")
	case pb.Result_MISRA_CPP_2008_RULE_6_6_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-6-1: Any label referenced by a goto statement shall be declared in the same block, or in a block enclosing the goto statement.")
	case pb.Result_MISRA_CPP_2008_RULE_6_6_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-6-2: The goto statement shall jump to a label declared later in the same function body.")
	case pb.Result_MISRA_CPP_2008_RULE_6_6_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-6-3: The continue statement shall only be used within a well-formed for loop.")
	case pb.Result_MISRA_CPP_2008_RULE_6_6_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-6-4: For any iteration statement there shall be no more than one break or goto statement used for loop termination.")
	case pb.Result_MISRA_CPP_2008_RULE_6_6_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 6-6-5: A function shall have a single point of exit at the end of the function.")
	case pb.Result_MISRA_CPP_2008_RULE_7_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-1-1: A variable which is not modified shall be const qualified.")
	case pb.Result_MISRA_CPP_2008_RULE_7_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-1-2: A pointer or reference parameter in a function shall be declared as pointer to const or reference to const if the corresponding object is not modified.")
	case pb.Result_MISRA_CPP_2008_RULE_7_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-2-1: An expression with enum underlying type shall only have values corresponding to the enumerators of the enumeration.")
	case pb.Result_MISRA_CPP_2008_RULE_7_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-3-1: The global namespace shall only contain main, namespace declarations and extern \"C\" declarations.")
	case pb.Result_MISRA_CPP_2008_RULE_7_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-3-2: The identifier main shall not be used for a function other than the global function main.")
	case pb.Result_MISRA_CPP_2008_RULE_7_3_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-3-3: There shall be no unnamed namespaces in header files.")
	case pb.Result_MISRA_CPP_2008_RULE_7_3_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-3-4: using-directives shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_7_3_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-3-5: Multiple declarations for an identifier in the same namespace shall not straddle a using-declaration for that idenifier.")
	case pb.Result_MISRA_CPP_2008_RULE_7_3_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-3-6: using-directives and using-declarations (excluding class scope or function scope using-declarations) shall not be used in header files.")
	case pb.Result_MISRA_CPP_2008_RULE_7_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-4-1: All usage of assembler shall be documented.")
	case pb.Result_MISRA_CPP_2008_RULE_7_4_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-4-2: Assembler instructions shall only be introduced using the asm declaration.")
	case pb.Result_MISRA_CPP_2008_RULE_7_4_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-4-3: Assembly language shall be encapsulated and isolated.")
	case pb.Result_MISRA_CPP_2008_RULE_7_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-5-1: A function shall not return a reference or a pointer to an automatic variable (including parameters) defined within the function.")
	case pb.Result_MISRA_CPP_2008_RULE_7_5_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-5-2: The address of an object with automatic storage shall not be assigned to another object that may persist after the first object has ceased to exist.")
	case pb.Result_MISRA_CPP_2008_RULE_7_5_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-5-3: A function shall not return a reference or a pointer to a parameter that is passed by reference or const reference.")
	case pb.Result_MISRA_CPP_2008_RULE_7_5_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 7-5-4: Functions should not call themselves, either directly or indirectly.")
	case pb.Result_MISRA_CPP_2008_RULE_8_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-0-1: An init-declarator-list or a member-declarator-list shall consist of a single init-declarator or member-declarator respectively.")
	case pb.Result_MISRA_CPP_2008_RULE_8_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-3-1: Parameters in an overriding virtual function shall either use the same default arguments as the function they ovrride, or else shall not specify any default arguments.")
	case pb.Result_MISRA_CPP_2008_RULE_8_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-4-1: Functions shall not be defined using the ellipsis notation.")
	case pb.Result_MISRA_CPP_2008_RULE_8_4_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-4-2: The identifiers used for the parameters in a re-declaration of a function shall be identical to those in the declaration.")
	case pb.Result_MISRA_CPP_2008_RULE_8_4_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-4-3: All exit paths from a function with non-void return type shall have an explicit return statement with an expression.")
	case pb.Result_MISRA_CPP_2008_RULE_8_4_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-4-4: A function identifier shall either be used to call the function or it shall be preceded by &.")
	case pb.Result_MISRA_CPP_2008_RULE_8_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-5-1: All variables shall have a defined value before they are used.")
	case pb.Result_MISRA_CPP_2008_RULE_8_5_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-5-2: Braces shall be used to indicate and match the structure in the non-zero initialization of arrays and structures.")
	case pb.Result_MISRA_CPP_2008_RULE_8_5_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 8-5-3: In an enumerator list, the = construct shall not be used to explicitly initialize members other than the first, unless all items are explicitly initialized.")
	case pb.Result_MISRA_CPP_2008_RULE_9_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-3-1: const member functions shall not return non-const pointers or references to class-data.")
	case pb.Result_MISRA_CPP_2008_RULE_9_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-3-2: Member functions shall not return non-const handles to class-data.")
	case pb.Result_MISRA_CPP_2008_RULE_9_3_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-3-3: If a member function can be made static then it shall be made static, otherwise if it can be made const then it shall be made const.")
	case pb.Result_MISRA_CPP_2008_RULE_9_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-5-1: Unions shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_9_6_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-6-1: When the absolute positioning of bits representing a bit-field is required, then the behaviour and packing of bit-fields shall be documented.")
	case pb.Result_MISRA_CPP_2008_RULE_9_6_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-6-2: Bit-fields shall be either bool type or an explicitly unsigned or signed integral type.")
	case pb.Result_MISRA_CPP_2008_RULE_9_6_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-6-3: Bit-fields shall not have enum type.")
	case pb.Result_MISRA_CPP_2008_RULE_9_6_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 9-6-4: Named bit-fields with signed integer type shall have a length of more than one bit.")
	case pb.Result_MISRA_CPP_2008_RULE_10_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-1-1: Classes should not be derived from virtual bases.")
	case pb.Result_MISRA_CPP_2008_RULE_10_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-1-2: A base class shall only be declared virtual if it is used in a diamond hierarchy.")
	case pb.Result_MISRA_CPP_2008_RULE_10_1_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-1-3: An accessible base class shall not be both virtual and non-virtual in the same hierarchy.")
	case pb.Result_MISRA_CPP_2008_RULE_10_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-2-1: All accessible entity names within a multiple inheritance hierarchy should be unique.")
	case pb.Result_MISRA_CPP_2008_RULE_10_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-3-1: There shall be no more than one definition of each virtual function on each path through the inheritance hierarchy.")
	case pb.Result_MISRA_CPP_2008_RULE_10_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-3-2: Each overriding virtual function shall be declared with the virtual keyword.")
	case pb.Result_MISRA_CPP_2008_RULE_10_3_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 10-3-3: A virtual function shall only be overridden by a pure virtual function if it is itself declared as pure virtual.")
	case pb.Result_MISRA_CPP_2008_RULE_11_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 11-0-1: Member data in non-POD class types shall be private.")
	case pb.Result_MISRA_CPP_2008_RULE_12_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 12-1-1: An object's dynamic type shall not be used from the body of its constructor or destructor.")
	case pb.Result_MISRA_CPP_2008_RULE_12_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 12-1-2: AlI constructors of a class should explicitly call a constructor for all of its immediate base classes and all virtual base classes.")
	case pb.Result_MISRA_CPP_2008_RULE_12_1_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 12-1-3: All constructors that are callable with a single argument of fundamental type shall be declared explicit.")
	case pb.Result_MISRA_CPP_2008_RULE_12_8_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 12-8-1: A copy constructor shall only initialize its base classes and the non-static members of the class of which it is a member.")
	case pb.Result_MISRA_CPP_2008_RULE_12_8_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 12-8-2: The copy assignment operator shall be declared protected or private in an abstract class.")
	case pb.Result_MISRA_CPP_2008_RULE_14_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-5-1: A non-member genericfunction shall only be declared in a namespace that is not an associated namespace.")
	case pb.Result_MISRA_CPP_2008_RULE_14_5_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-5-2: A copy constructor shall be declared when there is a template constuctor with a single parameter that is a generic parameter.")
	case pb.Result_MISRA_CPP_2008_RULE_14_5_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-5-3: A copy assignment operator shall be declared when there is a template assignment operator with a parameter that is a generic parameter:")
	case pb.Result_MISRA_CPP_2008_RULE_14_6_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-6-1: In a class template with a dependent base, any name that may be found in that dependent base shall be referred to using a qualified-id or this->.")
	case pb.Result_MISRA_CPP_2008_RULE_14_6_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-6-2: The function chosen by overload resolution shall resolve to a function declared previously in the translation unit.")
	case pb.Result_MISRA_CPP_2008_RULE_14_7_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-7-1: All class templates, function templates, class template member functions and class template static members shall be instantiated at least once.")
	case pb.Result_MISRA_CPP_2008_RULE_14_7_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-7-2: For any given template specialization, an explicit instantiation of the template with the template arguments used in the specializaion shall not render the program ill-formed")
	case pb.Result_MISRA_CPP_2008_RULE_14_7_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-7-3: All partial and explicit specializations for a template shall be declared in the same file as the declaration of their primary template.")
	case pb.Result_MISRA_CPP_2008_RULE_14_8_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-8-1: Overloaded function templates shall not be explicitly specialized.")
	case pb.Result_MISRA_CPP_2008_RULE_14_8_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 14-8-2: The viable function set for a function call should either contain no function specializaions, or only contain function specializations.")
	case pb.Result_MISRA_CPP_2008_RULE_15_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-0-1: Exceptions shall only be used for error handling.")
	case pb.Result_MISRA_CPP_2008_RULE_15_0_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-0-2: An exception object should not have pointer type.")
	case pb.Result_MISRA_CPP_2008_RULE_15_0_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-0-3: Control shall not be transferred into a try or catch block using a goto or a switch statement.")
	case pb.Result_MISRA_CPP_2008_RULE_15_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-1-1: The assignment-expression of a throw statement shall not itself cause an exception to be thrown.")
	case pb.Result_MISRA_CPP_2008_RULE_15_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-1-2: NULL shall not be thrown explicitly.")
	case pb.Result_MISRA_CPP_2008_RULE_15_1_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-1-3: An empty throw (throw;) shall only be used in the compound-statement of a catch handler.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-1: Exceptions shall be raised only after start-up and before termination of the program.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-2: There should be at least one exception handler to catch all otherwise unhandled exceptions.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-3: Handlers of a function-try-block implementation of a class constructor or destructor shall not reference non-static members from this class or its bases.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-4: Each exception explicitly thrown in the code shall have a handler of a compatible type in all calll paths that could lead to that point.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-5: A class type exception shall always be caught by reference.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-6: Where multiple handlers are provided in a single try-catch statement or, fuction-try-block for a derived class and some or all of its bases, the handlers shall be ordered most-derived to base class.")
	case pb.Result_MISRA_CPP_2008_RULE_15_3_7:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-3-7: Where multiple handlers are provided in a single try-catch statement or function-try-block, any elipsis (catch-all) handler shall occur last.")
	case pb.Result_MISRA_CPP_2008_RULE_15_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-4-1: If a function is declared with an exception-specification, then all declarations of the same function (in other translaion units) shall be declared with the same set of type -ids.")
	case pb.Result_MISRA_CPP_2008_RULE_15_5_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-5-1: A class destructor shall not exit with an exception.")
	case pb.Result_MISRA_CPP_2008_RULE_15_5_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-5-2: Where a function's declaration includes an exception-specification, the function shall only be capable of throwing exceptions of the indicated type(s).")
	case pb.Result_MISRA_CPP_2008_RULE_15_5_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 15-5-3: The terminate() function shall not be called implicitly.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-1: #include directives in a file shall only be preceded by other preprocessor directives or comments.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-2: Macros shall only be #define’d or #undef'd in the global namespace.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-3: #undef shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-4: Function-like macros shall not be defined.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-5: Arguments to a function-like macro shall not contain tokens that look like preprocessing directives.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-6: In the definition of a function-like macro, each instance of a parameter shall be enclosed in parentheses, unless it is used as the operand of # or ##.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_7:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-7: Undefined macro identifiers shall not be used in #if or #elif preprocessor directives, except as operands to the defined operator.")
	case pb.Result_MISRA_CPP_2008_RULE_16_0_8:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-0-8: If the # token appears as the first token on a line, then it shall be immediately followed by a preprocessing token.")
	case pb.Result_MISRA_CPP_2008_RULE_16_1_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-1-1: The defined preprocessor operator shall only be used in one of the two standard forms.")
	case pb.Result_MISRA_CPP_2008_RULE_16_1_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-1-2: All #else, #elif and #endif preprocessor directives shall reside in the same file as the #if or #ifdef directive to which they are related.")
	case pb.Result_MISRA_CPP_2008_RULE_16_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-2-1: The pre-processor shall only be used for file inclusion and include guards.")
	case pb.Result_MISRA_CPP_2008_RULE_16_2_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-2-2: C++ macros shall only be used for include guards, type qualifiers, or storage class specifiers.")
	case pb.Result_MISRA_CPP_2008_RULE_16_2_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-2-3: Include guards shall be provided.")
	case pb.Result_MISRA_CPP_2008_RULE_16_2_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-2-4: The ', \", /* or // characters shall not occur in a header file name.")
	case pb.Result_MISRA_CPP_2008_RULE_16_2_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-2-5: The \\ character should not occur in a header file name.")
	case pb.Result_MISRA_CPP_2008_RULE_16_2_6:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-2-6: The #include directive shall be followed by either a <filename> or \"filename\" sequence.")
	case pb.Result_MISRA_CPP_2008_RULE_16_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-3-1: There shall be at most one occurrence of the # or ## operators in a single macro definition.")
	case pb.Result_MISRA_CPP_2008_RULE_16_3_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-3-2: The # and ## operators should not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_16_6_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 16-6-1: All uses of the #pragma directive shall be documented.")
	case pb.Result_MISRA_CPP_2008_RULE_17_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 17-0-1: Reserved identifiers, macros and functions in the standard library shall not be defined, redefined or undefined.")
	case pb.Result_MISRA_CPP_2008_RULE_17_0_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 17-0-2: The names of standard library macros and objects shall not be reused.")
	case pb.Result_MISRA_CPP_2008_RULE_17_0_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 17-0-3: The names of standard library functions shall not be overridden.")
	case pb.Result_MISRA_CPP_2008_RULE_17_0_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 17-0-4: All library code shall conform to MISRA C++.")
	case pb.Result_MISRA_CPP_2008_RULE_17_0_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 17-0-5: The setjmp macro and the longjmp function shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-0-1: The C library shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_0_2:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-0-2: The library functions atof, atoi and atol from library <cstdlib> shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_0_3:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-0-3: The library functions abort, exit, getenv and system from library <cstdlib> shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_0_4:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-0-4: The time handling functions of library <ctime> shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_0_5:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-0-5: The unbounded functions of library <cstring> shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_2_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-2-1: The macro offsetof shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_4_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-4-1: Dynamic heap memory allocation shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_18_7_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 18-7-1: The signal handling facilities of <csignal> shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_19_3_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 19-3-1: The error indicator errno shall not be used.")
	case pb.Result_MISRA_CPP_2008_RULE_27_0_1:
		return p.Sprintf("Violation of MISRA C++:2008 Rule 27-0-1: The stream input/output library <cstdio> shall not be used.")
	default:
		return result.ErrorMessage
	}
}

func LocalizeErrorMessages(allResults *pb.ResultsList, lang string) {
	p := GetPrinter(lang)
	for _, result := range allResults.Results {
		if strings.Contains(result.ErrorMessage, "misra-c2012") || strings.Contains(result.ErrorMessage, "NAIVESYSTEMS_C7966") {
			result.ErrorMessage = localizeErrorMessage(result, p)
		} else {
			prefix := strings.Split(result.ErrorMessage, ": ")[0]
			result.ErrorMessage = prefix + ": " + localizeErrorMessage(result, p)
		}
	}
}

func GetPrinter(lang string) *message.Printer {
	var langTag language.Tag
	if _, exist := languageMap[lang]; exist {
		langTag = languageMap[lang]
	} else {
		langTag = languageMap["zh"]
	}
	return message.NewPrinter(langTag)
}
