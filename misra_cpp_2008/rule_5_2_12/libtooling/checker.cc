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

#include "misra_cpp_2008/rule_5_2_12/libtooling/checker.h"

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
namespace rule_5_2_12 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        callExpr(hasAnyArgument(
            castExpr(hasSourceExpression(hasType(arrayType())),
                     hasType(pointerType()), hasDescendant(declRefExpr()))
                .bind("decayed_arg"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* decayed_arg = result.Nodes.getNodeAs<Expr>("decayed_arg");
    if (misra::libtooling_utils::IsInSystemHeader(decayed_arg,
                                                  result.Context)) {
      return;
    }
    string error_message = "作为函数实参传递的数组类型标识符不应退化为指针";
    string path =
        misra::libtooling_utils::GetFilename(decayed_arg, result.SourceManager);
    int line =
        misra::libtooling_utils::GetLine(decayed_arg, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_2_12);
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
}  // namespace rule_5_2_12
}  // namespace misra_cpp_2008
