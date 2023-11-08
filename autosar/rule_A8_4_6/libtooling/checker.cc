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

#include "autosar/rule_A8_4_6/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "\"forward\" parameters declared as T && shall always be forwarded.";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A8_4_6 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        declRefExpr(
            to(parmVarDecl(hasAncestor(functionTemplateDecl().bind("ftd")))
                   .bind("pvd")),
            hasParent(stmt().bind("stmt")), unless(isExpansionInSystemHeader()))
            .bind("dre"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const DeclRefExpr* dre = result.Nodes.getNodeAs<DeclRefExpr>("dre");
    const ParmVarDecl* pvd = result.Nodes.getNodeAs<ParmVarDecl>("pvd");
    const FunctionTemplateDecl* ftd =
        result.Nodes.getNodeAs<FunctionTemplateDecl>("ftd");
    if (const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("stmt"))
      if (const UnresolvedLookupExpr* ule =
              dyn_cast<UnresolvedLookupExpr>(ce->getCallee())) {
        for (const Decl* possible_callee : ule->decls()) {
          const FunctionTemplateDecl* ft_callee =
              dyn_cast<FunctionTemplateDecl>(possible_callee);
          if (ft_callee &&
              ft_callee->getQualifiedNameAsString().find("std::forward") !=
                  string::npos &&
              IsInSystemHeader(ft_callee, result.Context))
            return;
        }
      }

    if (dre && isForwardingReference(pvd->getType(), ftd->getTemplateDepth()))
      ReportError(GetFilename(dre, result.SourceManager),
                  GetLine(dre, result.SourceManager), results_list_);
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
}  // namespace rule_A8_4_6
}  // namespace autosar
