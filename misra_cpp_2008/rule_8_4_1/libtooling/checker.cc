/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_8_4_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "不应在函数定义中使用省略号";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_8_4_1 {
namespace libtooling {

void FuncDefEllipsisCallback::Init(ResultsList* results_list,
                                   MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(functionDecl(isDefinition()).bind("d"), this);
}

void FuncDefEllipsisCallback::run(const MatchFinder::MatchResult& result) {
  ASTContext* context = result.Context;

  const FunctionDecl* function_match =
      result.Nodes.getNodeAs<FunctionDecl>("d");

  if (function_match->getEllipsisLoc().isValid()) {
    std::string path = misra::libtooling_utils::GetFilename(
        function_match, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(function_match, result.SourceManager);
    ReportError(path, line_number, results_list_);
  }
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new FuncDefEllipsisCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_8_4_1
}  // namespace misra_cpp_2008
