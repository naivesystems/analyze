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

#ifndef ANALYZER_MISRA_CPP_2008_RULE_2_7_2_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_2_7_2_LIBTOOLING_CHECKER_H_

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_2_7_2 {
namespace libtooling {

using namespace clang;

class CheckCommentConsumer : public ASTConsumer {
  analyzer::proto::ResultsList* results_list_;

 public:
  explicit CheckCommentConsumer(ASTContext* context,
                                analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}

  void HandleTranslationUnit(ASTContext& context) override;

  bool LooksLikeCode(std::string& line);

  void ReportError(ASTContext& context, RawComment* comment);
};

class CheckCommentAction : public ASTFrontendAction {
  analyzer::proto::ResultsList* results_list_;

 public:
  CheckCommentAction(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
      CompilerInstance& compiler, llvm::StringRef infile) {
    return std::make_unique<CheckCommentConsumer>(&compiler.getASTContext(),
                                                  results_list_);
  }
};

class CommentChecker : public tooling::FrontendActionFactory {
 public:
  CommentChecker(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<CheckCommentAction>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_2_7_2
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_RULE_2_7_2_LIBTOOLING_CHECKER_H_
