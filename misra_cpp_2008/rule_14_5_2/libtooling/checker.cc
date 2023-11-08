/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_14_5_2/libtooling/checker.h"

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
      "如果模板构造函数只有一个形参，且为泛型形参，那么必须声明一个复制构造函数";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_5_2);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_14_5_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(cxxConstructorDecl(hasParent(functionTemplateDecl()),
                                          ofClass(cxxRecordDecl().bind("cls")))
                           .bind("decl"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CXXConstructorDecl* decl_ =
        result.Nodes.getNodeAs<CXXConstructorDecl>("decl");
    std::string path_ =
        misra::libtooling_utils::GetFilename(decl_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(decl_, result.SourceManager);
    const SourceLocation loc = decl_->getLocation();
    if (result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    // Make sure the CXXConstructorDecl is a copy constructor declaration in the
    // template member function.
    if (!isCopyConstructor(decl_)) {
      return;
    }
    // If there is not an explicit declaration of the copy constructor for the
    // template constructor, its location may fall into the line of its class
    // since an implicit CXXConstructorDecl may be generated automatically
    // there as a non-explicit inline public member of its class. See more
    // details in: https://en.cppreference.com/w/cpp/language/copy_constructor.

    // Sometimes, the implicit declaration may be undefined or deleted. See the
    // section of Deleted implicitly-declared copy constructor of the above
    // link for the conditions.
    const CXXRecordDecl* cls = result.Nodes.getNodeAs<CXXRecordDecl>("cls");
    std::string cls_path =
        misra::libtooling_utils::GetFilename(cls, result.SourceManager);
    int cls_line_number =
        misra::libtooling_utils::GetLine(cls, result.SourceManager);
    // Find out whether there is an implicit or explicit CXXConstructorDecl.
    bool hasExplicitDecl = false;
    for (CXXRecordDecl::ctor_iterator i = cls->ctor_begin(),
                                      end = cls->ctor_end();
         i != end; i++) {
      const CXXConstructorDecl* ctor = dyn_cast<CXXConstructorDecl>(*i);
      if (!ctor->isCopyConstructor()) {
        continue;
      }
      std::string ctor_path =
          misra::libtooling_utils::GetFilename(ctor, result.SourceManager);
      int ctor_line_number =
          misra::libtooling_utils::GetLine(ctor, result.SourceManager);
      // Find out whether there is an implicit CXXConstructorDecl. The ctor
      // located in the class line indicates it is an implicit declaration
      // generated automatically. If there is any implicit CXXConstructorDecl,
      // report an error and return.
      if (ctor_path == cls_path && ctor_line_number == cls_line_number) {
        ReportError(path_, line_number_, results_list_);
        return;
      }
      // Find out whether there is an explicit CXXConstructorDecl. If there is
      // not any explicit CXXConstructorDecl, report an error in the end.
      if (ctor != decl_) {
        hasExplicitDecl = true;
      }
    }
    if (!hasExplicitDecl) {
      ReportError(path_, line_number_, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
  bool isCopyConstructor(const CXXConstructorDecl* decl) const {
    // adapt CXXConstructorDecl::isCopyConstructor for template member functions
    // in class
    if (!decl->hasOneParamOrDefaultArgs()) return false;
    bool isLValueReferenceType =
        decl->getParamDecl(0)->getType()->isLValueReferenceType();
    const ParmVarDecl* Param = decl->getParamDecl(0);
    const auto* ParamRefType = Param->getType()->getAs<ReferenceType>();
    if (!ParamRefType) return false && isLValueReferenceType;
    ASTContext& Context = decl->getASTContext();
    CanQualType PointeeType =
        Context.getCanonicalType(ParamRefType->getPointeeType());
    CanQualType ClassTy =
        Context.getCanonicalType(Context.getTagDeclType(decl->getParent()));
    if (PointeeType.getUnqualifiedType() != ClassTy)
      return false && isLValueReferenceType;
    return true && isLValueReferenceType;
  }
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_14_5_2
}  // namespace misra_cpp_2008
