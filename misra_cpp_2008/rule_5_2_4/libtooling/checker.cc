/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_5_2_4/libtooling/checker.h"

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
  string error_message =
      "[misra_cpp_2008-5.2.4]: 不得使用C风格的转换（除了void转换）和函数式记法转换（除了显式构造函数调用）";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_2_4 {
namespace libtooling {

AST_MATCHER(QualType, isVoid) { return Node->isVoidType(); }

void CheckCastCallback::Init(analyzer::proto::ResultsList* results_list,
                             ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      explicitCastExpr(
          anyOf(
              cxxFunctionalCastExpr(unless(hasDescendant(cxxConstructExpr()))),
              cStyleCastExpr(unless(hasDestinationType(qualType(isVoid()))))))
          .bind("cast"),
      this);
}

void CheckCastCallback::run(
    const ast_matchers::MatchFinder::MatchResult& result) {
  const ExplicitCastExpr* cast_ =
      result.Nodes.getNodeAs<ExplicitCastExpr>("cast");

  if (misra::libtooling_utils::IsInSystemHeader(cast_, result.Context)) {
    return;
  }

  ReportError(misra::libtooling_utils::GetFilename(cast_, result.SourceManager),
              misra::libtooling_utils::GetLine(cast_, result.SourceManager),
              results_list_);
  return;
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckCastCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_2_4
}  // namespace misra_cpp_2008
