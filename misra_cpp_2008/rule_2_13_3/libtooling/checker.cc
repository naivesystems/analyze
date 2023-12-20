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

#include "misra_cpp_2008/rule_2_13_3/libtooling/checker.h"

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
namespace rule_2_13_3 {
namespace libtooling {

auto isHexadecimal(StringRef num) -> bool {
  return num.startswith_insensitive("0x");
};

auto isOctal(StringRef num) -> bool {
  for (auto it : num) {
    if (!('0' <= it && it <= '7')) {
      return false;
    }
  }
  return num.startswith("0");
};

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(integerLiteral(hasType(isUnsignedInteger())).bind("lit"),
                       this);
    finder->addMatcher(
        binaryOperation(hasEitherOperand(implicitCastExpr(
                            hasSourceExpression(hasType(isUnsignedInteger())))),
                        hasEitherOperand(integerLiteral().bind("lit"))),
        this);  // pretend to use integerLiteral as unsigned

    finder->addMatcher(
        binaryOperation(hasEitherOperand(implicitCastExpr(
                            hasSourceExpression(hasType(isUnsignedInteger())))),
                        hasEitherOperand(implicitCastExpr(hasSourceExpression(
                            integerLiteral().bind("lit"))))),
        this);  // pretend to use integerLiteral as unsigned, u16 = u16 * <s16>

    finder->addMatcher(
        binaryOperation(hasEitherOperand(hasType(isUnsignedInteger())),
                        hasEitherOperand(integerLiteral().bind("lit"))),
        this);  // pretend to use integerLiteral as unsigned
  }

  void run(const MatchFinder::MatchResult& result) {
    const IntegerLiteral* lit = result.Nodes.getNodeAs<IntegerLiteral>("lit");
    clang::SourceRange range =
        SourceRange(result.SourceManager->getSpellingLoc(lit->getBeginLoc()),
                    result.SourceManager->getSpellingLoc(lit->getEndLoc()));
    auto char_range = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(range), *result.SourceManager,
        result.Context->getLangOpts());
    auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                       result.Context->getLangOpts());

    if (source == "") return;
    // We only check octal and hexadecimal integers.
    if (!isHexadecimal(source) && !isOctal(source)) return;

    if (source.endswith("U")) {
      return;  // marked with U
    }
    string error_message =
        "必须对所有八进制或十六进制的无符号整型字面量使用后缀“U”";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_,
        misra::libtooling_utils::GetFilename(lit, result.SourceManager),
        misra::libtooling_utils::GetLine(lit, result.SourceManager),
        error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_2_13_3);
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
}  // namespace rule_2_13_3
}  // namespace misra_cpp_2008
