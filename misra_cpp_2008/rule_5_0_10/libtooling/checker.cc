/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_5_0_10/libtooling/checker.h"

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

namespace {
void ReportError(ResultsList* results_list, string path, int line_number) {
  string error_message =
      "如果按位操作符~和<<应用于底层类型为无符号char或无符号short的操作数，其结果应立即转换为该操作数的底层类型";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_0_10);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_0_10 {
namespace libtooling {

class OpCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperator(
            hasOperatorName("<<"),
            hasLHS(hasDescendant(declRefExpr(
                anyOf(hasType(hasCanonicalType(asString("unsigned short"))),
                      hasType(hasCanonicalType(asString("unsigned char"))))))),
            unless(anyOf(hasAncestor(cxxStaticCastExpr()),
                         hasAncestor(returnStmt()), hasAncestor(callExpr()))))
            .bind("bo"),
        this);
    finder->addMatcher(
        unaryOperator(
            hasOperatorName("~"),
            hasDescendant(declRefExpr(
                anyOf(hasType(hasCanonicalType(asString("unsigned short"))),
                      hasType(hasCanonicalType(asString("unsigned char")))))),
            unless(anyOf(hasAncestor(cxxStaticCastExpr()),
                         hasAncestor(returnStmt()), hasAncestor(callExpr()))))
            .bind("uo"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const BinaryOperator* bo = result.Nodes.getNodeAs<BinaryOperator>("bo");
    const UnaryOperator* uo = result.Nodes.getNodeAs<UnaryOperator>("uo");
    if (bo) {
      ReportError(
          results_list_,
          misra::libtooling_utils::GetFilename(bo, result.SourceManager),
          misra::libtooling_utils::GetLine(bo, result.SourceManager));
      return;
    }
    if (uo) {
      ReportError(
          results_list_,
          misra::libtooling_utils::GetFilename(uo, result.SourceManager),
          misra::libtooling_utils::GetLine(uo, result.SourceManager));
      return;
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new OpCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_0_10
}  // namespace misra_cpp_2008
