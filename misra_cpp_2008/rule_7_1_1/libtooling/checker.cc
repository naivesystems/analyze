/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_7_1_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "不修改的变量必须使用const修饰";
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_1_1);
  auto message = absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                                 line_number);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_1_1 {
namespace libtooling {

class StaticOrConstMethodCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    // This is different to 7.1.2 because we don't need to consider pointer
    // escaped, so the matcher is simpler.

    clang::ast_matchers::internal::BindableMatcher<clang::Stmt>
        unchanged_vd_ref = declRefExpr(to(varDecl(equalsBoundNode("vd"))),
                                       unless(hasParent(implicitCastExpr(
                                           hasCastKind(CK_LValueToRValue)))));

    // If all the declRefExpr of a variable in the function has not been
    // modified, the value of it is considered unchanged, i.e., it should have a
    // const qualifier in the declaration.
    finder->addMatcher(
        functionDecl(
            unless(isTemplateInstantiation()),
            forEachDescendant(
                varDecl(unless(hasType(isConstQualified()))).bind("vd")),
            unless(hasDescendant(unchanged_vd_ref)))
            .bind("fd"),
        this);

    results_list_ = results_list;
  }
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("vd");
    if (!var_decl) {
      return;
    }
    if (misra::libtooling_utils::IsInSystemHeader(var_decl, result.Context)) {
      return;
    }
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    if (!fd->isUserProvided()) {
      return;
    }
    ReportError(
        misra::libtooling_utils::GetFilename(var_decl, result.SourceManager),
        misra::libtooling_utils::GetLine(var_decl, result.SourceManager),
        results_list_);
    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new StaticOrConstMethodCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_7_1_1
}  // namespace misra_cpp_2008
