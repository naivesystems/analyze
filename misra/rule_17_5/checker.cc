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

#include "misra/rule_17_5/checker.h"

#include <glog/logging.h>

#include "misra/libtooling_utils/libtooling_utils.h"

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace rule_17_5 {

class CallExprCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    // TODO: cannot handle arg like expr/pointer to an array
    finder->addMatcher(callExpr(forEachArgumentWithParam(
                           declRefExpr(hasType(arrayType())).bind("arg_expr"),
                           parmVarDecl().bind("parm_decl"))),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const DeclRefExpr* arg_expr =
        result.Nodes.getNodeAs<DeclRefExpr>("arg_expr");
    const ParmVarDecl* parm_decl =
        result.Nodes.getNodeAs<ParmVarDecl>("parm_decl");
    ASTContext* context = result.Context;

    const ConstantArrayType* parm_array_type =
        context->getAsConstantArrayType(parm_decl->getOriginalType());
    if (!parm_array_type) {
      return;
    }
    if (arg_expr->isNullPointerConstant(*context,
                                        Expr::NPC_NeverValueDependent)) {
      ReportNullPointerError(arg_expr, result.SourceManager);
    }

    const ConstantArrayType* arg_array_type = dyn_cast<ConstantArrayType>(
        context->getAsConstantArrayType(arg_expr->getType()));
    if (!arg_array_type) {
      return;
    }

    if (arg_array_type->getSize().ult(parm_array_type->getSize())) {
      ReportArraySizeError(arg_expr, result.SourceManager);
    }
  }

 private:
  ResultsList* results_list_;

  void ReportNullPointerError(const Expr* expr, SourceManager* source_manager) {
    string error_message =
        "[C1504][misra-c2012-17.5]: null pointer argument used for parameter with array type";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_, libtooling_utils::GetFilename(expr, source_manager),
        libtooling_utils::GetLine(expr, source_manager), error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_RULE_17_5_ARRAY_SIZE_ERROR);
    LOG(INFO) << error_message;
  }

  void ReportArraySizeError(const Expr* expr, SourceManager* source_manager) {
    string error_message =
        "[C1504][misra-c2012-17.5]: argument with improper array size used for parameter with array type";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_, libtooling_utils::GetFilename(expr, source_manager),
        libtooling_utils::GetLine(expr, source_manager), error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_RULE_17_5_ARRAY_SIZE_ERROR);
    LOG(INFO) << error_message;
  }
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new CallExprCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_17_5
}  // namespace misra
