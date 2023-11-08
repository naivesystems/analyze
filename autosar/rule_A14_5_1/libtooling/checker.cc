/*
Copyright 2023 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "autosar/rule_A14_5_1/libtooling/checker.h"

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
      "A template constructor shall not participate in overload resolution for a single argument of the enclosing class type.";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_5_2);
}

}  // namespace

// The implementation is a stricter version of misra_cpp_2008 rule_14_5_2.
// As long as there is a copy constructor declaration in the template memeber
// function, it should be reported, even if there is an explicit
// CXXConstructorDecl.

namespace autosar {
namespace rule_A14_5_1 {
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
    if (!isCopyConstructorTemplateMemberFunction(decl_)) {
      return;
    }

    ReportError(path_, line_number_, results_list_);
  }

 private:
  ResultsList* results_list_;
  bool isCopyConstructorTemplateMemberFunction(
      const CXXConstructorDecl* decl) const {
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
}  // namespace rule_A14_5_1
}  // namespace autosar
