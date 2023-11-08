/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_7_3_5/libtooling/checker.h"

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
      "同一命名空间中对一个标识符的多个声明不得跨越该标识符的using声明";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_3_5);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_3_5 {
namespace libtooling {

unordered_map<string, vector<int>> decl_loc_;

class NamespaceFDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(namespaceDecl(has(functionDecl().bind("decl"))), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const NamedDecl* decl_ = result.Nodes.getNodeAs<NamedDecl>("decl");
    int line_number_ =
        misra::libtooling_utils::GetLine(decl_, result.SourceManager);
    const SourceLocation loc = decl_->getLocation();
    if (loc.isInvalid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    string name = decl_->getQualifiedNameAsString();
    decl_loc_[name].push_back(line_number_);
  }

 private:
  ResultsList* results_list_;
};

class UsingCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(usingDecl(hasAnyUsingShadowDecl(hasTargetDecl(
                                     functionDecl().bind("decl"))))
                           .bind("using"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const UsingDecl* using_ = result.Nodes.getNodeAs<UsingDecl>("using");
    const NamedDecl* decl_ = result.Nodes.getNodeAs<NamedDecl>("decl");
    std::string path_ =
        misra::libtooling_utils::GetFilename(using_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(using_, result.SourceManager);
    const SourceLocation loc = using_->getLocation();
    if (loc.isInvalid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    string name = decl_->getQualifiedNameAsString();
    if (decl_loc_[name].size() < 2) {
      return;
    } else {
      int max_line_number =
          *max_element(decl_loc_[name].begin(), decl_loc_[name].end());
      int min_line_number =
          *min_element(decl_loc_[name].begin(), decl_loc_[name].end());
      if (line_number_ > min_line_number && line_number_ < max_line_number) {
        // Report error in the line of the using declaration between namespace
        // declarations.
        ReportError(path_, line_number_, results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
};

// The namespace_checker is for collecting the line numbers of declarations in
// namespaces.
void NamespaceChecker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new NamespaceFDCallback;
  callback_->Init(&finder_, results_list);
}
// The using_checker is to check whether there is a using-declaration locating
// between its namespaces.
void UsingChecker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new UsingCallback;
  callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_7_3_5
}  // namespace misra_cpp_2008
