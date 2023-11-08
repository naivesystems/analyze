/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
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

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(characterLiteral().bind("lit"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
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
    std::string error_message =
        "不得使用八进制常量（除零以外）和八进制转义序列（除“\\0”以外）";
    AddResultToResultsList(
        results_list_,
        misra::libtooling_utils::GetFilename(lit, result.SourceManager),
        misra::libtooling_utils::GetLine(lit, result.SourceManager),
        error_message);
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
}  // namespace rule_2_13_2
}  // namespace misra_cpp_2008
