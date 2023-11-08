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

#include "misra_cpp_2008/rule_12_1_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_12_1_3 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        decl(allOf(functionDecl(allOf(parameterCountIs(1),
                                      hasParameter(0, hasType(builtinType())))),
                   unless(cxxConstructorDecl(isExplicit()))))
            .bind("decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const FunctionDecl* s = result.Nodes.getNodeAs<FunctionDecl>("decl");

    if (misra::libtooling_utils::IsInSystemHeader(s, result.Context)) {
      return;
    }
    if (!llvm::isa<CXXConstructorDecl>(s)) {
      return;
    }

    if (s->isDefaulted()) {
      return;
    }

    string error_message =
        "所有可使用基本类型的单个参数调用的构造函数都应声明为显式";
    string path = misra::libtooling_utils::GetFilename(s, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(s, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_12_1_3);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_12_1_3
}  // namespace misra_cpp_2008
