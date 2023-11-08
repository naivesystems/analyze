/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_12_8_1/libtooling/checker.h"

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
      "复制构造函数（copy constructor）只得用来初始化它的基类，以及它所属的类的非静态成员";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_12_8_1);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_12_8_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    auto declIsStaticAndIsInCopyConstructor =
        declRefExpr(hasAncestor(cxxConstructorDecl(
                        isCopyConstructor(), hasAncestor(cxxRecordDecl()))),
                    to(varDecl(hasStaticStorageDuration())))
            .bind("decl");
    // using '=', '+=', '-=' to initialize the static member in the copy
    // constructor
    finder->addMatcher(
        binaryOperation(anyOf(isAssignmentOperator(), hasOperatorName("+="),
                              hasOperatorName("-=")),
                        hasLHS(declIsStaticAndIsInCopyConstructor)),
        this);
    // using '++', '--' to initialize the static member in the copy constructor
    finder->addMatcher(
        unaryOperator(anyOf(hasOperatorName("++"), hasOperatorName("--")),
                      hasUnaryOperand(declIsStaticAndIsInCopyConstructor)),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const DeclRefExpr* decl_ = result.Nodes.getNodeAs<DeclRefExpr>("decl");
    std::string path_ =
        misra::libtooling_utils::GetFilename(decl_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(decl_, result.SourceManager);
    const SourceLocation loc = decl_->getLocation();
    if (result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    ReportError(path_, line_number_, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_12_8_1
}  // namespace misra_cpp_2008
