/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_18_2_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "不应使用宏 offsetof";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_18_2_1 {
namespace libtooling {

const internal::VariadicDynCastAllOfMatcher<Stmt, OffsetOfExpr> offsetOfExpr;

class SpecificFunctionUsageCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(offsetOfExpr().bind("offsetof_expr"), this);
  }
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const OffsetOfExpr* offsetof_expr =
        result.Nodes.getNodeAs<OffsetOfExpr>("offsetof_expr");
    ReportError(
        misra::libtooling_utils::GetFilename(offsetof_expr,
                                             result.SourceManager),
        misra::libtooling_utils::GetLine(offsetof_expr, result.SourceManager),
        results_list_);

    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new SpecificFunctionUsageCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_18_2_1
}  // namespace misra_cpp_2008
