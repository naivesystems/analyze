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

#include "autosar/rule_A3_1_6/libtooling/checker.h"

#include <glog/logging.h>

#include <algorithm>
#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Trivial accessor and mutator functions should be inlined.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A3_1_6 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxMethodDecl(unless(isExpansionInSystemHeader()),
                      unless(cxxConstructorDecl()), unless(cxxDestructorDecl()))
            .bind("method"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const auto* md = result.Nodes.getNodeAs<CXXMethodDecl>("method");
    if (!md->hasBody()) {
      return;
    }
    if (md->isTrivial()) {
      return;
    }
    if (!IsAccessor(md) && !IsMutator(md)) {
      return;
    }
    auto body = md->getBody();
    if (body) {
      int num_statements = 0;
      for (const auto* child : body->children()) {
        num_statements++;
        if (num_statements > 1) {
          ReportError(
              misra::libtooling_utils::GetFilename(md, result.SourceManager),
              misra::libtooling_utils::GetLine(md, result.SourceManager),
              results_list_);
          return;
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;

  bool IsAccessor(CXXMethodDecl const* md) {
    auto body = md->getBody();
    if (body) {
      for (const auto* child : body->children()) {
        if (const auto* rs = dyn_cast<ReturnStmt>(child)) {
          if (!rs) {
            continue;
          }
          const auto* retexpr = rs->getRetValue();
          if (!retexpr) {
            return false;
          }
          if (const auto* me =
                  dyn_cast<MemberExpr>(retexpr->IgnoreImplicit())) {
            const auto* rd = md->getParent();
            for (auto it = rd->field_begin(); it != rd->field_end(); it++) {
              if (it->getIdentifier() == me->getMemberNameInfo().getName()) {
                return true;
              }
            }
          }
          return false;
        }
      }
    }
    return false;
  }

  bool IsMutator(CXXMethodDecl const* md) {
    if (md->getDeclName().isIdentifier() &&
        (md->getName().startswith("Set") || md->getName().startswith("set"))) {
      return true;
    }
    return false;
  }
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_A3_1_6
}  // namespace autosar
