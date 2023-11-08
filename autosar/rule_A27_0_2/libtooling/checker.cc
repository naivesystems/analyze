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

#include "autosar/rule_A27_0_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A C-style string shall guarantee sufficient space for data and the null terminator.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A27_0_2 {
namespace libtooling {

// The function list is referenced from:
// https://clang.llvm.org/docs/analyzer/checkers.html#alpha-unix-cstring-outofbounds-c
unordered_set<string> dangerous_func_set = {"memcpy",  "bcopy",  "strcpy",
                                            "strncpy", "strcat", "strncat",
                                            "memmove", "memcmp", "memset"};

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    Matcher<Decl> c_string_var =
        varDecl(hasType(arrayType(hasElementType(isAnyCharacter()))))
            .bind("c_string_var");
    Matcher<Stmt> c_string_ref =
        declRefExpr(to(c_string_var), unless(isExpansionInSystemHeader()))
            .bind("decl_ref");
    // to match operators >> ... or ... << ;
    finder->addMatcher(cxxOperatorCallExpr(hasAnyOperatorName(">>"),
                                           hasRHS(hasDescendant(c_string_ref))),
                       this);
    finder->addMatcher(cxxOperatorCallExpr(hasAnyOperatorName("<<"),
                                           hasLHS(hasDescendant(c_string_ref))),
                       this);
    // to match str(...);
    finder->addMatcher(
        cxxConstructExpr(
            hasDescendant(c_string_ref),
            // filter cases like std::string str(buffer, in.gcount());
            unless(hasDescendant(declRefExpr(unless(to(c_string_var)))))),
        this);
    // to match functions like strcpy(...);
    finder->addMatcher(
        callExpr(
            hasDescendant(c_string_ref),
            callee(functionDecl(isExpansionInSystemHeader())),
            // Filter cases that have a size check in an ancestor IfStmt.
            // It may introduce some false negative cases since the upper
            // bound or lower bound of strlen() may not be properly specified.
            unless(hasAncestor(ifStmt(has(binaryOperator(
                hasAnyOperatorName(">=", ">", "<=", "<"),
                hasEitherOperand(callExpr(
                    callee(functionDecl(hasName("strlen"),
                                        isExpansionInSystemHeader())),
                    hasDescendant(declRefExpr(
                        to(varDecl(equalsBoundNode("c_string_var")))))))))))),
            unless(isExpansionInSystemHeader()))
            .bind("call_expr"),
        this);
    // to match readlink(...);
    finder->addMatcher(
        functionDecl(
            // len = readlink(link, buff, sizeof(buff));
            hasDescendant(binaryOperator(
                isAssignmentOperator(),
                hasLHS(declRefExpr(to(varDecl().bind("len_var")))),
                hasRHS(hasDescendant(
                    callExpr(callee(functionDecl(hasName("readlink"),
                                                 isExpansionInSystemHeader())),
                             hasDescendant(declRefExpr(to(c_string_var)))))))),
            // buff[len] = ...;
            hasDescendant(binaryOperator(
                isAssignmentOperator(),
                hasLHS(arraySubscriptExpr(
                    hasBase(hasDescendant(
                        declRefExpr(
                            to(varDecl(equalsBoundNode("c_string_var"))))
                            .bind("decl_ref"))),
                    hasIndex(hasDescendant(declRefExpr(
                        to(varDecl(equalsBoundNode("len_var")))))))),
                // Filter cases that have a size check in an ancestor IfStmt. It
                // may introduce some false negative cases since the upper bound
                // specified for "<" or ">" might exceed the buffer size.
                unless(hasAncestor(ifStmt(has(binaryOperator(
                    hasOperatorName("&&"),
                    // len != -1: -1 means errors in readlink()
                    hasEitherOperand(binaryOperator(
                        hasOperatorName("!="),
                        hasEitherOperand(hasDescendant(declRefExpr(
                            to(varDecl(equalsBoundNode("len_var")))))),
                        hasEitherOperand(unaryOperator(
                            hasOperatorName("-"),
                            hasUnaryOperand(integerLiteral(equals(1))))))),
                    // len < ... or ... > len
                    hasEitherOperand(anyOf(
                        binaryOperator(
                            hasAnyOperatorName("<"),
                            hasLHS(hasDescendant(declRefExpr(
                                to(varDecl(equalsBoundNode("len_var"))))))),
                        binaryOperator(
                            hasAnyOperatorName(">"),
                            hasRHS(hasDescendant(declRefExpr(to(varDecl(
                                equalsBoundNode("len_var")))))))))))))))),
            unless(isExpansionInSystemHeader())),
        this);
    // to match functions that use "!= '\0'" condition to check and process a
    // C-style string
    finder->addMatcher(
        callExpr(
            callee(functionDecl(
                parameterCountIs(1),
                hasDescendant(forStmt(hasCondition(binaryOperator(
                    hasOperatorName("!="),
                    hasEitherOperand(
                        hasDescendant(characterLiteral(equals(0)))),
                    hasEitherOperand(
                        hasDescendant(arraySubscriptExpr(hasBase(hasDescendant(
                            declRefExpr(to(parmVarDecl(hasType(pointerType(
                                pointee(isAnyCharacter()))))))))))))))))),
            hasDescendant(c_string_ref), unless(isExpansionInSystemHeader())),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const DeclRefExpr* decl_ref =
        result.Nodes.getNodeAs<DeclRefExpr>("decl_ref");
    string path =
        misra::libtooling_utils::GetFilename(decl_ref, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(decl_ref, result.SourceManager);

    const CallExpr* call_expr = result.Nodes.getNodeAs<CallExpr>("call_expr");
    if (call_expr != nullptr) {
      string function_name = misra::libtooling_utils::GetLibFDNameOfCallExpr(
          call_expr, result.Context);
      if (dangerous_func_set.find(function_name) != dangerous_func_set.end()) {
        ReportError(path, line, results_list_);
      }
      return;
    }

    ReportError(path, line, results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A27_0_2
}  // namespace autosar
