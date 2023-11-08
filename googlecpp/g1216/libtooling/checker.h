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

#ifndef ANALYZER_GOOGLECPP_G1216_LIBTOOLING_CHECKER_H_
#define ANALYZER_GOOGLECPP_G1216_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

namespace googlecpp {
namespace g1216 {
namespace libtooling {

template <class MatchT, class NodeT>
class Callback : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::ast_matchers::MatchFinder* finder, MatchT matcher);

  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override;

 private:
  analyzer::proto::ResultsList* results_list_;
};

typedef Callback<clang::ast_matchers::DeclarationMatcher, clang::Decl>
    DeclCallback;
typedef Callback<clang::ast_matchers::StatementMatcher, clang::Stmt>
    StmtCallback;
typedef Callback<clang::ast_matchers::TypeLocMatcher, clang::TypeLoc>
    TypeLocCallback;
typedef Callback<clang::ast_matchers::StatementMatcher, clang::FunctionDecl>
    FunctionUseCallback;

class ASTChecker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  // decls with GNU attributes & FileScopeAsmDecl
  DeclCallback* attr_callback_;
  // GNU Statement Expression extension: ({int X=4; X;})
  StmtCallback* stmtexpr_callback_;
  // GNU binary conditional operator (a ?: b)
  StmtCallback* cond_callback_;
  // GNU address of label statements
  StmtCallback* addrlabel_callback_;
  // GNU __builtin_choose_expr
  StmtCallback* choose_callback_;
  // GNU __null
  StmtCallback* gnunull_callback_;
  // GNU case range extension (switch (1) {case1: case 1+1: case 3 ... 4: ; })
  StmtCallback* caserange_callback_;
  // intrinsic functions like __builtin_prefetch， also checks use of alloca
  FunctionUseCallback* intrinsticf_callback_;
  // inline assembly
  StmtCallback* asm_callback_;
  // predefined expressions like __func__
  StmtCallback* predefined_callback_;
  // designated initializers, supported since C++20
  // TODO: we should add an option to decide whether use this feature
  StmtCallback* designated_callback_;
  TypeLocCallback* va_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

class MacroCallback : public clang::PPCallbacks {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::SourceManager* source_manager) {
    results_list_ = results_list;
    source_manager_ = source_manager;
  }
  void MacroExpands(const clang::Token& macro_name_tok,
                    const clang::MacroDefinition& md, clang::SourceRange range,
                    const clang::MacroArgs* args) override;

 private:
  analyzer::proto::ResultsList* results_list_;
  clang::SourceManager* source_manager_;
};

class Action : public clang::ASTFrontendAction {
 public:
  Action(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, llvm::StringRef in_file) override {
    ASTChecker* astchecker = new ASTChecker;
    astchecker->Init(results_list_);
    return astchecker->GetMatchFinder()->newASTConsumer();
  }
  bool BeginSourceFileAction(clang::CompilerInstance& ci) override {
    std::unique_ptr<MacroCallback> callback(new MacroCallback());
    callback->Init(results_list_, &ci.getSourceManager());
    ci.getPreprocessor().addPPCallbacks(std::move(callback));
    return true;
  };

 private:
  analyzer::proto::ResultsList* results_list_;
};

class ActionFactory : public clang::tooling::FrontendActionFactory {
 public:
  void Init(analyzer::proto::ResultsList* results_list) {
    results_list_ = results_list;
  }
  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<Action>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace g1216
}  // namespace googlecpp

#endif
