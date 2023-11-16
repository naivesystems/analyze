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

#ifndef ANALYZER_MISRA_rule_1_1_CHECKER_H_
#define ANALYZER_MISRA_rule_1_1_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/MacroArgs.h>
#include <clang/Tooling/Tooling.h>
#include <glog/logging.h>

#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace rule_1_1 {

struct Identifier {
  string fullname;
  SourceLocation loc;
};

struct LimitList {
  int struct_member_limit;
  int function_parm_limit;
  int function_arg_limit;
  int nested_record_limit;
  int nested_expr_limit;
  int switch_case_limit;
  int enum_constant_limit;
  int string_char_limit;
  int extern_id_limit;
  int macro_id_limit;
  int macro_parm_limit;
  int macro_arg_limit;
  int nested_block_limit;
  int nested_include_limit;
  int iom_id_char_limit;
  int nested_cond_inclu_limit;
  int block_id_limit;
  int nested_decl_limit;
  int modify_decl_limit;
};

class StructMemberCallback;
class FunctionParmCallback;
class FunctionArgCallback;
class NestedRecordCallback;
class NestedExprCallback;
class SwitchCaseCallback;
class EnumConstantCallback;
class StringCharCallback;
class ExternIDCallback;
class NestedBlockCallback;
class InternIDCharCallback;
class BlockIDCallback;
class NestedDeclCallback;
class ModifyDeclCallback;

class ASTChecker {
 public:
  void Init(LimitList* limits, ResultsList* results_list);
  void Report();

  ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  StructMemberCallback* struct_member_callback_;
  FunctionParmCallback* function_parm_callback_;
  FunctionArgCallback* function_arg_callback_;
  NestedRecordCallback* nested_record_callback_;
  NestedExprCallback* nested_expr_callback_;
  SwitchCaseCallback* switch_case_callback_;
  EnumConstantCallback* enum_constant_callback_;
  StringCharCallback* string_char_callback_;
  ExternIDCallback* extern_id_callback_;
  NestedBlockCallback* nested_block_callback_;
  InternIDCharCallback* intern_id_char_callback_;
  BlockIDCallback* block_id_callback_;
  NestedDeclCallback* nested_decl_callback_;
  ModifyDeclCallback* modify_decl_callback_;
  ast_matchers::MatchFinder finder_;
  ResultsList* results_list_;
};

class PreprocessConsumer : public ASTConsumer {
 public:
  explicit PreprocessConsumer(ASTContext* context, ResultsList* results_list,
                              LimitList* limits, CompilerInstance& compiler)
      : results_list_(results_list), limits_(limits), compiler_(compiler) {}

  virtual void HandleTranslationUnit(ASTContext& context);

 private:
  ResultsList* results_list_;
  LimitList* limits_;
  CompilerInstance& compiler_;
};

class PPCheck : public PPCallbacks {
 public:
  PPCheck(SourceManager* sm, LimitList* limits, ResultsList* result_list)
      : source_manager_(sm), limits_(limits), results_list_(result_list) {}
  void MacroExpands(const Token& MacroNameTok, const MacroDefinition& MD,
                    SourceRange Range, const MacroArgs* Args) override;
  void LexedFileChanged(FileID FID, LexedFileChangeReason Reason,
                        SrcMgr::CharacteristicKind FileType, FileID PrevFID,
                        SourceLocation Loc) override;
  void If(SourceLocation Loc, SourceRange ConditionRange,
          ConditionValueKind ConditionValue) override;
  void Ifdef(SourceLocation Loc, const Token& MacroNameTok,
             const MacroDefinition& MD) override;
  void Ifndef(SourceLocation Loc, const Token& MacroNameTok,
              const MacroDefinition& MD) override;
  void Endif(SourceLocation Loc, SourceLocation IfLoc) override;

 private:
  int include_depth_ = -1;
  unsigned include_max_depth_ = 0;
  unsigned cond_inclu_depth_ = 0;
  unsigned cond_inclu_max_depth_ = 0;
  SourceManager* source_manager_;
  LimitList* limits_;
  ResultsList* results_list_;
};

class PreprocessAction : public ASTFrontendAction {
 public:
  PreprocessAction(ResultsList* results_list, LimitList* limits)
      : results_list_(results_list), limits_(limits) {}
  virtual unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                                    StringRef infile) {
    return make_unique<PreprocessConsumer>(&compiler.getASTContext(),
                                           results_list_, limits_, compiler);
  }
  bool BeginSourceFileAction(CompilerInstance& ci);

 private:
  ResultsList* results_list_;
  LimitList* limits_;
};

class PreprocessChecker : public tooling::FrontendActionFactory {
 public:
  PreprocessChecker(ResultsList* results_list, LimitList* limits)
      : results_list_(results_list), limits_(limits) {}
  unique_ptr<FrontendAction> create() override {
    return std::make_unique<PreprocessAction>(results_list_, limits_);
  }

 private:
  ResultsList* results_list_;
  LimitList* limits_;
};

}  // namespace rule_1_1
}  // namespace misra

#endif  // ANALYZER_MISRA_rule_1_1_CHECKER_H_
