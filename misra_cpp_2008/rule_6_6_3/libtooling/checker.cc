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

#include "misra_cpp_2008/rule_6_6_3/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang::ast_matchers;
using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using analyzer::proto::ResultsList;
using namespace internal;

namespace {

void ReportError(std::string filepath, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      absl::StrFormat("continue语句只能在为良构（well-formed）的for循环中使用");
  analyzer::proto::Result* pb_result = AddResultToResultsList(
      results_list, filepath, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_6_6_3);
  pb_result->set_filename(filepath);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_6_6_3 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;

    // match loop counter (6.5.1)
    auto loop_counter_matcher =
        CreateLoopCounterMatcher(AllCondFormat, AllIncFormat);

    // match non-bool loop controller (6.5.6)
    auto invalid_controller_matcher = allOf(
        loop_counter_matcher,
        hasCondition(forEachDescendant(declRefExpr(
            unless(anyOf(hasType(booleanType()),
                         to(varDecl(equalsBoundNode("loop_counter")))))))));

    auto continue_matcher = hasBody(findAll(continueStmt().bind("continue")));

    // rule 6.5.1
    finder->addMatcher(forStmt(loop_counter_matcher, continue_matcher)
                           .bind("for_stmt_with_counter"),
                       this);
    finder->addMatcher(forStmt(unless(loop_counter_matcher), continue_matcher)
                           .bind("for_stmt_without_counter"),
                       this);
    // rule 6.5.6
    finder->addMatcher(forStmt(invalid_controller_matcher, continue_matcher)
                           .bind("for_stmt_with_invalid_controller"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const ForStmt* for_stmt_with_counter =
        result.Nodes.getNodeAs<ForStmt>("for_stmt_with_counter");
    const ForStmt* for_stmt_without_counter =
        result.Nodes.getNodeAs<ForStmt>("for_stmt_without_counter");
    const ForStmt* for_stmt_with_invalid_controller =
        result.Nodes.getNodeAs<ForStmt>("for_stmt_with_invalid_controller");
    const VarDecl* loop_counter =
        result.Nodes.getNodeAs<VarDecl>("loop_counter");
    const ContinueStmt* continue_stmt =
        result.Nodes.getNodeAs<ContinueStmt>("continue");

    if (IsInSystemHeader(continue_stmt, result.Context)) {
      return;
    }
    // avoid reporting error repeatedly
    if (continue_set.count(continue_stmt) > 0) {
      return;
    }
    // no loop-counter in the `ForStmt` || loop controller is invalid (non-bool)
    if (for_stmt_without_counter || for_stmt_with_invalid_controller) {
      continue_set.insert(continue_stmt);
      string path = GetFilename(continue_stmt, result.SourceManager);
      int line = GetLine(continue_stmt, result.SourceManager);
      ReportError(path, line, results_list_);
    } else if (for_stmt_with_counter) {
      // if the loop-counter exists and the current `ForStmt` is valid,
      // justify whether it will be invalid thanks to the new loop-counter
      if (loop_counter) {
        // add the current loop-counter to the map of the certain `ForStmt`
        loop_counter_set[for_stmt_with_counter].insert(loop_counter);
        // if there is more than one loop-counter in the `ForStmt` or
        // the loop-counter is float type,
        // the `ForStmt` is invalid
        if (loop_counter_set[for_stmt_with_counter].size() > 1 ||
            loop_counter->getType()->isRealFloatingType()) {
          continue_set.insert(continue_stmt);
          string path = GetFilename(continue_stmt, result.SourceManager);
          int line = GetLine(continue_stmt, result.SourceManager);
          ReportError(path, line, results_list_);
        }
      }
    }
  }

 private:
  ResultsList* results_list_;
  // the number of loop-counter declarations for each `ForStmt`
  std::unordered_map<const clang::ForStmt*,
                     std::unordered_set<const clang::VarDecl*> >
      loop_counter_set;
  // avoid reporting error repeatedly
  std::unordered_set<const clang::ContinueStmt*> continue_set;
};

void Checker::Init(ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_6_6_3
}  // namespace misra_cpp_2008
