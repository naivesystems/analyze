/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_7_5_3/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "如果一个形参是通过引用或const引用传递的，那么函数不得返回该形参的引用或指针";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_5_3);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_5_3 {
namespace libtooling {

class ReturnCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl(forEachDescendant(
                           returnStmt(hasDescendant(declRefExpr(to(parmVarDecl(
                                          hasType(referenceType()))))))
                               .bind("return"))),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const ReturnStmt* return_ = result.Nodes.getNodeAs<ReturnStmt>("return");
    std::string path_ =
        misra::libtooling_utils::GetFilename(return_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(return_, result.SourceManager);
    if (!misra::libtooling_utils::IsInSystemHeader(return_, result.Context)) {
      ReportError(path_, line_number_, results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  returnCallback_ = new ReturnCallback;
  returnCallback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_7_5_3
}  // namespace misra_cpp_2008
