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

#include "googlecpp/g1269/libtooling/checker.h"

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
      "Should give only one other class access to a class member";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1269 {
namespace libtooling {

set<uint64_t> record_decl_set;

class FriendInSameFileCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxRecordDecl(unless(isExpansionInSystemHeader()),
                      forEachDescendant(friendDecl().bind("friend")))
            .bind("record"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;

    auto* record_decl = result.Nodes.getNodeAs<RecordDecl>("record");
    auto* friend_decl = result.Nodes.getNodeAs<FriendDecl>("friend");
    if (friend_decl->getFriendType() == NULL) {
      // We only report for the friend method that belong to other classs'
      // class A{
      //    void f1();
      // }
      // class B{
      //  friend void A::f1();
      //  friend void f2();
      // }
      // so we will skip f2() here
      if (!isa<CXXMethodDecl>(friend_decl->getFriendDecl())) {
        return;
      }
    }
    // report error when we matched the friend_decls have the same record_decl
    // parent
    if (!record_decl_set.insert(record_decl->getID()).second) {
      ReportError(
          misra::libtooling_utils::GetFilename(friend_decl, source_manager),
          misra::libtooling_utils::GetLine(friend_decl, source_manager),
          results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new FriendInSameFileCallback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1269
}  // namespace googlecpp
