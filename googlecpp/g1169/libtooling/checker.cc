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

#include "googlecpp/g1169/libtooling/checker.h"

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
      "A class's public API must make clear whether the class is copyable, move-only, or neither copyable nor movable";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1169 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxRecordDecl(
            unless(isImplicit()), unless(isExpansionInSystemHeader()),
            isClass(),
            optionally(forEachDescendant(cxxConstructorDecl(isCopyConstructor())
                                             .bind("copy_construct"))),
            optionally(
                forEachDescendant(cxxMethodDecl(isCopyAssignmentOperator())
                                      .bind("copy_assignment"))),
            optionally(forEachDescendant(cxxConstructorDecl(isMoveConstructor())
                                             .bind("move_construct"))),
            optionally(
                forEachDescendant(cxxMethodDecl(isMoveAssignmentOperator())
                                      .bind("move_assignment"))))
            .bind("record"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;
    auto* copy_cons =
        result.Nodes.getNodeAs<CXXConstructorDecl>("copy_construct");
    auto* copy_assign =
        result.Nodes.getNodeAs<CXXMethodDecl>("copy_assignment");
    auto* move_cons =
        result.Nodes.getNodeAs<CXXConstructorDecl>("move_construct");
    auto* move_assign =
        result.Nodes.getNodeAs<CXXMethodDecl>("move_assignment");

    auto* record_decl = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    if (!record_decl->hasDefinition()) {
      return;
    }
    if (record_decl->isAbstract()) {
      return;
    }
    if (!record_decl->hasPrivateFields()) {
      return;
    }
    // The constructors and assignments will be NULL
    // if they are not explictly declared or deleted
    if (copy_cons && copy_assign) {
      return;
    }
    if (move_assign && move_cons) {
      return;
    }
    ReportError(
        misra::libtooling_utils::GetFilename(record_decl, source_manager),
        misra::libtooling_utils::GetLine(record_decl, source_manager),
        results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1169
}  // namespace googlecpp
