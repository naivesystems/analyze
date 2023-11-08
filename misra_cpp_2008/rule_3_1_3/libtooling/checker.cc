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

#include "misra_cpp_2008/rule_3_1_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_3_1_3 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(varDecl(hasType(arrayType())).bind("vd"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    if (misra::libtooling_utils::IsInSystemHeader(vd, result.Context)) {
      return;
    }
    if (!vd->getType()->isIncompleteArrayType()) {
      return;
    }
    string error_message =
        "声明一个数组时，应该显式说明其大小，或通过初始化隐式定义其大小";
    string path =
        misra::libtooling_utils::GetFilename(vd, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(vd, result.SourceManager);
    misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                              error_message);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_3_1_3
}  // namespace misra_cpp_2008
