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

#ifndef ANALYZER_MISRA_CPP_2008_RULE_17_0_2_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_17_0_2_LIBTOOLING_CHECKER_H_

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_17_0_2 {
namespace libtooling {

class Check : public clang::PPCallbacks {
 public:
  void Init(analyzer::proto::ResultsList* results_list_,
            clang::SourceManager* source_manager);

  void MacroDefined(const clang::Token& macro_name_tok,
                    const clang::MacroDirective* md) override;

 private:
  clang::SourceManager* source_manager_;
  analyzer::proto::ResultsList* results_list_;
  std::set<std::string> macros_;
};

class VarDeclVisitor : public clang::RecursiveASTVisitor<VarDeclVisitor> {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::SourceManager* source_manager);
  bool VisitVarDecl(clang::VarDecl* vd);

 private:
  analyzer::proto::ResultsList* results_list_;
  clang::SourceManager* source_manager_;
  std::set<std::string> objects_;
};

class VarDeclConsumer : public clang::ASTConsumer {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::SourceManager* source_manager) {
    results_list_ = results_list;
    visitor_.Init(results_list, source_manager);
  }
  virtual void HandleTranslationUnit(clang::ASTContext& context) {
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
  }

 private:
  VarDeclVisitor visitor_;
  analyzer::proto::ResultsList* results_list_;
  clang::SourceManager* source_manager_;
};

class Action : public clang::ASTFrontendAction {
 public:
  Action(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::make_unique<clang::ASTConsumer>();
  }
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, llvm::StringRef in_file) {
    auto consumer = std::make_unique<VarDeclConsumer>();
    consumer->Init(results_list_, &ci.getSourceManager());
    return consumer;
  }

  bool BeginSourceFileAction(clang::CompilerInstance& ci);

 private:
  analyzer::proto::ResultsList* results_list_;
};

class Checker : public clang::tooling::FrontendActionFactory {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<Action>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_17_0_2
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_RULE_17_0_2_LIBTOOLING_CHECKER_H_
