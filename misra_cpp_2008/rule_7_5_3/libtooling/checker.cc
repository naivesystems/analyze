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

#include "misra_cpp_2008/rule_7_5_3/libtooling/checker.h"

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
      "如果一个形参是通过引用或const引用传递的，那么函数不得返回该形参的引用或指针";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_5_3);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_5_3 {
namespace libtooling {

class ReturnCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl(forEachDescendant(
                           returnStmt(hasDescendant(declRefExpr(to(parmVarDecl(
                                          hasType(referenceType()))))))
                               .bind("return"))),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const ReturnStmt* return_ = result.Nodes.getNodeAs<ReturnStmt>("return");
    string path_ =
        misra::libtooling_utils::GetFilename(return_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(return_, result.SourceManager);
    if (!misra::libtooling_utils::IsInSystemHeader(return_, result.Context)) {
      ReportError(path_, line_number_, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  returnCallback_ = new ReturnCallback;
  returnCallback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_7_5_3
}  // namespace misra_cpp_2008
