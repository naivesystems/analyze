/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_7_3_1/libtooling/checker.h"

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
      "全局命名空间只得包含main，命名空间声明和extern “C”声明";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_3_1);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_3_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(decl(hasParent(translationUnitDecl())).bind("decl"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Decl* decl_ = result.Nodes.getNodeAs<Decl>("decl");
    std::string path_ =
        misra::libtooling_utils::GetFilename(decl_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(decl_, result.SourceManager);
    const SourceLocation loc = decl_->getLocation();
    if (!loc.isValid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    if (isa<NamespaceDecl>(decl_)) {
      return;
    }
    if (isa<FunctionDecl>(decl_) && dyn_cast<FunctionDecl>(decl_)->isMain()) {
      return;
    }
    // In enum LanguageIDs: lang_c = 1, lang_cxx = 2.
    if (isa<LinkageSpecDecl>(decl_) &&
        dyn_cast<LinkageSpecDecl>(decl_)->getLanguage() == 1) {
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
}  // namespace rule_7_3_1
}  // namespace misra_cpp_2008
