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

#ifndef ANALYZER_MISRA_DIR_4_10_CHECKER_H_
#define ANALYZER_MISRA_DIR_4_10_CHECKER_H_

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

using namespace clang;
using clang::tooling::FrontendActionFactory;
using std::map;
using std::string;

namespace misra {
namespace dir_4_10 {

// False Negative: write code before #ifndef or #if !defined
// False Negative: write code between #ifndef and #endif,
//   or #if !defined and #endif
// False Negative: write code after #endif
class PrecautionCheck : public PPCallbacks {
 public:
  void Init(analyzer::proto::ResultsList* results_list_,
            SourceManager* source_manager);

  void FileChanged(SourceLocation loc, FileChangeReason reason,
                   SrcMgr::CharacteristicKind file_type,
                   FileID prev_fid = FileID()) override;

  void Ifndef(SourceLocation loc, const Token& macro_name_tok,
              const MacroDefinition& md) override;

  void MacroDefined(const Token& macro_name_tok,
                    const MacroDirective* md) override;

  void Defined(const Token& macro_name_tok, const MacroDefinition& md,
               SourceRange range) override;

  void If(SourceLocation loc, SourceRange condition_range,
          ConditionValueKind condition_value) override;

 private:
  enum class FileState;

  map<string, FileState> file_states_;
  map<string, string> macro_files_;
  string current_file_;
  string current_macro_;
  SourceManager* source_manager_;
  analyzer::proto::ResultsList* results_list_;

  bool IsNotIgnoredHeaderFile(string filename);
  bool IsTerminated(FileState state);
  void CheckHeaderFile(string filename);
  void ReportError(string filename);
  void ReportError(string filename1, string filename2);
};

class PrecautionAction : public ASTFrontendAction {
 public:
  PrecautionAction(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  std::unique_ptr<ASTConsumer> newASTConsumer() {
    return std::make_unique<ASTConsumer>();
  }
  std::unique_ptr<ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci,
                                                 llvm::StringRef in_file) {
    return std::make_unique<ASTConsumer>();
  }

  bool BeginSourceFileAction(CompilerInstance& ci);

 private:
  analyzer::proto::ResultsList* results_list_;
};

class Checker : public FrontendActionFactory {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<PrecautionAction>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace dir_4_10
}  // namespace misra

#endif  // ANALYZER_MISRA_DIR_4_10_CHECKER_H_
