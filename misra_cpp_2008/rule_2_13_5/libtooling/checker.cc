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

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(stringLiteral().bind("lit"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const StringLiteral* lit = result.Nodes.getNodeAs<StringLiteral>("lit");
    unsigned int tok_num = lit->getNumConcatenated();
    bool first_is_wide =
        SourceIsWide(lit->getStrTokenLoc(0), *result.SourceManager,
                     result.Context->getLangOpts(), result.Context);
    for (int i = 1; i < tok_num; i++) {
      if (first_is_wide !=
          SourceIsWide(lit->getStrTokenLoc(i), *result.SourceManager,
                       result.Context->getLangOpts(), result.Context)) {
        string error_message = "不得将宽字符串字面量和窄字符串字面量串接";
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
