/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_3_1_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "[misra_cpp_2008-3.1.2]: 不得在块作用域内声明函数";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_1_2 {
namespace libtooling {

void CheckFuncDeclCallback::Init(analyzer::proto::ResultsList* results_list,
                                 ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      functionDecl(hasParent(declStmt(hasParent(compoundStmt())))).bind("func"),
      this);
}

void CheckFuncDeclCallback::run(
    const ast_matchers::MatchFinder::MatchResult& result) {
  const FunctionDecl* func_ = result.Nodes.getNodeAs<FunctionDecl>("func");

  if (misra::libtooling_utils::IsInSystemHeader(func_, result.Context)) {
    return;
  }

  ReportError(misra::libtooling_utils::GetFilename(func_, result.SourceManager),
              misra::libtooling_utils::GetLine(func_, result.SourceManager),
              results_list_);
  return;
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckFuncDeclCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_3_1_2
}  // namespace misra_cpp_2008
