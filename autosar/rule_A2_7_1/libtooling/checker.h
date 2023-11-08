/*
Copyright 2023 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_AUTOSAR_RULE_A2_7_1_LIBTOOLING_CHECKER_H_
#define ANALYZER_AUTOSAR_RULE_A2_7_1_LIBTOOLING_CHECKER_H_

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

using analyzer::proto::ResultsList;

namespace autosar {
namespace rule_A2_7_1 {
namespace libtooling {

class CheckCommentConsumer : public clang::ASTConsumer {
  analyzer::proto::ResultsList* results_list_;

 public:
  explicit CheckCommentConsumer(clang::ASTContext* context,
                                analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}

  void HandleTranslationUnit(clang::ASTContext& context);

  void ReportError(std::string path, int line_number,
                   ResultsList* results_list);
};

class CheckCommentAction : public clang::ASTFrontendAction {
  analyzer::proto::ResultsList* results_list_;

 public:
  CheckCommentAction(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& compiler, llvm::StringRef infile) {
    return std::make_unique<CheckCommentConsumer>(&compiler.getASTContext(),
                                                  results_list_);
  }
};

class CommentChecker : public clang::tooling::FrontendActionFactory {
 public:
  CommentChecker(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<CheckCommentAction>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_A2_7_1
}  // namespace autosar
#endif  // ANALYZER_AUTOSAR_RULE_A2_7_1_LIBTOOLING_CHECKER_H_