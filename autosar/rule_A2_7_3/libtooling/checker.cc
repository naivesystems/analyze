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

#include "autosar/rule_A2_7_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "All declarations of \"user-defined\" types, static and non-static data members, functions and methods shall be preceded by documentation.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A2_7_3 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // Match data members and "user-defined" types.
    finder->addMatcher(decl(anyOf(fieldDecl(), tagDecl())).bind("decl"), this);
    // Match function declarations.
    finder->addMatcher(functionDecl().bind("func_decl"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    SourceManager* source_manager = result.SourceManager;
    const Decl* decl = result.Nodes.getNodeAs<Decl>("decl");
    if (decl) {
      // Cpp will have an implict decl for recordDecl. It will duplicate with
      // the origin one. Skip it
      if (decl->isImplicit()) {
        return;
      }
      // Skip self duplicated declaration
      if (decl->getPreviousDecl()) {
        return;
      }
      // Skip using declaration.
      if (decl->getIdentifierNamespace() ==
          clang::Decl::IdentifierNamespace::IDNS_Using) {
        return;
      }
      // Get the comment attached to decl.
      auto comment = context->getRawCommentForDeclNoCache(decl);
      if (comment == nullptr) {
        int line_loc = misra::libtooling_utils::GetLine(decl, source_manager);
        string file_loc =
            misra::libtooling_utils::GetFilename(decl, source_manager);
        ReportError(file_loc, line_loc, results_list_);
      }
      return;
    }
    const FunctionDecl* func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl");
    if (func_decl) {
      // Get the comment attached to func_decl.
      auto comment = context->getRawCommentForDeclNoCache(func_decl);
      if (comment == nullptr) {
        int line_loc =
            misra::libtooling_utils::GetLine(func_decl, source_manager);
        string file_loc =
            misra::libtooling_utils::GetFilename(func_decl, source_manager);
        ReportError(file_loc, line_loc, results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_A2_7_3
}  // namespace autosar
