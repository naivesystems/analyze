/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
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

class IntCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(integerLiteral().bind("lit"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
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
        std::string error_message = "字面量后缀必须是大写字母";
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

class FloatCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(floatLiteral().bind("lit"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
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
        std::string error_message = "字面量后缀必须是大写字母";
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
