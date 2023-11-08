/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_7_3_2/libtooling/checker.h"

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
  string error_message = "除全局函数main外，标识符main不得用于其他函数";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_3_2);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_3_2 {
namespace libtooling {

class FDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(hasName("main"), unless(isMain())).bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd_ = result.Nodes.getNodeAs<FunctionDecl>("fd");
    std::string path_ =
        misra::libtooling_utils::GetFilename(fd_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(fd_, result.SourceManager);
    const SourceLocation loc = fd_->getLocation();
    if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
      ReportError(path_, line_number_, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new FDCallback;
  callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_7_3_2
}  // namespace misra_cpp_2008
