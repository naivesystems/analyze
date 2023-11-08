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

#include "googlecpp/g1165/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Objects with static storage duration are forbidden unless they are trivially destructible";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1165 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // Find all variables with static storage duration
    finder->addMatcher(varDecl(hasStaticStorageDuration()).bind("staticvar"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const auto* var = result.Nodes.getNodeAs<VarDecl>("staticvar")) {
      if (misra::libtooling_utils::IsInSystemHeader(var, result.Context))
        return;

      // Allowed: constexpr guarantees trivial destructor
      if (var->isConstexpr()) return;

      QualType type = var->getType();
      // Get the base type of the array type
      while (type->isArrayType()) {
        const ArrayType* arrType = type->castAsArrayTypeUnsafe();
        type = arrType->getElementType();
      }
      // If the variable is an object of a class record type
      if (CXXRecordDecl* recordDecl = type->getAsCXXRecordDecl()) {
        CXXDestructorDecl* destructor = recordDecl->getDestructor();
        // Sometimes getDestructor() returns null because the user does not
        // write a desctructor, but the default desctructor is trivial,
        // so don't report in this case
        if (!destructor) return;
        // Judge whether the object is trivially destructable
        if (destructor->isTrivial()) return;

        ReportError(
            misra::libtooling_utils::GetFilename(var, result.SourceManager),
            misra::libtooling_utils::GetLine(var, result.SourceManager),
            results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1165
}  // namespace googlecpp
