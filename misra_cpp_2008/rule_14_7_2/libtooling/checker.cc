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

#include "misra_cpp_2008/rule_14_7_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_14_7_2 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(classTemplateSpecializationDecl(
                           forEachDescendant(cxxMethodDecl().bind("method")))
                           .bind("template_spec"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* method =
        result.Nodes.getNodeAs<CXXMethodDecl>("method");

    if (misra::libtooling_utils::IsInSystemHeader(method, result.Context)) {
      return;
    }
    if (!method->isInvalidDecl()) {
      return;
    }

    const ClassTemplateSpecializationDecl* class_template =
        result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>(
            "template_spec");
    string error_message =
        "对于任何给定的模板特化，使用在特化中使用的模板参数的模板的显式实例化不应使程序格式错误";
    string path = misra::libtooling_utils::GetFilename(class_template,
                                                       result.SourceManager);
    int line =
        misra::libtooling_utils::GetLine(class_template, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_7_2);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_14_7_2
}  // namespace misra_cpp_2008
