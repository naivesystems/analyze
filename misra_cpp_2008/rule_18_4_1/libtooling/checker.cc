/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_18_4_1/libtooling/checker.h"

#include <glog/logging.h>

#include <regex>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "不应使用动态堆内存分配";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_18_4_1 {
namespace libtooling {

class SpecificFunctionUsageCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxNewExpr().bind("new"), this);
    finder->addMatcher(cxxDeleteExpr().bind("delete"), this);
    finder->addMatcher(declRefExpr(hasType(functionType())).bind("func_ptr"),
                       this);
  }
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXNewExpr* new_expr = result.Nodes.getNodeAs<CXXNewExpr>("new");
    const CXXDeleteExpr* delete_expr =
        result.Nodes.getNodeAs<CXXDeleteExpr>("delete");
    const DeclRefExpr* func_ptr =
        result.Nodes.getNodeAs<DeclRefExpr>("func_ptr");

    Stmt const* error_stmt = nullptr;

    if (nullptr != new_expr) {
      error_stmt = new_expr;
    }
    if (nullptr != delete_expr) {
      error_stmt = delete_expr;
    }
    if (nullptr != func_ptr) {
      string func_name = func_ptr->getNameInfo().getName().getAsString();
      // 参考 https://zh.cppreference.com/w/c/experimental/dynamic
      if (std::regex_match(
              func_name,
              std::regex("(m|re|c)alloc|free|getw?line|getw?delim|strn?dup")) &&
          misra::libtooling_utils::IsInSystemHeader(func_ptr->getFoundDecl(),
                                                    result.Context)) {
        error_stmt = func_ptr;
      }
    }
    if (nullptr != error_stmt && !misra::libtooling_utils::IsInSystemHeader(
                                     error_stmt, result.Context)) {
      ReportError(
          misra::libtooling_utils::GetFilename(error_stmt,
                                               result.SourceManager),
          misra::libtooling_utils::GetLine(error_stmt, result.SourceManager),
          results_list_);
    }

    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new SpecificFunctionUsageCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_18_4_1
}  // namespace misra_cpp_2008
