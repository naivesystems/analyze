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

#include "misra_cpp_2008/rule_14_6_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_14_6_2 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(callExpr(usesADL()).bind("call"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("call");

    if (misra::libtooling_utils::IsInSystemHeader(ce, result.Context)) {
      return;
    }

    bool isBefore = result.SourceManager->isBeforeInTranslationUnit(
        ce->getBeginLoc(), ce->getDirectCallee()->getBeginLoc());
    if (!isBefore) {
      return;
    }
    string error_message =
        "重载决议选择的函数应解析为先前在翻译单元中声明的函数";
    string path =
        misra::libtooling_utils::GetFilename(ce, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(ce, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_6_2);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_14_6_2
}  // namespace misra_cpp_2008
