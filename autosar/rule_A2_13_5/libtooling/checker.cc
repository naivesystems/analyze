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

#include "autosar/rule_A2_13_5/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list_) {
  std::string error_message =
      absl::StrFormat("Hexadecimal constants should be upper case.");
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A2_13_5 {
namespace libtooling {

auto isHexadecimal(StringRef num) -> bool {
  return num.startswith_insensitive("0x");
};

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        integerLiteral(unless(isExpansionInSystemHeader())).bind("lit"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const Stmt* lit = result.Nodes.getNodeAs<Stmt>("lit");

    clang::SourceRange range =
        SourceRange(result.SourceManager->getSpellingLoc(lit->getBeginLoc()),
                    result.SourceManager->getSpellingLoc(lit->getEndLoc()));
    auto char_range = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(range), *result.SourceManager,
        result.Context->getLangOpts());
    auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                       result.Context->getLangOpts());

    if (!isHexadecimal(source)) return;

    const int start_loc = 2;  // length of "0x"
    for (int i = start_loc; i < source.size(); i++) {
      if (source[i] >= 'a' && source[i] <= 'f') {
        std::string path =
            misra::libtooling_utils::GetFilename(lit, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(lit, result.SourceManager);
        ReportError(path, line_number, results_list_);
        return;
      }
    }

    return;
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
}  // namespace rule_A2_13_5
}  // namespace autosar
