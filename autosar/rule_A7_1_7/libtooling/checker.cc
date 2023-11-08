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

#include "autosar/rule_A7_1_7/libtooling/checker.h"

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
      "Each expression statement and identifier declaration shall be placed on a separate line.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A7_1_7 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    last_decl_line_ = -1;
    has_reported_in_last_decl_line_ = false;

    finder->addMatcher(declStmt(unless(hasParent(forStmt())),
                                unless(isExpansionInSystemHeader()))
                           .bind("stmt"),
                       this);
    finder->addMatcher(
        typedefDecl(unless(isExpansionInSystemHeader())).bind("decl"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const DeclStmt* stmt = result.Nodes.getNodeAs<DeclStmt>("stmt");
    const Decl* decl = result.Nodes.getNodeAs<Decl>("decl");

    if (stmt) {
      int cnt = 0;
      for (auto child : stmt->decls()) {
        if (isa<VarDecl>(child)) {
          cnt++;
        }
        if (cnt > 1) {
          std::string path =
              misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
          int line_number =
              misra::libtooling_utils::GetLine(stmt, result.SourceManager);
          ReportError(path, line_number, results_list_);
          return;
        }
      }
    }

    if (decl) {
      FullSourceLoc location = result.Context->getFullLoc(decl->getBeginLoc());
      if (location.isInvalid()) {
        return;
      }
      int line_number =
          misra::libtooling_utils::GetLine(decl, result.SourceManager);
      if (line_number == last_decl_line_) {
        if (!has_reported_in_last_decl_line_) {
          ReportError(last_decl_path_, last_decl_line_, results_list_);
          has_reported_in_last_decl_line_ = true;
        }
      } else {
        last_decl_line_ = line_number;
        last_decl_path_ =
            misra::libtooling_utils::GetFilename(decl, result.SourceManager);
        has_reported_in_last_decl_line_ = false;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  int last_decl_line_;
  std::string last_decl_path_;
  bool has_reported_in_last_decl_line_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A7_1_7
}  // namespace autosar
