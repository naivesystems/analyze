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

// This rule only focus on one use, so it's not a dead store problem

#include "misra_cpp_2008/rule_0_1_8/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list_) {
  std::string error_message =
      absl::StrFormat("具有无效返回类型的函数必须有外部副作用");
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_0_1_8);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_0_1_8 {
namespace libtooling {

// This rule focus on the function definition, not function call.
// HasSideEffects is a member function for clang::Expr::EvalStatus,
// But we cannot eval a function decl as a Expr, so Checking side
// effects need to be done manually.
// I'm trying to cover most cases:
// A function have not external side effects should:
// - returns void type
// - have no parameter whose type is a pointer type or reference type
// - does not access any non-local variable
// - does not using volatile objects
// - does not call any other functions(Include read or write files,
//   to avoid false negatives, for example, call another function
//   which writes to a file)
// - does not raise any exception
// We need to report on definition, so declaration is skipped.
void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  finder_.addMatcher(
      // have no parameter whose type is a pointer type or reference type
      functionDecl(unless(hasAnyParameter(anyOf(hasType(pointerType()),
                                                hasType(referenceType())))),
                   // returns void type
                   returns(voidType()),
                   // does not call any other functions
                   unless(hasDescendant(callExpr())),
                   // skipped declaration
                   isDefinition(),
                   // does not access any non-local variable
                   unless(hasDescendant(
                       declRefExpr(to(varDecl(unless(hasLocalStorage())))))),
                   // does not access any member
                   unless(hasDescendant(memberExpr(has(cxxThisExpr())))),
                   // does not using volatile objects
                   unless(hasDescendant(declRefExpr(
                       to(varDecl(hasType(isVolatileQualified())))))),
                   // does not raise any exception
                   unless(hasDescendant(cxxThrowExpr())),
                   // is not constructor
                   unless(cxxConstructorDecl()),
                   // is not destructor
                   unless(cxxDestructorDecl()))
          .bind("func_decl"),
      this);
}

void Checker::run(const MatchFinder::MatchResult& result) {
  clang::ASTContext* context = result.Context;
  auto fd = result.Nodes.getNodeAs<FunctionDecl>("func_decl");
  // skip system header
  if (misra::libtooling_utils::IsInSystemHeader(fd, result.Context)) {
    return;
  }
  if (fd->isDefaulted()) {
    return;
  }
  ReportError(misra::libtooling_utils::GetFilename(fd, result.SourceManager),
              misra::libtooling_utils::GetLine(fd, result.SourceManager),
              results_list_);
}

}  // namespace libtooling
}  // namespace rule_0_1_8
}  // namespace misra_cpp_2008
