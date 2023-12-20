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

#include "misra_cpp_2008/rule_2_13_2/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace misra_cpp_2008 {
namespace rule_2_13_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(characterLiteral().bind("lit"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const CharacterLiteral* lit =
        result.Nodes.getNodeAs<CharacterLiteral>("lit");
    if (lit->getValue() == 0) {
      return;  // '\0' or not an octal
    }
    auto char_range = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(lit->getSourceRange()),
        *result.SourceManager, result.Context->getLangOpts());
    auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                       result.Context->getLangOpts())
                      .str();
    if (source.rfind("'\\", 0) != 0) {
      return;  // not an octal
    }
    if (source.length() < 3 || !std::isdigit(source[2])) {
      return;  // not an octal
    }
    string error_message =
        "不得使用八进制常量（除零以外）和八进制转义序列（除“\\0”以外）";
    AddResultToResultsList(
        results_list_,
        misra::libtooling_utils::GetFilename(lit, result.SourceManager),
        misra::libtooling_utils::GetLine(lit, result.SourceManager),
        error_message);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_2_13_2
}  // namespace misra_cpp_2008
