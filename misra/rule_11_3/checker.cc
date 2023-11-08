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

#include "misra/rule_11_3/checker.h"

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
                 string loc, std::string path, int line_number,
                 ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C1407][misra-c2012-11.3]: Conversions violation of misra-c2012-11.3\n"
      "source pointer object type: %s\n"
      "destination pointer object type: %s\n"
      "Location: %s",
      source.getAsString(), destination.getAsString(), loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_11_3);
  pb_result->set_source_type(source.getAsString());
  pb_result->set_destination_type(destination.getAsString());
  pb_result->set_loc(loc);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_11_3 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        castExpr(
            // pointer to object type is not pointer to void type
            unless(hasSourceExpression(hasType(pointsTo(voidType())))),
            hasSourceExpression(hasType(pointerType())),
            anyOf(explicitCastExpr(hasDestinationType(pointerType())),
                  implicitCastExpr(hasImplicitDestinationType(pointerType()))))
            .bind("ce"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CastExpr* ce = result.Nodes.getNodeAs<CastExpr>("ce");
    // skip system header
    if (libtooling_utils::IsInSystemHeader(ce, result.Context)) {
      return;
    }
    ASTContext* context = result.Context;
    std::string path = libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number = libtooling_utils::GetLine(ce, result.SourceManager);

    QualType destination_type = ce->getType()->getPointeeType();
    QualType source_type = ce->getSubExpr()->getType()->getPointeeType();
    QualType base_dtype =
        destination_type.getCanonicalType().getUnqualifiedType();
    QualType base_stype = source_type.getCanonicalType().getUnqualifiedType();
    if (ce->getSubExpr()->isNullPointerConstant(
            *context, Expr::NPC_ValueDependentIsNotNull) ==
        Expr::NPCK_ZeroLiteral) {
      // Exception NULL
      // The can not only matches ((void*)0), but also other NULL types.
      return;
    }
    if (base_dtype != base_stype) {
      if (!base_dtype->isCharType()) {
        std::string source_name = libtooling_utils::GetExprName(
            ce->getSubExpr(), result.SourceManager, context);
        ReportError(source_name, destination_type, source_type,
                    libtooling_utils::GetLocation(ce, result.SourceManager),
                    path, line_number, results_list_);
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

}  // namespace rule_11_3
}  // namespace misra
