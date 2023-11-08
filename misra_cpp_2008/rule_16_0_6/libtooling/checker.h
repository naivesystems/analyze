/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_rule_16_0_6_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_rule_16_0_6_LIBTOOLING_CHECKER_H_

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

using namespace clang;

namespace misra_cpp_2008 {
namespace rule_16_0_6 {
namespace libtooling {

class FindMacroDefineCallback : public PPCallbacks {
 public:
  void Init(analyzer::proto::ResultsList* results_list_,
            SourceManager* source_manager);

  void MacroDefined(const Token& macro_name_tok,
                    const MacroDirective* md) override;

 private:
  SourceManager* source_manager_;
  analyzer::proto::ResultsList* results_list_;
};

class Action : public ASTFrontendAction {
 public:
  Action(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, llvm::StringRef in_file) override {
    return std::make_unique<ASTConsumer>();
  }
  bool BeginSourceFileAction(CompilerInstance& ci) override;

 private:
  analyzer::proto::ResultsList* results_list_;
};

class Checker : public tooling::FrontendActionFactory {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<Action>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_16_0_6
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_rule_16_0_6_LIBTOOLING_CHECKER_H_
