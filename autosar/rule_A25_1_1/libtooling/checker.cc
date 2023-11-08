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

#include "autosar/rule_A25_1_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/QualTypeNames.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;
using namespace clang;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Non-static data members or captured values of predicate function objects that are state related to this object's identity shall not be copied.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A25_1_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    BindableMatcher<clang::Decl> pred = parmVarDecl(hasName("__pred"));
    Matcher<clang::Decl> non_const_parm =
        parmVarDecl(
            unless(anyOf(hasType(referenceType(pointee(isConstQualified()))),
                         hasType(pointerType(pointee(isConstQualified()))),
                         hasType(isConstQualified()))))
            .bind("non_const_parm");
    VariadicOperatorMatcher<
        clang::ast_matchers::internal::BindableMatcher<clang::Stmt>,
        clang::ast_matchers::internal::BindableMatcher<clang::Stmt>>
        changed_var =
            anyOf(memberExpr(hasDescendant(cxxThisExpr())),
                  declRefExpr(hasDeclaration(varDecl(
                      // filter out the local variable in the lambda expression
                      unless(hasAncestor(lambdaExpr()))))));
    Matcher<clang::Stmt> unary_operator =
        unaryOperator(hasUnaryOperand(changed_var)).bind("op");
    Matcher<clang::Stmt> binary_operator =
        binaryOperator(isAssignmentOperator(), hasLHS(changed_var)).bind("op");

    finder->addMatcher(
        callExpr(
            // match the function which has predicate parameter
            callee(functionDecl(
                // filter out warpped as a std::reference_wrapper
                unless(hasDescendant(
                    callExpr(callee(functionDecl(hasName("std::ref")))))),
                hasDescendant(pred),
                // match the constructor function used as predicate
                hasDescendant(
                    cxxConstructExpr(hasDeclaration(cxxConstructorDecl(
                        // match the class definition
                        hasParent(cxxRecordDecl(
                            // match the overload function of call operator
                            // which has non const parameter or modify the
                            // data memeber
                            hasDescendant(cxxMethodDecl(
                                hasOverloadedOperatorName("()"),
                                anyOf(hasDescendant(non_const_parm),
                                      hasDescendant(unary_operator),
                                      hasDescendant(binary_operator)))))),
                        unless(isExpansionInSystemHeader()))))))),
            unless(isExpansionInSystemHeader()))
            .bind("ce"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const Stmt* op = result.Nodes.getNodeAs<Stmt>("op");
    if (op != nullptr) {
      std::string path =
          misra::libtooling_utils::GetFilename(op, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(op, result.SourceManager);
      ReportError(path, line_number, results_list_);
    }
    const Decl* non_const_parm = result.Nodes.getNodeAs<Decl>("non_const_parm");
    if (non_const_parm != nullptr) {
      std::string path = misra::libtooling_utils::GetFilename(
          non_const_parm, result.SourceManager);
      int line_number = misra::libtooling_utils::GetLine(non_const_parm,
                                                         result.SourceManager);
      ReportError(path, line_number, results_list_);
    }
    const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("ce");
    if (ce != nullptr) {
      std::string path =
          misra::libtooling_utils::GetFilename(ce, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(ce, result.SourceManager);
      ReportError(path, line_number, results_list_);
    }
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
}  // namespace rule_A25_1_1
}  // namespace autosar
