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

#include "misra/rule_21_18/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(QualType destination, QualType source, string loc,
                 std::string path, int line_number, ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0403][misra-c2012-21.18]: size_t value invalid as function argument.\n"
      "source pointer object type: %s\n"
      "destination object type: %s\n"
      "Location: %s",
      source.getAsString(), destination.getAsString(), loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_source_type(source.getAsString());
  pb_result->set_destination_type(destination.getAsString());
  pb_result->set_loc(loc);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_21_18);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_21_18 {

class SizetCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto calleeMatcher = callee(functionDecl(hasName("strxfrm")));
    auto argumentMatcher = hasArgument(2, expr().bind("x"));
    finder->addMatcher(callExpr(calleeMatcher, argumentMatcher), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;

    const Expr* third_arg = result.Nodes.getNodeAs<Expr>("x");
    if (third_arg) {
      // skip system header
      if (libtooling_utils::IsInSystemHeader(third_arg, result.Context)) {
        return;
      }
      if (third_arg->getType()->isIntegerType()) {
        clang::Expr::EvalResult rint;
        third_arg->EvaluateAsInt(rint, *context);
        if (rint.Val.isInt() && rint.Val.getInt() < 0) {
          std::string path =
              libtooling_utils::GetFilename(third_arg, result.SourceManager);
          int line_number =
              libtooling_utils::GetLine(third_arg, result.SourceManager);
          ReportError(
              third_arg->getType(), third_arg->getType(),
              libtooling_utils::GetLocation(third_arg, result.SourceManager),
              path, line_number, results_list_);
        }
      }
      return;
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new SizetCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_21_18
}  // namespace misra
