/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_2_13_5/libtooling/checker.h"

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
namespace rule_2_13_5 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(stringLiteral().bind("lit"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const StringLiteral* lit = result.Nodes.getNodeAs<StringLiteral>("lit");
    unsigned int tok_num = lit->getNumConcatenated();
    bool first_is_wide =
        SourceIsWide(lit->getStrTokenLoc(0), *result.SourceManager,
                     result.Context->getLangOpts(), result.Context);
    for (int i = 1; i < tok_num; i++) {
      if (first_is_wide !=
          SourceIsWide(lit->getStrTokenLoc(i), *result.SourceManager,
                       result.Context->getLangOpts(), result.Context)) {
        std::string error_message = "不得将宽字符串字面量和窄字符串字面量串接";
        analyzer::proto::Result* pb_result = AddResultToResultsList(
            results_list_,
            misra::libtooling_utils::GetFilename(lit, result.SourceManager),
            misra::libtooling_utils::GetLine(lit, result.SourceManager),
            error_message);
        pb_result->set_error_kind(
            analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_2_13_5);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  bool SourceIsWide(SourceLocation loc, const SourceManager& sm,
                    const LangOptions& langopts,
                    const clang::ASTContext* context) {
    auto char_range = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(SourceRange(loc)), sm, langopts);
    // If the source text location is in a system header, it is not in the scope
    // of this rule.
    FullSourceLoc location = context->getFullLoc(char_range.getBegin());
    if (location.isInvalid() || location.isInSystemHeader()) {
      return false;
    }
    string source = Lexer::getSourceText(char_range, sm, langopts).str();
    return source[0] == 'L' && source[1] == '"';
  }
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_2_13_5
}  // namespace misra_cpp_2008
