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

#include "misra_cpp_2008/rule_5_2_8/libtooling/checker.h"

#include <glog/logging.h>

#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "整数类型或void指针类型的对象不得转换为指针类型的对象";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_2_8 {
namespace libtooling {
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        castExpr(hasSourceExpression(hasType(pointsTo(voidType()))),
                 anyOf(explicitCastExpr(
                           hasDestinationType(unless(pointsTo(voidType())))),
                       implicitCastExpr(hasImplicitDestinationType(
                           unless(pointsTo(voidType()))))))
            .bind("ce"),
        this);
    finder->addMatcher(
        castExpr(
            hasSourceExpression(hasType(isInteger())),
            anyOf(explicitCastExpr(hasDestinationType(pointerType())),
                  implicitCastExpr(hasImplicitDestinationType(pointerType()))))
            .bind("ce"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CastExpr* ce = result.Nodes.getNodeAs<CastExpr>("ce");
    ASTContext* context = result.Context;
    // skip system header
    if (misra::libtooling_utils::IsInSystemHeader(ce, context)) {
      return;
    }
    std::string path =
        misra::libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(ce, result.SourceManager);
    ReportError(path, line_number, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}
}  // namespace libtooling
}  // namespace rule_5_2_8
}  // namespace misra_cpp_2008
