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

#include "misra_cpp_2008/rule_6_5_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using namespace misra::libtooling_utils;

namespace misra_cpp_2008 {
namespace rule_6_5_3 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    auto loop_counter_matcher =
        CreateLoopCounterMatcher(AllCondFormat, AllIncFormat);
    auto assign_matcher = CreateAssignmentMatcher(AllIncFormat);

    finder->addMatcher(
        forStmt(loop_counter_matcher,
                anyOf(hasBody(assign_matcher), hasCondition(assign_matcher)))
            .bind("for_stmt"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const ForStmt* for_stmt = result.Nodes.getNodeAs<ForStmt>("for_stmt");
    if (misra::libtooling_utils::IsInSystemHeader(for_stmt, result.Context)) {
      return;
    }
    string error_message = "不得在条件或语句中修改循环计数器";
    string path =
        misra::libtooling_utils::GetFilename(for_stmt, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(for_stmt, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_6_5_3);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_6_5_3
}  // namespace misra_cpp_2008
