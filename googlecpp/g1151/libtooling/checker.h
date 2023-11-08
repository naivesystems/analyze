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

#ifndef ANALYZER_GOOGLECPP_G1151_LIBTOOLING_CHECKER_H_
#define ANALYZER_GOOGLECPP_G1151_LIBTOOLING_CHECKER_H_

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include <fstream>

#include "misra/proto_util.h"

using namespace clang;

namespace googlecpp {
namespace g1151 {
namespace libtooling {
class Check : public PPCallbacks {
 public:
  void Init(analyzer::proto::ResultsList* results_list_,
            SourceManager* source_manager,
            const std::string& optional_info_file);

  void InclusionDirective(SourceLocation HashLoc, const Token& IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange FilenameRange,
                          Optional<FileEntryRef> File, StringRef SearchPath,
                          StringRef RelativePath, const Module* Imported,
                          SrcMgr::CharacteristicKind FileType) override;

 private:
  SourceManager* source_manager_;
  analyzer::proto::ResultsList* results_list_;
  std::ofstream ofs;
};

class Action : public ASTFrontendAction {
 public:
  Action(analyzer::proto::ResultsList* results_list,
         const std::string& optional_info_file)
      : results_list_(results_list), optional_info_file_(optional_info_file) {}
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
  std::string optional_info_file_;
};

class Checker : public tooling::FrontendActionFactory {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            const std::string& optional_info_file);
  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<Action>(results_list_, optional_info_file_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::string optional_info_file_;
};

}  // namespace libtooling
}  // namespace g1151
}  // namespace googlecpp

#endif  // ANALYZER_GOOGLECPP_G1151_LIBTOOLING_CHECKER_H_
