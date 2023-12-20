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

#include "misra_cpp_2008/rule_2_13_4/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;

namespace misra_cpp_2008 {
namespace rule_2_13_4 {
namespace libtooling {

class IntCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(integerLiteral().bind("lit"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* lit = result.Nodes.getNodeAs<Expr>("lit");
    if (misra::libtooling_utils::IsInSystemHeader(lit, result.Context)) {
      return;
    }
    auto char_range = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(lit->getSourceRange()),
        *result.SourceManager, result.Context->getLangOpts());
    auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                       result.Context->getLangOpts())
                      .str();
    for (int i = source.length() - 1; i >= 0; i--) {
      if ((source[i] == 'u') || (source[i] == 'l') || (source[i] == 'z')) {
        string error_message = "字面量后缀必须是大写字母";
        analyzer::proto::Result* pb_result = AddResultToResultsList(
            results_list_,
            misra::libtooling_utils::GetFilename(lit, result.SourceManager),
            misra::libtooling_utils::GetLine(lit, result.SourceManager),
            error_message);
        pb_result->set_error_kind(
            analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_2_13_4);
        return;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class FloatCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(floatLiteral().bind("lit"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* lit = result.Nodes.getNodeAs<Expr>("lit");
    if (misra::libtooling_utils::IsInSystemHeader(lit, result.Context)) {
      return;
    }
    auto char_range = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(lit->getSourceRange()),
        *result.SourceManager, result.Context->getLangOpts());
    auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                       result.Context->getLangOpts())
                      .str();
    for (int i = source.length() - 1; i >= 0; i--) {
      if ((source[i] == 'l') || (source[i] == 'f')) {
        string error_message = "字面量后缀必须是大写字母";
        analyzer::proto::Result* pb_result = AddResultToResultsList(
            results_list_,
            misra::libtooling_utils::GetFilename(lit, result.SourceManager),
            misra::libtooling_utils::GetLine(lit, result.SourceManager),
            error_message);
        pb_result->set_error_kind(
            analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_2_13_4);
        return;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  icallback_ = new IntCallback;
  icallback_->Init(results_list_, &finder_);
  fcallback_ = new FloatCallback;
  fcallback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_2_13_4
}  // namespace misra_cpp_2008
