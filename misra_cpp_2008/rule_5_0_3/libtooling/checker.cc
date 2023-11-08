/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_5_0_3/libtooling/checker.h"

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

void ReportError503(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-5.0.3] 一个c值表达式不应隐式转换为不同的底层类型";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_0_3 {
namespace libtooling {

void CheckCvalueImplicitCallback::Init(
    analyzer::proto::ResultsList* results_list,
    clang::ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;

  auto cvalue_range =
      anyOf(binaryOperator(), unaryOperator(), conditionalOperator());

  auto parent_restriction =
      unless(anyOf(binaryOperator(unless(hasOperatorName("="))),
                   unaryOperator(), callExpr(), returnStmt()));

  auto implicitCastForCValueForReturnAndParam =
      implicitCastExpr(hasSourceExpression(expr().bind("cvalue")),
                       hasImplicitDestinationType(qualType().bind("dest_type")))
          .bind("impl_cast");

  // Match a full cvalue expression, check if it has been implicit casted to a
  // different underlying type of parent node

  // Match the first cvalue child of a expr. The first cvalue expr will contain
  // the type of the whole cvalue expression
  // For example: hasDescendant() will match "+", and it will contain the
  // underlying type of the whole right AST tree
  //              =
  //       s32          +
  //                s32   s8
  //
  auto common_matcher =
      expr(hasDescendant(expr(cvalue_range).bind("cvalue")),
           hasDescendant(implicitCastExpr().bind("impl_cast")),
           parent_restriction)
          .bind("parent");

  // Matches the implicit cast on return values. Return values are always
  // cvalue expressions.
  auto return_matcher =
      returnStmt(hasReturnValue(implicitCastForCValueForReturnAndParam));

  // // Matches the implicit cast on function arguments. Function arguments are
  // // always cvalue expressions.
  auto parm_matcher =
      implicitCastExpr(unless(hasCastKind(CK_FunctionToPointerDecay)),
                       hasAncestor(callExpr()),
                       hasSourceExpression(expr().bind("cvalue")),
                       hasImplicitDestinationType(qualType().bind("dest_type")))
          .bind("impl_cast");

  finder->addMatcher(common_matcher, this);
  finder->addMatcher(return_matcher, this);
  finder->addMatcher(parm_matcher, this);
}

void CheckCvalueImplicitCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult& result) {
  const ASTContext& ctx = *result.Context;
  const ImplicitCastExpr* cast_expr =
      result.Nodes.getNodeAs<ImplicitCastExpr>("impl_cast");

  if (misra::libtooling_utils::IsInSystemHeader(cast_expr, result.Context)) {
    return;
  }
  if (cast_expr->isPartOfExplicitCast()) {
    return;
  }
  const Expr* cvalue_expr = result.Nodes.getNodeAs<Expr>("cvalue");

  if (cvalue_expr == NULL) return;
  QualType cvalue_underlying_type;
  misra::libtooling_utils::GetUnderlyingTypeOfExpr(cvalue_expr, result.Context,
                                                   cvalue_underlying_type);
  const auto* parent = result.Nodes.getNodeAs<Expr>("parent");
  QualType dest_type;
  if (parent) {
    dest_type = parent->getType();
  } else {
    dest_type = *result.Nodes.getNodeAs<QualType>("dest_type");
  }
  if (dest_type.isNull() || cvalue_underlying_type.isNull()) return;
  if (check503(dest_type, cvalue_underlying_type)) {
    ReportError503(
        misra::libtooling_utils::GetFilename(cast_expr, result.SourceManager),
        misra::libtooling_utils::GetLine(cast_expr, result.SourceManager),
        results_list_);
  }
}

bool CheckCvalueImplicitCallback::check503(
    const clang::QualType dest_type, const clang::QualType underlying_type) {
  if (dest_type.getCanonicalType() == underlying_type.getCanonicalType()) {
    return false;
  }
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  implicit_callback_ = new CheckCvalueImplicitCallback;
  implicit_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_0_3
}  // namespace misra_cpp_2008
