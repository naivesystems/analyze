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

#include "autosar/rule_A20_8_1/libtooling/checker.h"

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
      "An already-onwed pointer value shall not be stored in an unrelated smart pointer.";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A20_8_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxConstructExpr().bind("cce"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const CXXConstructExpr* cce =
        result.Nodes.getNodeAs<CXXConstructExpr>("cce");
    if (cce) {
      const CXXRecordDecl* crd_type = cce->getType()->getAsCXXRecordDecl();
      if (cce->getNumArgs() >= 1 && isSmartPtrType(crd_type, result.Context)) {
        const DeclRefExpr* arg =
            dyn_cast<DeclRefExpr>(cce->getArg(0)->IgnoreImpCasts());
        if (arg && arg->getType()->isPointerType() &&
            !already_owned_ptrs.insert(arg->getDecl()).second)
          ReportError(GetFilename(cce, result.SourceManager),
                      GetLine(cce, result.SourceManager), results_list_);
      }
    }
  }

 private:
  set<const ValueDecl*> already_owned_ptrs{};
  ResultsList* results_list_;
  bool isSmartPtrType(const CXXRecordDecl* crd, ASTContext* context) {
    if (!crd || !context) return false;
    string type = crd->getQualifiedNameAsString();
    return IsInSystemHeader(crd, context) &&
           (type == "std::unique_ptr" || type == "std::shared_ptr" ||
            type == "std::auto_ptr" || type == "std::weak_ptr");
  }
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A20_8_1
}  // namespace autosar
