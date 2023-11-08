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

#include "misra/dir_4_3/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace dir_4_3 {

class AsmFunctionCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(asmStmt(hasAncestor(functionDecl(
                                   hasBody(has(stmt(unless(asmStmt())))))))
                           .bind("mixed_asm"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Stmt* mixed_asm = result.Nodes.getNodeAs<Stmt>("mixed_asm");

    std::string error_message =
        "[C2306][misra-c2012-dir-4.3]: assembly code should be isolated";
    std::string path =
        misra::libtooling_utils::GetFilename(mixed_asm, result.SourceManager);
    int line =
        misra::libtooling_utils::GetLine(mixed_asm, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_DIR_4_3_ASM_SHOULD_BE_ISOLATED);
    LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                                 line);
  }

 private:
  ResultsList* results_list_;
};

class AsmCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        asmStmt(unless(hasParent(compoundStmt(hasParent((functionDecl()))))))
            .bind("asm"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Stmt* asm_stmt = result.Nodes.getNodeAs<Stmt>("asm");
    if (misra::libtooling_utils::IsInMacroExpansion(asm_stmt,
                                                    result.SourceManager)) {
      return;
    }

    std::string error_message =
        "[C2306][misra-c2012-dir-4.3]: assembly code should be encapsulated";
    std::string path =
        misra::libtooling_utils::GetFilename(asm_stmt, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(asm_stmt, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_DIR_4_3_ASM_SHOULD_BE_ENCAPSULATED);
    LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                                 line);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  encapsulated_callback_ = new AsmCallback;
  encapsulated_callback_->Init(results_list_, &finder_);
  isolated_callback_ = new AsmFunctionCallback;
  isolated_callback_->Init(results_list_, &finder_);
}

}  // namespace dir_4_3
}  // namespace misra
