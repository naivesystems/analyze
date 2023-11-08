/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_5_3_2/libtooling/checker.h"

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
namespace rule_5_3_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto utype = hasType(isUnsignedInteger());
    finder->addMatcher(
        unaryOperator(
            hasOperatorName("-"),
            anyOf(hasUnaryOperand(expr(utype).bind("u")),
                  hasUnaryOperand(
                      implicitCastExpr(hasSourceExpression(utype)).bind("u")))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* u = result.Nodes.getNodeAs<Expr>("u");
    string error_message = "-运算符不得用于底层类型为无符号的表达式";
    string path = misra::libtooling_utils::GetFilename(u, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(u, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_3_2);
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
}  // namespace rule_5_3_2
}  // namespace misra_cpp_2008
