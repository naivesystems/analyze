/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_3_1_1/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-3.1.1]: 在不违反“单一定义”规则的情况下，必须可以在多个翻译单元中包含任何头文件";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_1_1 {
namespace libtooling {

void CheckFuncDeclCallback::Init(analyzer::proto::ResultsList* results_list,
                                 ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;
  auto matcher = anyOf(functionDecl(), varDecl(isDefinition()));
  finder->addMatcher(namedDecl(matcher).bind("named-decl"), this);
}

void CheckFuncDeclCallback::run(
    const ast_matchers::MatchFinder::MatchResult& result) {
  const NamedDecl* nd = result.Nodes.getNodeAs<NamedDecl>("named-decl");
  SourceManager* source_manager = result.SourceManager;
  if (misra::libtooling_utils::IsInSystemHeader(nd, result.Context)) {
    return;
  }

  if (!misra::libtooling_utils::isInHeader(nd, source_manager)) {
    return;
  }

  if (nd->isInvalidDecl()) {
    return;
  }

  if (const auto* fd = dyn_cast<FunctionDecl>(nd)) {
    if (!fd->hasBody() && !fd->isStatic()) return;
    // Inline functions are allowed.
    if (fd->isInlined()) return;
    // Function templates are allowed.
    if (fd->getTemplatedKind() == FunctionDecl::TK_FunctionTemplate) return;
    // Ignore instantiated functions.
    if (fd->isTemplateInstantiation()) return;
    if (fd->getStorageClass() == clang::SC_Extern) return;
    // Member function of a class template and member function of a nested
    // class in a class template are allowed.
    if (const auto* md = dyn_cast<CXXMethodDecl>(fd)) {
      const auto* dc = md->getDeclContext();
      while (dc->isRecord()) {
        if (const auto* rd = dyn_cast<CXXRecordDecl>(dc)) {
          if (isa<ClassTemplatePartialSpecializationDecl>(rd)) return;
          if (rd->getDescribedClassTemplate()) return;
        }
        dc = dc->getParent();
      }
    }

    if (fd->hasBody() || fd->isStatic()) {
      int line_loc = misra::libtooling_utils::GetLine(fd, source_manager);
      string file_loc =
          misra::libtooling_utils::GetFilename(fd, source_manager);
      ReportError(file_loc, line_loc, results_list_);
    }
  } else if (const auto* vd = dyn_cast<VarDecl>(nd)) {
    // C++14 variable templates are allowed.
    if (vd->getDescribedVarTemplate()) return;
    // Static data members of a class template are allowed.
    if (vd->getDeclContext()->isDependentContext() && vd->isStaticDataMember())
      return;
    // Ignore instantiated static data members of classes.
    if (isTemplateInstantiation(vd->getTemplateSpecializationKind())) return;
    // Ignore variable definition within function scope.
    if (vd->hasLocalStorage() || vd->isStaticLocal()) return;
    // Ignore inline variables.
    if (vd->isInline()) return;
    // Ignore partial specializations.
    if (isa<VarTemplatePartialSpecializationDecl>(vd)) return;
    if (vd->isConstexpr() || vd->getType().isConstQualified()) return;

    int line_loc = misra::libtooling_utils::GetLine(vd, source_manager);
    string file_loc = misra::libtooling_utils::GetFilename(vd, source_manager);
    ReportError(file_loc, line_loc, results_list_);
  }
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckFuncDeclCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_3_1_1
}  // namespace misra_cpp_2008
