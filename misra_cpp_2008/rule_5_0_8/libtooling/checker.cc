/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_5_0_8/libtooling/checker.h"

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

void ReportError507(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-5.0.7] c值表达式不得有显式的浮点-整型转换";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

void ReportError508(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-5.0.8] 显式的整型或浮点转换不得使c值表达式的底层类型变大";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

void ReportError509(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-5.0.9] 显式的整型转换不得改变c值表达式的底层类型的符号性";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_0_8 {
namespace libtooling {

// Matches QualType nodes that are of floating type or integer type.
AST_MATCHER(QualType, isIntegerOrFloat) {
  return Node->isIntegerType() || Node->isFloatingType();
}

// Matches explict cast on cvalues. The passed matcher determines what the
// source cvalue expression looks like.
template <typename T>
auto explicitCastForCValue(const T& matcher) {
  return (
      explicitCastExpr(
          hasSourceExpression(
              (expr(hasType(qualType(isIntegerOrFloat())), matcher)
                   .bind("cvalue"))),
          hasDestinationType(qualType(isIntegerOrFloat()).bind("dest_type")))
          .bind("cast_expr"));
}

void CheckCvalueCallback::Init(std::string rule_name,
                               analyzer::proto::ResultsList* results_list,
                               ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;
  check_name = rule_name;

  auto cvalue_range =
      anyOf(binaryOperator(), unaryOperator(), conditionalOperator());

  auto parent_restriction =
      unless(anyOf(binaryOperator(unless(hasOperatorName("="))),
                   unaryOperator(), callExpr(), returnStmt()));

  // Matches the common cases of explicit cast on cvalues, which means the
  // cvalue expression is not return value or function argument. In this case
  // the cvalue expression is an operation (matched by 'cvalue_range')
  // and it is not a part of larger operation (matched by 'parent_restriction').
  auto common_matcher =
      expr(forEach(explicitCastForCValue(cvalue_range)), parent_restriction);

  // Matches the explicit cast on return values. Return values are always cvalue
  // expressions.
  auto return_matcher =
      returnStmt(hasReturnValue(explicitCastForCValue(anything())));

  // Matches the explicit cast on function arguments. Function arguments are
  // always cvalue expressions.
  auto parm_matcher = callExpr(forEach(explicitCastForCValue(anything())));

  // TK_IgnoreUnlessSpelledInSource is used for stripping implicit casts and
  // unnecessary parenExpr.
  finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource, common_matcher),
                     this);
  finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource, return_matcher),
                     this);
  finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource, parm_matcher),
                     this);
}

void CheckCvalueCallback::run(
    const ast_matchers::MatchFinder::MatchResult& result) {
  const ASTContext& ctx = *result.Context;
  const ExplicitCastExpr* cast_expr_ =
      result.Nodes.getNodeAs<ExplicitCastExpr>("cast_expr");
  const Expr* expr_ = result.Nodes.getNodeAs<Expr>("cvalue");
  const QualType* dest_type_ = result.Nodes.getNodeAs<QualType>("dest_type");

  if (misra::libtooling_utils::IsInSystemHeader(cast_expr_, result.Context)) {
    return;
  }

  QualType expr_type_;
  misra::libtooling_utils::GetUnderlyingTypeOfExpr(expr_, result.Context,
                                                   expr_type_);

  if (check_name == "rule-5-0-7" && check507(*dest_type_, expr_type_)) {
    ReportError507(
        misra::libtooling_utils::GetFilename(cast_expr_, result.SourceManager),
        misra::libtooling_utils::GetLine(cast_expr_, result.SourceManager),
        results_list_);
    return;
  }

  if (check_name == "rule-5-0-8" &&
      check508(*dest_type_, expr_type_, result.Context)) {
    ReportError508(
        misra::libtooling_utils::GetFilename(cast_expr_, result.SourceManager),
        misra::libtooling_utils::GetLine(cast_expr_, result.SourceManager),
        results_list_);
    return;
  }

  if (check_name == "rule-5-0-9" && check509(*dest_type_, expr_type_)) {
    ReportError509(
        misra::libtooling_utils::GetFilename(cast_expr_, result.SourceManager),
        misra::libtooling_utils::GetLine(cast_expr_, result.SourceManager),
        results_list_);
  }
  return;
}

bool CheckCvalueCallback::check507(const clang::QualType dest_type,
                                   const clang::QualType underlying_type) {
  if ((dest_type->isIntegerType() && underlying_type->isFloatingType()) ||
      (dest_type->isFloatingType() && underlying_type->isIntegerType())) {
    return true;
  }
  return false;
}

bool CheckCvalueCallback::check508(const clang::QualType dest_type,
                                   const clang::QualType underlying_type,
                                   clang::ASTContext* context) {
  if (context->getTypeInfo(dest_type.getTypePtr()).Width >
      context->getTypeInfo(underlying_type.getTypePtr()).Width) {
    return true;
  }
  return false;
}

bool CheckCvalueCallback::check509(const clang::QualType dest_type,
                                   const clang::QualType underlying_type) {
  if ((dest_type->isSignedIntegerType() &&
       underlying_type->isUnsignedIntegerType()) ||
      (dest_type->isUnsignedIntegerType() &&
       underlying_type->isSignedIntegerType())) {
    return true;
  }
  return false;
}

void Checker::Init(std::string rule_name,
                   analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckCvalueCallback;
  record_callback_->Init(rule_name, result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_0_8
}  // namespace misra_cpp_2008
