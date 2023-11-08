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

#include "misra_cpp_2008/rule_15_5_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
// using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_15_5_2 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        functionDecl(
            allOf(hasDynamicExceptionSpec(),
                  hasDescendant(cxxThrowExpr(has(expr().bind("throw_type"))))))
            .bind("decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("decl");
    const Expr* s = result.Nodes.getNodeAs<Expr>("throw_type");

    if (fd == nullptr || s == nullptr) return;

    auto fp = fd->getType()->getAs<FunctionProtoType>();
    // check if throw_type is in the exception list of the function
    if (fp->getExceptionSpecType() == EST_Dynamic) {
      for (auto it = fp->exceptions().begin(); it != fp->exceptions().end();
           it++) {
        if (it->getAsString() == s->getType().getAsString()) {
          return;
        }
      }
    }

    if (misra::libtooling_utils::IsInSystemHeader(fd, result.Context)) {
      return;
    }

    string error_message =
        "当一个函数的声明包含一个异常说明时，该函数只得抛出指定类型的异常";
    string path =
        misra::libtooling_utils::GetFilename(fd, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(fd, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_15_5_2);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_15_5_2
}  // namespace misra_cpp_2008
