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

#include "misra_cpp_2008/rule_5_0_12/libtooling/checker.h"

#include <glog/logging.h>

#include <climits>
#include <unordered_map>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace std::literals::string_literals;
using namespace clang;
using namespace clang::ast_matchers;

namespace misra_cpp_2008 {
namespace rule_5_0_12 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        implicitCastExpr(
            allOf(hasType(isInteger()),
                  hasSourceExpression(
                      expr(hasType(isAnyCharacter())).bind("expr"))),
            unless(hasParent(castExpr())))
            .bind("cast"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    ImplicitCastExpr const* cast =
        result.Nodes.getNodeAs<ImplicitCastExpr>("cast");

    if (!cast) {
      return;
    }

    // skip the system header file
    if (misra::libtooling_utils::IsInSystemHeader(cast, result.Context)) {
      return;
    }

    QualType targetType =
        cast->getType().getDesugaredType(*context).getUnqualifiedType();
    std::string typeString = targetType.getAsString();

    if (typeString != "signed char"s && typeString != "unsigned char"s) {
      return;
    }

    string error_message =
        "有符号char和无符号char类型只得用于数字值的存储和使用";
    string path =
        misra::libtooling_utils::GetFilename(cast, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(cast, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_0_12);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_5_0_12
}  // namespace misra_cpp_2008
