
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

#ifndef ANALYZER_LIBTOOLING_INCLUDES_CMD_OPTIONS_H_
#define ANALYZER_LIBTOOLING_INCLUDES_CMD_OPTIONS_H_
#include <clang/Tooling/CommonOptionsParser.h>
llvm::cl::OptionCategory ns_libtooling_checker("ns libtooling checker");
llvm::cl::opt<std::string> results_path(
    "results-path",
    llvm::cl::desc(
        "The path to the ResultsList protobuf file, default /output/results"),
    llvm::cl::init("/output/results"), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<bool> case_sensitive(
    "CaseSensitive",
    llvm::cl::desc("Set to the Case Sensitive mode, default true"),
    llvm::cl::init(true), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> limit("Limit", llvm::cl::desc("Limit count, default 31"),
                         llvm::cl::init(31),
                         llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<bool> implicit_decl(
    "ImplicitDecl",
    llvm::cl::desc("Whether consider implicit declarations, default true"),
    llvm::cl::init(true), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> struct_member_limit(
    "StructMemberLimit",
    llvm::cl::desc("Struct member limit count, default 1023"),
    llvm::cl::init(1023), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> function_parm_limit(
    "FunctionParmLimit",
    llvm::cl::desc("Function parameter limit count, default 127"),
    llvm::cl::init(127), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> function_arg_limit(
    "FunctionArgLimit",
    llvm::cl::desc("Function call argument limit count, default 127"),
    llvm::cl::init(127), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> nested_record_limit(
    "NestedRecordLimit",
    llvm::cl::desc("nested record(struct/union) level limit count, default 63"),
    llvm::cl::init(63), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> nested_expr_limit(
    "NestedExprLimit",
    llvm::cl::desc(
        "nested parenthesized expression level limit count, default 63"),
    llvm::cl::init(63), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> block_id_limit(
    "BlockIDLimit", llvm::cl::desc("block identifier limit count, default 511"),
    llvm::cl::init(511), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> switch_case_limit(
    "SwitchCaseLimit",
    llvm::cl::desc(
        "case label for a switch statement limit count, default 1023"),
    llvm::cl::init(1023), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> enum_constant_limit(
    "EnumConstantLimit",
    llvm::cl::desc("enumeration constant limit count, default 1023"),
    llvm::cl::init(1023), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> string_char_limit(
    "StringCharLimit",
    llvm::cl::desc("string literal character limit count, default 4095"),
    llvm::cl::init(4095), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> extern_id_limit(
    "ExternIDLimit",
    llvm::cl::desc(
        "external identifier limit count in one translation unit, default 4095"),
    llvm::cl::init(4095), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> macro_id_limit(
    "MacroIDLimit",
    llvm::cl::desc(
        "macro identifiers simultaneously defined in one preprocessing translation unit, default 4095"),
    llvm::cl::init(4095), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> macro_parm_limit(
    "MacroParmLimit",
    llvm::cl::desc(
        "parameter limit count in one macro definition, default 127"),
    llvm::cl::init(127), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> macro_arg_limit(
    "MacroArgLimit",
    llvm::cl::desc("argument limit count in one macro invocation, default 127"),
    llvm::cl::init(127), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> nested_block_limit(
    "NestedBlockLimit",
    llvm::cl::desc("nesting block level limit count, default 127"),
    llvm::cl::init(127), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> nested_include_limit(
    "NestedIncludeLimit",
    llvm::cl::desc("nesting level limit count for #include files, default 15"),
    llvm::cl::init(15), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> iom_id_char_limit(
    "IoMIDCharLimit",
    llvm::cl::desc(
        "significant initial characters limit count in an internal identifier or a macro name, default 63"),
    llvm::cl::init(63), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> nested_decl_limit(
    "NestedDeclLimit",
    llvm::cl::desc(
        "nesting level limit count for parenthesized declaration, default 63"),
    llvm::cl::init(63), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> modify_decl_limit(
    "ModifyDeclLimit",
    llvm::cl::desc(
        "pointer, array, and function declarators limit count (in any combinations) modifying an arithmetic, structure, union, or void type in a declaration, default 12"),
    llvm::cl::init(12), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> nested_cond_inclu_limit(
    "NestedCondIncluLimit",
    llvm::cl::desc(
        "nesting level limit count of conditional inclusion, default 63"),
    llvm::cl::init(63), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<bool> aggressive_mode(
    "AggressiveMode",
    llvm::cl::desc("Set to the Aggressive Mode, default false"),
    llvm::cl::init(false), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> maximum_inline_func_line(
    "MaximumInlineFuncLine",
    llvm::cl::desc("The maximum inline function line number, default 10"),
    llvm::cl::init(10), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<std::string> project_name("ProjectName",
                                        llvm::cl::desc("Project name"),
                                        llvm::cl::init(""),
                                        llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> maximum_allowed_func_line(
    "MaximumAllowedFuncLine",
    llvm::cl::desc("The maximum allowed function line number, default 40"),
    llvm::cl::init(40), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<int> maximum_allowed_return_num(
    "MaximumAllowedReturnNum",
    llvm::cl::desc("The maximum allowed return line number, default 2"),
    llvm::cl::init(2), llvm::cl::cat(ns_libtooling_checker));

llvm::cl::opt<bool> report_error_in_calling_system_function(
    "ReportErrorInCallingSystemFunction",
    llvm::cl::desc(
        "Set as true to report errors in calling system function, default false"),
    llvm::cl::init(false), llvm::cl::cat(ns_libtooling_checker));
#endif
