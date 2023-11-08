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

#include "misra/rule_11_8/checker.h"

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

void ReportError(std::string name, QualType destination, QualType source,
                 std::string path, int line_number, ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C1402][misra-c2012-11.8]: Conversions violation of misra-c2012-11.8\n"
      "source pointer object type: %s\n"
      "destination pointer object type: %s",
      source.getAsString(), destination.getAsString());
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_11_8);
  pb_result->set_source_type(source.getAsString());
  pb_result->set_destination_type(destination.getAsString());
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_11_8 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto pointer_or_reference = anyOf(pointerType(), referenceType());
    finder->addMatcher(
        castExpr(
            hasSourceExpression(
                anyOf(hasType(pointerType()), hasType(referenceType()))),
            anyOf(explicitCastExpr(hasDestinationType(pointer_or_reference)),
                  implicitCastExpr(
                      hasImplicitDestinationType(pointer_or_reference))))
            .bind("ce"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CastExpr* ce = result.Nodes.getNodeAs<CastExpr>("ce");
    ASTContext* context = result.Context;
    // skip system header
    if (libtooling_utils::IsInSystemHeader(ce, context)) {
      return;
    }

    std::string path = libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number = libtooling_utils::GetLine(ce, result.SourceManager);

    std::string source_name = libtooling_utils::GetExprName(
        ce->getSubExpr(), result.SourceManager, context);
    QualType destination_type = ce->getType()->getPointeeType();
    QualType source_type = ce->getSubExpr()->getType()->getPointeeType();
    if (source_type.isVolatileQualified() &&
        !destination_type.isVolatileQualified()) {
      ReportError(source_name, destination_type, source_type, path, line_number,
                  results_list_);
    }
    if (source_type.isConstQualified() &&
        !destination_type.isConstQualified()) {
      ReportError(source_name, destination_type, source_type, path, line_number,
                  results_list_);
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

}  // namespace rule_11_8
}  // namespace misra
