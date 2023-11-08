/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_14_5_3/libtooling/checker.h"

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
      "如果模板赋值运算符有一个泛型形参，那么必须声明一个复制赋值运算符";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_5_3);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_14_5_3 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(cxxMethodDecl(hasParent(functionTemplateDecl()),
                                     ofClass(cxxRecordDecl().bind("cls")))
                           .bind("decl"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* decl_ = result.Nodes.getNodeAs<CXXMethodDecl>("decl");
    std::string path_ =
        misra::libtooling_utils::GetFilename(decl_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(decl_, result.SourceManager);
    const SourceLocation loc = decl_->getLocation();
    if (result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    // Make sure the CXXMethodDecl is a copy assignment operator in the template
    // member function.
    if (!isCopyAssignmentOperator(decl_)) {
      return;
    }
    // If there is not an explicit declaration of the copy assignment operator
    // for the template assignment operator, its location may fall into the
    // line of its class since an implicit CXXMethodDecl may be generated
    // automatically there as an inline public member of its class. See more
    // details in: https://en.cppreference.com/w/cpp/language/copy_assignment.

    // Sometimes, the implicit declaration may be undefined or deleted. See the
    // section of Deleted implicitly-declared copy assignment operator of the
    // above link for the conditions.
    const CXXRecordDecl* cls = result.Nodes.getNodeAs<CXXRecordDecl>("cls");
    std::string cls_path =
        misra::libtooling_utils::GetFilename(cls, result.SourceManager);
    int cls_line_number =
        misra::libtooling_utils::GetLine(cls, result.SourceManager);
    // Find out whether there is an implicit or explicit copy assignment
    // operator.
    bool hasExplicitDecl = false;
    for (CXXRecordDecl::method_iterator i = cls->method_begin(),
                                        end = cls->method_end();
         i != end; i++) {
      const CXXMethodDecl* method = dyn_cast<CXXMethodDecl>(*i);
      if (!method->isCopyAssignmentOperator()) {
        continue;
      }
      std::string method_path =
          misra::libtooling_utils::GetFilename(method, result.SourceManager);
      int method_line_number =
          misra::libtooling_utils::GetLine(method, result.SourceManager);
      // Find out whether there is an implicit copy assignment operator. The
      // method decl located in the class line indicates it is an implicit
      // declaration generated automatically. If there is any implicit copy
      // assignment operator, report an error and return.
      if (method_path == cls_path && method_line_number == cls_line_number) {
        ReportError(path_, line_number_, results_list_);
        return;
      }
      // Find out whether there is an explicit copy assignment operator. If
      // not any, report an error in the end.
      if (method != decl_) {
        hasExplicitDecl = true;
      }
    }
    if (!hasExplicitDecl) {
      ReportError(path_, line_number_, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
  bool isCopyAssignmentOperator(const CXXMethodDecl* decl) const {
    // adapt CXXMethodDecl::isCopyAssignmentOperator for template member
    // functions in class
    if (/*operator=*/decl->getOverloadedOperator() != OO_Equal ||
        /*non-static*/ decl->isStatic() || decl->getNumParams() != 1)
      return false;
    QualType ParamType = decl->getParamDecl(0)->getType();
    if (const auto* Ref = ParamType->getAs<LValueReferenceType>())
      ParamType = Ref->getPointeeType();
    ASTContext& Context = decl->getASTContext();
    QualType ClassType =
        Context.getCanonicalType(Context.getTypeDeclType(decl->getParent()));
    return Context.hasSameUnqualifiedType(ClassType, ParamType);
  }
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_14_5_3
}  // namespace misra_cpp_2008
