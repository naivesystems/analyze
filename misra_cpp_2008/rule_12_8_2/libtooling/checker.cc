/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_12_8_2/libtooling/checker.h"

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
      "在抽象类中，复制赋值运算符必须被声明为保护（protected）或私有（private）";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_12_8_2);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_12_8_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxMethodDecl(isCopyAssignmentOperator(), isUserProvided(),
                      unless(anyOf(isPrivate(), isProtected())),
                      ofClass(cxxRecordDecl().bind("cls")))
            .bind("assign"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* assign_ =
        result.Nodes.getNodeAs<CXXMethodDecl>("assign");
    const CXXRecordDecl* cls_ = result.Nodes.getNodeAs<CXXRecordDecl>("cls");
    std::string path_ =
        misra::libtooling_utils::GetFilename(assign_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(assign_, result.SourceManager);
    const SourceLocation loc = assign_->getLocation();
    if (result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    if (cls_->isAbstract()) {
      ReportError(path_, line_number_, results_list_);
    }
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
}  // namespace rule_12_8_2
}  // namespace misra_cpp_2008
