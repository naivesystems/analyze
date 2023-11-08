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

#include "misra_cpp_2008/rule_8_3_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {
void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "覆盖虚拟函数中的形参必须使用与被它们覆盖的函数相同的缺省实参，否则不应指定任何缺省实参";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_8_3_1);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_8_3_1 {
namespace libtooling {
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxMethodDecl(isVirtual(), isOverride(),
                                     hasAnyParameter(hasDefaultArgument()))
                           .bind("method"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* cur_method =
        result.Nodes.getNodeAs<CXXMethodDecl>("method");

    const CXXMethodDecl* base_method =
        (*cur_method->begin_overridden_methods());

    ArrayRef<ParmVarDecl*> cur_param_arr = cur_method->parameters();
    ArrayRef<ParmVarDecl*> base_param_arr = base_method->parameters();

    for (int i = 0; i < cur_param_arr.size(); i++) {
      if (!cur_param_arr[i]->hasDefaultArg()) {
        continue;
      }
      auto cur_char_range = Lexer::makeFileCharRange(
          CharSourceRange::getTokenRange(cur_param_arr[i]->getSourceRange()),
          *result.SourceManager, result.Context->getLangOpts());
      auto base_char_range = Lexer::makeFileCharRange(
          CharSourceRange::getTokenRange(base_param_arr[i]->getSourceRange()),
          *result.SourceManager, result.Context->getLangOpts());
      std::string cur_str =
          Lexer::getSourceText(cur_char_range, *result.SourceManager,
                               result.Context->getLangOpts())
              .str();
      std::string base_str =
          Lexer::getSourceText(base_char_range, *result.SourceManager,
                               result.Context->getLangOpts())
              .str();
      if (cur_str != base_str) {
        string path = misra::libtooling_utils::GetFilename(
            cur_method, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(cur_method, result.SourceManager);
        ReportError(path, line_number, results_list_);
        return;
      }
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_8_3_1
}  // namespace misra_cpp_2008
