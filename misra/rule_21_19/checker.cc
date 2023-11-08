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

#include "misra/rule_21_19/checker.h"

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
      "[C0402][misra-c2012-21.19]: the return value of function is assigned to non-const qualified type\n"
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
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_21_19);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_21_19 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto calleeMatcher = callee(functionDecl(
        hasAnyName("localeconv", "getenv", "setlocale", "strerror")));
    finder->addMatcher(
        castExpr(hasSourceExpression(callExpr(calleeMatcher))).bind("withCast"),
        this);
    finder->addMatcher(
        traverse(TK_AsIs, callExpr(calleeMatcher, unless(hasParent(castExpr())))
                              .bind("withoutCast")),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;

    const CallExpr* withoutCast =
        result.Nodes.getNodeAs<CallExpr>("withoutCast");
    if (withoutCast) {
      // skip system header
      if (libtooling_utils::IsInSystemHeader(withoutCast, result.Context)) {
        return;
      }
      std::string path =
          libtooling_utils::GetFilename(withoutCast, result.SourceManager);
      int line_number =
          libtooling_utils::GetLine(withoutCast, result.SourceManager);
      ReportError(
          withoutCast->getType(), withoutCast->getType(),
          libtooling_utils::GetLocation(withoutCast, result.SourceManager),
          path, line_number, results_list_);
      return;
    }

    const CastExpr* withCast = result.Nodes.getNodeAs<CastExpr>("withCast");
    assert(withCast);
    // skip system header
    if (libtooling_utils::IsInSystemHeader(withCast, result.Context)) {
      return;
    }

    std::string path =
        libtooling_utils::GetFilename(withCast, result.SourceManager);
    int line_number = libtooling_utils::GetLine(withCast, result.SourceManager);
    std::string location = libtooling_utils::GetLocation(withCast->getSubExpr(),
                                                         result.SourceManager);
    QualType destination_type = withCast->getType();
    QualType source_type = withCast->getSubExpr()->getType();
    if (destination_type->isVoidType()) {
      // ignore cast to void type
      return;
    }
    if (destination_type->isPointerType()) {
      if (!destination_type->getPointeeType().isConstQualified()) {
        ReportError(destination_type, source_type, location, path, line_number,
                    results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new CastCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_21_19
}  // namespace misra
