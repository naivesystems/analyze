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

#include "googlecpp/g1183/libtooling/checker.h"

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
      "Group similar declarations together, placing public parts earlier";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1183 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxRecordDecl(isClass(), unless(isExpansionInSystemHeader()))
            .bind("record_decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    auto* SM = result.SourceManager;
    const auto* record_decl = result.Nodes.getNodeAs<RecordDecl>("record_decl");
    unsigned int first_decl_line = 0;
    for (auto const decl : record_decl->decls()) {
      if (!dyn_cast<CXXRecordDecl>(decl) && !dyn_cast<AccessSpecDecl>(decl) &&
          !decl->isImplicit()) {
        first_decl_line = misra::libtooling_utils::GetLine(decl, SM);
        break;
      }
    }
    ClassInfo classInfo;
    classInfo.first_decl_line = first_decl_line;
    SM->getSpellingLineNumber(record_decl->getSourceRange().getBegin());
    for (auto const& decl : record_decl->decls()) {
      const auto& access_spec_decl = dyn_cast<AccessSpecDecl>(decl);
      if (!access_spec_decl) continue;
      const auto access_spec_decl_line =
          misra::libtooling_utils::GetLine(access_spec_decl, SM);
      switch (access_spec_decl->getAccess()) {
        case AS_public:
          if (classInfo.public_line != 0)
            ReportError(
                misra::libtooling_utils::GetFilename(access_spec_decl, SM),
                misra::libtooling_utils::GetLine(access_spec_decl, SM),
                results_list_);
          else
            classInfo.public_line = access_spec_decl_line;
          break;
        case AS_protected:
          if (classInfo.protected_line != 0)
            ReportError(
                misra::libtooling_utils::GetFilename(access_spec_decl, SM),
                misra::libtooling_utils::GetLine(access_spec_decl, SM),
                results_list_);
          else
            classInfo.protected_line = access_spec_decl_line;
          break;
        case AS_private:
          if (classInfo.private_line != 0)
            ReportError(
                misra::libtooling_utils::GetFilename(access_spec_decl, SM),
                misra::libtooling_utils::GetLine(access_spec_decl, SM),
                results_list_);
          else
            classInfo.private_line = access_spec_decl_line;
          break;
        default:
          break;
      }
      if ((classInfo.protected_line != 0 &&
           classInfo.protected_line < classInfo.public_line) ||
          (classInfo.private_line != 0 &&
           classInfo.private_line < classInfo.public_line)) {
        ReportError(misra::libtooling_utils::GetFilename(access_spec_decl, SM),
                    classInfo.public_line, results_list_);
      }
      if ((classInfo.private_line != 0 &&
           classInfo.private_line < classInfo.protected_line)) {
        ReportError(misra::libtooling_utils::GetFilename(access_spec_decl, SM),
                    classInfo.protected_line, results_list_);
      }
      if ((classInfo.public_line == 0 ||
           classInfo.first_decl_line < classInfo.public_line) &&
          (classInfo.protected_line == 0 ||
           classInfo.first_decl_line < classInfo.protected_line) &&
          (classInfo.private_line == 0 ||
           classInfo.first_decl_line < classInfo.private_line)) {
        ReportError(misra::libtooling_utils::GetFilename(access_spec_decl, SM),
                    classInfo.first_decl_line, results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  struct ClassInfo {
    unsigned int public_line = 0;
    unsigned int protected_line = 0;
    unsigned int private_line = 0;
    unsigned int first_decl_line = 0;
  };
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1183
}  // namespace googlecpp
