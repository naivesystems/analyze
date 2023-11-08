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

#ifndef ANALYZER_MISRA_RULE_8_2_CHECKER_H_
#define ANALYZER_MISRA_RULE_8_2_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_8_2 {

class FuncParmVarDeclCallback;

/*
 * From [misra-c2012-8.2]
 * Function types shall be in prototype form with named parameters.
 *
 * According to the description of the rule and existing implementation
 * of rule 8.2 in cppcheck misra.py, the tasks are as follows:
 *
 * 1. Check if there is any function paramter with no name, no matter
 *    where it is (typedef, function pointer, function paramter list, etc.)
 *
 * 2. Check if the function declaration is of K&R style.
 *
 * 3. Check if the names of parameters in declaration is consistent with
 *    those in definition.
 *
 * There are some facts to know:
 *
 * Facts:
 * 1. Every function parameter (including those in function pointer)
 *    is of type ParmVarDecl, whose name can be easily accessed by
 *    'ParVarDecl.getName()'.
 *
 * 2. For ParmVarDecl, only those in normal function declarations will have
 *    'getParentFunctionOrMethod()' returned FunctionDecl nodes. For parameters
 *    in function pointers the return values will be NULL since they are not
 *    defined inside a function / method / block.
 *
 * 3. Function with no parameter will have no ParmVarDecl child.
 *
 * 4. For K&R style function declaration, clang has a specific type
 *    FunctionNoProtoType to denote K&R declarations with no explicit parameter
 *    like the example below:
 *    1    int func();
 *    while the other normal style declaration will have type FunctionProtoType.
 *    For K&R declarations with parameters:
 *    1    int func(a, b)
 *    2    int a;
 *    3    double b;
 *    4    { return a + b; }
 *    There is no specific type for that kind of declaration in clang. But clang
 *    will set the location of K&R parameters to where they are typed defined,
 *    in this example the location of parameter 'a' will be in line 2 and
 *    parameter 'b' will be in line 3. That gives us a stright-forward way to
 *    identify if a declaration is K&R style by checking the relative location
 * of ParVarDecl and right parenthesis of the function parameter list, where the
 *    latter one can be retrieved by
 *    'FunctionDecl.getFunctionTypeLoc().getRParenLoc()'.
 *
 * 5. For FunctionDecl, it is easy to get its corresponding definition by
 *    'FunctionDecl.getDefinition()' if current FunctionDecl is a declaration.
 *
 * 6. If a node returns non-NULL by calling 'getFunctionType()', then it is
 *    a function pointer or a function declaration.
 *
 * Procedure:
 * Based on above facts, the matcher should match all NamedNode in source and
 * check whether it is ParmDeclNode first (Fact 1).
 * - If yes, then check if it has FunctionDecl as parent (Fact 2).
 *   - If parent is FunctionDecl, it means the current ParmVarDecl is in a
 *     function declaration / definition. Then check the name of the parameter.
 *     - If the name is empty, then the rule is violated, report an error.
 *     - If the name is not empty, then further check if the parent FunctionDecl
 *       is a definition.
 *       - If it is a definition, follow Fact 4 to check if the definition is
 *         K&R style and report corresponding error. Note that this is the
 *         only check needed to be run on definitions, since the checking of
 *         parameter names is done at previous branch, which covers all kinds of
 *         declarations / definitions. And checking of name consistency should
 *         be carried on declarations since definitions do not need to match
 *         itself.
 *       - If it is a declaration, then by Fact 5 check the name consistency
 *         with its definition, if the function is defined.
 *   - If the parent is NULL, then the current ParmVarDecl is in a function
 *     pointer. Check if the name is empty and report error if necessary.
 * By now every parameter in source is checked, no matter it is in typedef, in
 * function pointer, or in function declaration. Also the name consistency is
 * guaranteed. So task 1 & 3 are done.
 * But task 2 is not fully finished since only function with non-zero paramters
 * are checked. function / function pointer with no parameter have not been
 * checked yet since they have no ParmVarDecl, accessing through ParmDeclNode
 * won't reach them (Fact 3).
 * The else branch is right for solving this.
 * - If no, that means the node is not a ParmDeclNode. Then check if the
 *   NamedNode has FunctionType (Fact 6).
 *   - If yes, then check if the node is of type FunctionNoProtoType to
 *     determine whether it is K&R style declaration with no parameter. If it
 *     is, then report an error. If no, then it means the type has properly
 *     filled the parameter list with a 'void'.
 * Then task 2 is done.
 *
 */

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* result_list);
  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  FuncParmVarDeclCallback* func_parm_named_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_8_2
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_8_2_CHECKER_H_
