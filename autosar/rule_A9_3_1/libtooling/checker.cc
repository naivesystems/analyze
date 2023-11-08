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

#include "autosar/rule_A9_3_1/libtooling/checker.h"

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
      "Member functions shall not return non-const \"raw\" pointers or references to private or protected data owned by the class.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A9_3_1 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    const auto CMDReturnsRawPtrOrRef =
        cxxMethodDecl(
            returns(anyOf(qualType(pointerType(), unless(isConstQualified())),
                          qualType(referenceType(), unless(isConstQualified())),
                          references(namedDecl(matchesName("std::unique_ptr"),
                                               isExpansionInSystemHeader())))),
            unless(isExpansionInSystemHeader()))
            .bind("cmd");
    finder->addMatcher(
        returnStmt(
            anyOf(
                has(memberExpr(hasObjectExpression(cxxThisExpr())).bind("me")),
                has(unaryOperator(
                    hasOperatorName("&"),
                    hasUnaryOperand(
                        memberExpr(hasObjectExpression(cxxThisExpr()))
                            .bind("me"))))),
            hasAncestor(CMDReturnsRawPtrOrRef))
            .bind("rs"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXMethodDecl* cmd = result.Nodes.getNodeAs<CXXMethodDecl>("cmd");
    const ReturnStmt* rs = result.Nodes.getNodeAs<ReturnStmt>("rs");
    const MemberExpr* me = result.Nodes.getNodeAs<MemberExpr>("me");
    if (cmd && rs && me) {
      const CXXRecordDecl* crd = cmd->getParent();
      const Decl* d = me->getMemberDecl();

      bool isContainerMimic = true;
      for (auto field : crd->fields()) {
        if (!field->getType()->isPointerType()) {
          isContainerMimic = false;
        }
      }
      if (isContainerMimic) return;

      if (d->getAccess() != AS_public)
        ReportError(
            misra::libtooling_utils::GetFilename(rs, result.SourceManager),
            misra::libtooling_utils::GetLine(rs, result.SourceManager),
            results_list_);
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
}  // namespace rule_A9_3_1
}  // namespace autosar
