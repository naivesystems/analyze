/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_14_7_3/libtooling/checker.h"

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
      "模板的所有部分和显式特化应在与其主模板的声明相同的文件中声明";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_7_3);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_14_7_3 {
namespace libtooling {

class FuncDeclCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    // function only supports full specialization
    finder->addMatcher(functionTemplateDecl().bind("func_template"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* sm = result.SourceManager;
    const FunctionTemplateDecl* func_template =
        result.Nodes.getNodeAs<FunctionTemplateDecl>("func_template");
    if (misra::libtooling_utils::IsInSystemHeader(func_template,
                                                  result.Context)) {
      return;
    }
    if (func_template->specializations().empty()) {
      return;
    }
    for (auto i = func_template->spec_begin(); i != func_template->spec_end();
         ++i) {
      if (!i->getTemplateSpecializationInfo()) {
        continue;
      }
      auto func_decl = i->getTemplateSpecializationInfo()->getFunction();
      auto func_decl_file = misra::libtooling_utils::GetFilename(func_decl, sm);
      auto primary_file =
          misra::libtooling_utils::GetFilename(func_template, sm);
      if (func_decl_file == primary_file) {
        continue;
      }
      if (!func_decl->getPreviousDecl()) {
        continue;
      }
      // if the previous decl is not the canonical decl
      // this decl has been examined, need to skip
      if (func_decl->getPreviousDecl()->getSourceRange() !=
          func_decl->getCanonicalDecl()->getSourceRange()) {
        continue;
      }
      ReportError(func_decl_file,
                  misra::libtooling_utils::GetLine(func_decl, sm),
                  results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

class ClassDeclCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(
        classTemplateSpecializationDecl(
            hasSpecializedTemplate(classTemplateDecl().bind("class_template")))
            .bind("class_decl"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* sm = result.SourceManager;
    auto* class_decl =
        result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("class_decl");
    if (misra::libtooling_utils::IsInSystemHeader(class_decl, result.Context)) {
      return;
    }
    if (!class_decl->isThisDeclarationADefinition()) {
      return;
    }
    // if a decl has prev, it means that this decl has been examined,
    // need to skip
    if (class_decl->getPreviousDecl()) {
      return;
    }
    auto primary_templ =
        result.Nodes.getNodeAs<ClassTemplateDecl>("class_template");
    if (misra::libtooling_utils::IsInSystemHeader(primary_templ,
                                                  result.Context)) {
      return;
    }
    auto class_decl_file = misra::libtooling_utils::GetFilename(class_decl, sm);
    auto primary_template_file =
        misra::libtooling_utils::GetFilename(primary_templ, sm);
    if (class_decl_file == primary_template_file) {
      return;
    }
    ReportError(class_decl_file,
                misra::libtooling_utils::GetLine(class_decl, sm),
                results_list_);
  }

 private:
  ResultsList* results_list_;
};
void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  func_callback_ = new FuncDeclCallback;
  func_callback_->Init(&finder_, results_list);
  class_callback_ = new ClassDeclCallback;
  class_callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_14_7_3
}  // namespace misra_cpp_2008
