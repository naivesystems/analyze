/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_6_5_1/libtooling/checker.h"

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
  std::string error_message = absl::StrFormat(
      "for循环中必须有且仅有一个循环计数器，该计数器不得为浮点类型");
  analyzer::proto::Result* pb_result = AddResultToResultsList(
      results_list, filepath, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_6_5_1);
  pb_result->set_filename(filepath);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_6_5_1 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    auto loop_counter_matcher =
        CreateLoopCounterMatcher(AllCondFormat, AllIncFormat);

    finder->addMatcher(
        forStmt(loop_counter_matcher).bind("for_stmt_with_counter"), this);
    finder->addMatcher(
        forStmt(unless(loop_counter_matcher)).bind("for_stmt_without_counter"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const ForStmt* for_stmt_without_counter =
        result.Nodes.getNodeAs<ForStmt>("for_stmt_without_counter");
    const ForStmt* for_stmt_with_counter =
        result.Nodes.getNodeAs<ForStmt>("for_stmt_with_counter");
    const VarDecl* loop_counter =
        result.Nodes.getNodeAs<VarDecl>("loop_counter");

    // if there is no loop-counter in the `ForStmt`, the `ForStmt` is invalid
    if (for_stmt_without_counter &&
        !misra::libtooling_utils::IsInSystemHeader(for_stmt_without_counter,
                                                   result.Context)) {
      string path = misra::libtooling_utils::GetFilename(
          for_stmt_without_counter, result.SourceManager);
      int line = misra::libtooling_utils::GetLine(for_stmt_without_counter,
                                                  result.SourceManager);
      ReportError(path, line, results_list_);
    } else if (for_stmt_with_counter &&
               !misra::libtooling_utils::IsInSystemHeader(for_stmt_with_counter,
                                                          result.Context)) {
      // if the loop-counter exists and the current `ForStmt` is valid,
      // justify whether it will be invalid thanks to the new loop-counter
      if (loop_counter && var_set[for_stmt_with_counter].size() <= 1) {
        // add the current loop-counter to the map of the certain `ForStmt`
        var_set[for_stmt_with_counter].insert(loop_counter);
        // if there is more than one loop-counter in the `ForStmt` or
        // the loop-counter is float type (bad3),
        // the `ForStmt` is invalid
        if (var_set[for_stmt_with_counter].size() > 1 ||
            loop_counter->getType()->isRealFloatingType()) {
          string path = misra::libtooling_utils::GetFilename(
              for_stmt_with_counter, result.SourceManager);
          int line = misra::libtooling_utils::GetLine(for_stmt_with_counter,
                                                      result.SourceManager);
          ReportError(path, line, results_list_);
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<const clang::ForStmt*,
                     std::unordered_set<const clang::VarDecl*> >
      var_set;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_6_5_1
}  // namespace misra_cpp_2008
