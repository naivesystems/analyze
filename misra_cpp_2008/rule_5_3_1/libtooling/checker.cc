/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_5_3_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace misra_cpp_2008 {
namespace rule_5_3_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto inner_expr =
        implicitCastExpr(hasSourceExpression(unless(hasType(booleanType()))))
            .bind("e");
    finder->addMatcher(
        unaryOperator(hasOperatorName("!"), hasUnaryOperand(inner_expr)), this);
    finder->addMatcher(cxxOperatorCallExpr(hasAnyOperatorName("!"),
                                           hasUnaryOperand(inner_expr)),
                       this);
    finder->addMatcher(binaryOperator(hasAnyOperatorName("||", "&&"),
                                      hasEitherOperand(inner_expr)),
                       this);
    finder->addMatcher(cxxOperatorCallExpr(hasAnyOperatorName("||", "&&"),
                                           hasEitherOperand(inner_expr)),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* e = result.Nodes.getNodeAs<Expr>("e");
    string error_message = "!, &&, || 的操作数应当为 bool 类型";
    string path = misra::libtooling_utils::GetFilename(e, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(e, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_3_1);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_3_1
}  // namespace misra_cpp_2008
