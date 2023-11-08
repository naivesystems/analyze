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

#include "autosar/rule_A18_9_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;

namespace {

void ReportError(const string& path, int line_number,
                 ResultsList* results_list) {
  string error_message =
      "Forwarding values to other functions shall be done via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding reference.";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A18_9_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        callExpr(unless(hasAncestor(functionDecl(isInstantiated()))),
                 unless(isExpansionInSystemHeader()))
            .bind("ce"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("ce");
    if (ce) {
      if (CalleeIsMoveOrForward(ce)) return;
      for (const Expr* arg : ce->arguments()) {
        arg = arg->IgnoreImpCasts();
        bool need_report = false;
        if (const CallExpr* ce_arg = dyn_cast<CallExpr>(arg)) {
          if (unsigned isMoveOrForward = CalleeIsMoveOrForward(ce_arg)) {
            const DeclRefExpr* moved_or_forwarded_arg =
                dyn_cast<DeclRefExpr>(ce_arg->getArg(0)->IgnoreImpCasts());
            if (moved_or_forwarded_arg) {
              QualType ref_qt = moved_or_forwarded_arg->getDecl()->getType();
              if (isMoveOrForward == 1)
                need_report = isForwardingReferenceForThisRule(ref_qt);
              if (isMoveOrForward == 2)
                need_report = !isForwardingReferenceForThisRule(ref_qt);
            } else if (isMoveOrForward == 2)
              need_report = true;
          }
        } else if (const DeclRefExpr* dre_arg = dyn_cast<DeclRefExpr>(arg)) {
          need_report = dre_arg->getDecl()->getType()->isRValueReferenceType();
        }
        if (need_report)
          ReportError(GetFilename(arg, result.SourceManager),
                      GetLine(arg, result.SourceManager), results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
  // return 0 if neither or error; 1 if move; 2 if forward
  unsigned CalleeIsMoveOrForward(const CallExpr* ce) {
    if (!ce || ce->getNumArgs() != 1) return 0;
    const FunctionDecl* fd_callee = ce->getDirectCallee();
    const UnresolvedLookupExpr* ule_callee =
        dyn_cast<UnresolvedLookupExpr>(ce->getCallee());
    string callee_name{};
    if (fd_callee) {
      callee_name = fd_callee->getQualifiedNameAsString();
    } else if (ule_callee) {
      callee_name = getQualifiedName(ule_callee);
    }
    if (callee_name == "std::move") {
      return 1;
    } else if (callee_name == "std::forward") {
      return 2;
    } else {
      return 0;
    }
  }
  bool isForwardingReferenceForThisRule(QualType qt) {
    if (isForwardingReference(qt)) return true;
    if (qt->isRValueReferenceType()) {
      // As auto is implemented with argument deduction rules,
      // auto&& is considered as forwarding reference in this rule
      return dyn_cast<AutoType>(qt.getNonReferenceType());
    }
    return false;
  }
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A18_9_2
}  // namespace autosar
