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

#include "misra/dir_4_12/checker.h"

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
namespace dir_4_12 {

class CallCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(callExpr(callee(decl().bind("callee"))).bind("call"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* function_call = result.Nodes.getNodeAs<Expr>("call");
    const Decl* callee_function = result.Nodes.getNodeAs<Decl>("callee");
    if (!callee_function->getAsFunction()) {
      return;
    }
    string function_name = callee_function->getAsFunction()->getNameAsString();
    if (function_name == "malloc" || function_name == "calloc" ||
        function_name == "realloc" || function_name == "free") {
      std::string error_message =
          "[C2315][misra-c2012-dir-4.12]: dynamic allocation should not be used";
      analyzer::proto::Result* pb_result = AddResultToResultsList(
          results_list_,
          libtooling_utils::GetFilename(function_call, result.SourceManager),
          libtooling_utils::GetLine(function_call, result.SourceManager),
          error_message);
      pb_result->set_error_kind(
          analyzer::proto::Result_ErrorKind_MISRA_C_2012_DIR_4_12);
      LOG(INFO) << error_message;
    }
  }

 private:
  ResultsList* results_list_;
  string GetFunctionName(const Decl* decl,
                         const SourceManager* source_manager) {
    return decl->getAsFunction()->getNameAsString();
  }
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new CallCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace dir_4_12
}  // namespace misra
