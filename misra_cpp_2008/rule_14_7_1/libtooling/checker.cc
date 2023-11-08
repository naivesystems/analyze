/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "misra_cpp_2008/rule_14_7_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {
void ReportError(const Decl* decl, SourceManager* sm,
                 ResultsList* results_list) {
  string error_message =
      "所有类模板、函数模板、类模板成员函数和类模板静态成员至少要实例化一次";
  analyzer::proto::Result* pb_result = AddResultToResultsList(
      results_list, misra::libtooling_utils::GetFilename(decl, sm),
      misra::libtooling_utils::GetLine(decl, sm), error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_7_1);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_14_7_1 {
namespace libtooling {
class ClassTemplateDeclCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    this->results_list_ = results_list;

    finder->addMatcher(classTemplateDecl().bind("class_template_unins"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const ClassTemplateDecl* class_template =
        result.Nodes.getNodeAs<ClassTemplateDecl>("class_template_unins");
    if (misra::libtooling_utils::IsInSystemHeader(class_template,
                                                  result.Context)) {
      return;
    }
    if (!class_template->specializations().empty()) {
      return;
    }
    ReportError(class_template, result.SourceManager, results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class ClassTemplateInstanceCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(classTemplateSpecializationDecl(
                           forEachDescendant(cxxMethodDecl().bind("method"))),
                       this);
    finder->addMatcher(classTemplatePartialSpecializationDecl(
                           forEachDescendant(cxxMethodDecl().bind("method"))),
                       this);

    finder->addMatcher(classTemplateSpecializationDecl(
                           forEachDescendant(fieldDecl().bind("field_decl"))),
                       this);
    finder->addMatcher(classTemplatePartialSpecializationDecl(
                           forEachDescendant(fieldDecl().bind("field_decl"))),
                       this);

    finder->addMatcher(
        classTemplateSpecializationDecl(forEachDescendant(
            varDecl(isStaticStorageClass()).bind("static_var_decl"))),
        this);
    finder->addMatcher(
        classTemplatePartialSpecializationDecl(forEachDescendant(
            varDecl(isStaticStorageClass()).bind("static_var_decl"))),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* cxx_method =
        result.Nodes.getNodeAs<CXXMethodDecl>("method");
    const FieldDecl* field_decl =
        result.Nodes.getNodeAs<FieldDecl>("field_decl");
    const VarDecl* static_var_decl =
        result.Nodes.getNodeAs<VarDecl>("static_var_decl");

    if (cxx_method) {
      if (misra::libtooling_utils::IsInSystemHeader(cxx_method,
                                                    result.Context)) {
        return;
      }
      if (isa<CXXConstructorDecl>(cxx_method) ||
          isa<CXXDestructorDecl>(cxx_method)) {
        return;
      }
      if (cxx_method->isUsed()) {
        return;
      }
      ReportError(cxx_method, result.SourceManager, results_list_);
    }

    if (field_decl) {
      if (misra::libtooling_utils::IsInSystemHeader(field_decl,
                                                    result.Context)) {
        return;
      }

      if (field_decl->isReferenced()) {
        return;
      }

      ReportError(field_decl, result.SourceManager, results_list_);
    }
    if (static_var_decl) {
      if (misra::libtooling_utils::IsInSystemHeader(static_var_decl,
                                                    result.Context)) {
        return;
      }
      if (static_var_decl->isUsed()) {
        return;
      }
      ReportError(static_var_decl, result.SourceManager, results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class FuncTemplateInitCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    this->results_list_ = results_list;

    finder->addMatcher(functionTemplateDecl().bind("func_template"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const FunctionTemplateDecl* func_template =
        result.Nodes.getNodeAs<FunctionTemplateDecl>("func_template");
    if (misra::libtooling_utils::IsInSystemHeader(func_template,
                                                  result.Context)) {
      return;
    }
    if (!func_template->specializations().empty()) {
      return;
    }
    ReportError(func_template, result.SourceManager, results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  class_template_decl_callback_ = new ClassTemplateDeclCallback;
  class_template_decl_callback_->Init(results_list, &finder_);
  class_template_callback_ = new ClassTemplateInstanceCallback;
  class_template_callback_->Init(results_list, &finder_);
  func_template_callback_ = new FuncTemplateInitCallback;
  func_template_callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_14_7_1
}  // namespace misra_cpp_2008
