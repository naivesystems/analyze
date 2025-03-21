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

#include "misra_cpp_2008/rule_15_3_3/libtooling/checker.h"

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
namespace rule_15_3_3 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxConstructorDecl(has(cxxTryStmt(hasDescendant(
            memberExpr(hasDescendant(cxxThisExpr())).bind("member"))))),
        this);
    finder->addMatcher(
        cxxDestructorDecl(has(cxxTryStmt((hasDescendant(
            memberExpr(hasDescendant(cxxThisExpr())).bind("member")))))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const MemberExpr* member = result.Nodes.getNodeAs<MemberExpr>("member");
    if (misra::libtooling_utils::IsInSystemHeader(member, result.Context)) {
      return;
    }
    string error_message =
        "类构造函数或析构函数的“函数 try block”实现的处理程序, 不应引用此类或其基类的非静态成员";
    string path =
        misra::libtooling_utils::GetFilename(member, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(member, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_15_3_3);
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
}  // namespace rule_15_3_3
}  // namespace misra_cpp_2008
