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

#include "misra/rule_11_4/checker.h"

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

void ReportError(std::string name, bool ifIntToPointer, QualType destination,
                 QualType source, string loc, std::string path, int line_number,
                 ResultsList* results_list) {
  std::string error_message;
  analyzer::proto::Result* pb_result;
  if (ifIntToPointer) {
    error_message = absl::StrFormat(
        "[C1406][misra-c2012-11.4]: Conversions violation of misra-c2012-11.4\n"
        "source type: %s\n"
        "destination pointer object type: %s\n"
        "Location: %s",
        source.getAsString(), destination.getAsString(), loc);
    pb_result =
        AddResultToResultsList(results_list, path, line_number, error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_RULE_11_4_INT_TO_POINTER);
  } else {
    error_message = absl::StrFormat(
        "[C1406][misra-c2012-11.4]: Conversions violation of misra-c2012-11.4\n"
        "source pointer object type: %s\n"
        "destination type: %s\n"
        "Location: %s",
        source.getAsString(), destination.getAsString(), loc);
    pb_result =
        AddResultToResultsList(results_list, path, line_number, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_11_4);
  }
  pb_result->set_source_type(source.getAsString());
  pb_result->set_destination_type(destination.getAsString());
  pb_result->set_loc(loc);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_11_4 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        castExpr(
            hasSourceExpression(hasType(isInteger())),
            anyOf(explicitCastExpr(hasDestinationType(pointerType())),
                  implicitCastExpr(hasImplicitDestinationType(pointerType()))))
            .bind("intToPointer"),
        this);
    finder->addMatcher(
        castExpr(
            hasSourceExpression(hasType(pointerType())),
            anyOf(explicitCastExpr(hasDestinationType(isInteger())),
                  implicitCastExpr(hasImplicitDestinationType(isInteger()))))
            .bind("pointerToInt"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CastExpr* intToPointer =
        result.Nodes.getNodeAs<CastExpr>("intToPointer");
    const CastExpr* pointerToint =
        result.Nodes.getNodeAs<CastExpr>("pointerToInt");
    bool ifIntToPointer = intToPointer ? true : false;
    auto ce = intToPointer ? intToPointer : pointerToint;
    assert(ce);

    // skip system header
    if (libtooling_utils::IsInSystemHeader(ce, result.Context)) {
      return;
    }
    ASTContext* context = result.Context;
    std::string path = libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number = libtooling_utils::GetLine(ce, result.SourceManager);
    QualType destination_type =
        ifIntToPointer ? ce->getType()->getPointeeType() : ce->getType();
    QualType source_type = ifIntToPointer
                               ? ce->getSubExpr()->getType()
                               : ce->getSubExpr()->getType()->getPointeeType();
    if (ifIntToPointer && ce->getSubExpr()->isNullPointerConstant(
                              *context, Expr::NPC_ValueDependentIsNotNull) ==
                              Expr::NPCK_ZeroLiteral) {
      // Exception NULL
      // The can not only matches ((void*)0), but also other NULL types.
      return;
    }
    std::string source_name = libtooling_utils::GetExprName(
        ce->getSubExpr(), result.SourceManager, context);
    ReportError(source_name, ifIntToPointer, destination_type, source_type,
                libtooling_utils::GetLocation(ce, result.SourceManager), path,
                line_number, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new CastCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_11_4
}  // namespace misra
