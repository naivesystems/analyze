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

#include "misra/rule_11_1/checker.h"

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

void ReportError(string name, QualType destination, QualType source, string loc,
                 string path, int line_number, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1409][misra-c2012-11.1]: Conversions violation of misra-c2012-11.1\n"
      "source type: %s\n"
      "destination type: %s\n"
      "Location: %s",
      source.getAsString(), destination.getAsString(), loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_11_1);
  pb_result->set_source_type(source.getAsString());
  pb_result->set_destination_type(destination.getAsString());
  pb_result->set_loc(loc);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_11_1 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto PointerToFunctionType = pointsTo(parenType(innerType(functionType())));
    finder->addMatcher(
        castExpr(hasSourceExpression(hasType(PointerToFunctionType)))
            .bind("typeFromFuncPointer"),
        this);
    finder->addMatcher(
        castExpr(
            anyOf(explicitCastExpr(hasDestinationType(PointerToFunctionType)),
                  implicitCastExpr(
                      hasImplicitDestinationType(PointerToFunctionType))))
            .bind("typeToFuncPointer"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CastExpr* fromCast =
        result.Nodes.getNodeAs<CastExpr>("typeFromFuncPointer");
    const CastExpr* toCast =
        result.Nodes.getNodeAs<CastExpr>("typeToFuncPointer");
    auto ce = fromCast ? fromCast : toCast;
    assert(ce);

    ASTContext* context = result.Context;
    string path = libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number = libtooling_utils::GetLine(ce, result.SourceManager);
    QualType destination_type = ce->getType().getCanonicalType();
    QualType source_type = ce->getSubExpr()->getType().getCanonicalType();
    // skip system header
    if (libtooling_utils::IsInSystemHeader(ce, context)) {
      return;
    }
    if (source_type == destination_type) {
      // Pass the same type (usually Lvalue to Rvalue)
      return;
    }
    if (toCast && ce->getSubExpr()->isNullPointerConstant(
                      *context, Expr::NPC_ValueDependentIsNotNull) ==
                      Expr::NPCK_ZeroLiteral) {
      // Exception 1
      return;
    }
    if (fromCast && destination_type->isVoidType()) {
      // Exception 2
      return;
    }
    if (toCast && source_type->isFunctionType()) {
      // Exception 3
      if (destination_type->getPointeeType().getCanonicalType() ==
          source_type) {
        return;
      }
    }
    string source_name = libtooling_utils::GetExprName(
        ce->getSubExpr(), result.SourceManager, context);
    ReportError(source_name, destination_type, source_type,
                libtooling_utils::GetLocation(ce, result.SourceManager), path,
                line_number, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new CastCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_11_1
}  // namespace misra
