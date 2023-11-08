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

#include "autosar/rule_A11_0_2/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
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
      "A type defined as struct shall: (1) provide only public data members, (2) not provide any special member functions or methods, (3) not be a base of another struct or class, (4) not inherit from another struct or class.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A11_0_2 {
namespace libtooling {

auto isPrivateOrProtected(StringRef spec) -> bool {
  return spec.contains("private") || spec.contains("protected");
};

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        cxxRecordDecl(hasDefinition(), unless(isExpansionInSystemHeader()),
                      forEach(accessSpecDecl().bind("spec")))
            .bind("decl"),
        this);

    finder->addMatcher(
        cxxRecordDecl(hasDefinition(), unless(isExpansionInSystemHeader()),
                      unless(has(accessSpecDecl())))
            .bind("decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXRecordDecl* decl = result.Nodes.getNodeAs<CXXRecordDecl>("decl");
    const AccessSpecDecl* spec = result.Nodes.getNodeAs<AccessSpecDecl>("spec");

    // check list - 1:
    // if a base is a struct, report error
    for (auto base : decl->bases()) {
      CXXRecordDecl* rd = base.getType()->getAsCXXRecordDecl();
      if (!rd) {
        continue;
      }
      if (rd->isStruct()) {
        std::string path =
            misra::libtooling_utils::GetFilename(rd, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(rd, result.SourceManager);
        ReportError(path, line_number, results_list_);
      }
    }

    if (decl->isStruct()) {
      // check list - 2:
      // only public data members and functions.
      if (spec) {
        clang::SourceRange range = SourceRange(
            result.SourceManager->getSpellingLoc(spec->getBeginLoc()),
            result.SourceManager->getSpellingLoc(spec->getEndLoc()));
        auto char_range = Lexer::makeFileCharRange(
            CharSourceRange::getTokenRange(range), *result.SourceManager,
            result.Context->getLangOpts());
        auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                           result.Context->getLangOpts());

        if (isPrivateOrProtected(source)) {
          std::string path =
              misra::libtooling_utils::GetFilename(decl, result.SourceManager);
          int line_number =
              misra::libtooling_utils::GetLine(decl, result.SourceManager);
          ReportError(path, line_number, results_list_);
          return;
        }
      }

      // check list - 3:
      // no bases;
      // is POD: no virtual, no user-declared special members functions
      // (constructor, destructor, copy operation,
      // move operation), etc.
      if (decl->getNumBases() || !decl->isPOD()) {
        std::string path =
            misra::libtooling_utils::GetFilename(decl, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(decl, result.SourceManager);
        ReportError(path, line_number, results_list_);
        return;
      }
    }
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
}  // namespace rule_A11_0_2
}  // namespace autosar
