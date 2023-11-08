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

#include "googlecpp/g1186/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace googlecpp {
namespace g1186 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // can not handle situations that the parameter is changed as parameter to
    // another function, e.g.
    // void bar(int& p) { ++p; }
    // void foo(int& p) { bar(p); }
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("func_decl"),
        this);
    finder->addMatcher(
        binaryOperator(
            unless(isExpansionInSystemHeader()), isAssignmentOperator(),
            hasLHS(anyOf(unaryOperator(hasOperatorName("*"),
                                       hasUnaryOperand(hasDescendant(
                                           declRefExpr().bind("binary_lhs"))))
                             .bind("deref_op"),
                         declRefExpr().bind("binary_lhs"))),
            hasAncestor(functionDecl().bind("func"))),
        this);
    finder->addMatcher(
        unaryOperator(unless(isExpansionInSystemHeader()),
                      anyOf(hasOperatorName("++"), hasOperatorName("--")),
                      hasDescendant(declRefExpr().bind("unary_lhs")),
                      hasAncestor(functionDecl().bind("func"))),
        this);
    finder->addMatcher(
        cxxMemberCallExpr(unless(isExpansionInSystemHeader()),
                          callee(cxxMethodDecl(unless(isConst()))),
                          has(memberExpr(hasDescendant(
                              declRefExpr().bind("deref_member_call")))),
                          hasAncestor(functionDecl().bind("func"))),
        this);
  }

  vector<ParamInfo>& EnsureParamInfo(FunctionDecl const* const func,
                                     SourceManager* SM) {
    FuncInfo func_info = {
        .id = func->getNameInfo().getLoc().getHashValue(),
        .path = misra::libtooling_utils::GetFilename(func, SM),
        .line_number = misra::libtooling_utils::GetLine(func, SM),
    };
    vector<ParamInfo>& param_infos = func_info_2_param_infos[func_info];
    if (param_infos.size() == 0) {
      for (unsigned int i = 0; i < func->getNumParams() - func->isVariadic();
           i++) {
        ParmVarDecl const* const param = func->getParamDecl(i);
        QualType const& type = param->getType();
        param_infos.push_back({
            .name = param->getNameAsString(),
            .is_pointer_ty = type->isPointerType(),
            .is_value_or_const_reference =
                !type->isPointerType() &&
                (type->isFundamentalType() ||
                 (type->isReferenceType() &&
                  type.getNonReferenceType().isConstQualified())),
            .can_be_output = type->isReferenceType() || type->isPointerType(),
            .is_output = false,
        });
      }
    }
    return param_infos;
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    FunctionDecl const* const func =
        result.Nodes.getNodeAs<FunctionDecl>("func");
    DeclRefExpr const* const binary_lhs =
        result.Nodes.getNodeAs<DeclRefExpr>("binary_lhs");
    DeclRefExpr const* const unary_lhs =
        result.Nodes.getNodeAs<DeclRefExpr>("unary_lhs");
    FunctionDecl const* const func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl");
    UnaryOperator const* const deref_op =
        result.Nodes.getNodeAs<UnaryOperator>("deref_op");
    DeclRefExpr const* const deref_member_call =
        result.Nodes.getNodeAs<DeclRefExpr>("deref_member_call");
    auto* SM = result.SourceManager;
    if (func_decl) EnsureParamInfo(func_decl, SM);
    if (!func || (!binary_lhs && !unary_lhs && !deref_member_call)) return;
    vector<ParamInfo>& param_infos = EnsureParamInfo(func, SM);
    DeclRefExpr const* const lhs =
        binary_lhs ? binary_lhs
                   : (deref_member_call ? deref_member_call : unary_lhs);
    string const& lhs_name = lhs->getNameInfo().getName().getAsString();
    auto const& is_param_info = [&lhs_name](ParamInfo const& param_info) {
      return param_info.name == lhs_name;
    };
    vector<ParamInfo>::iterator const& find_result =
        std::find_if(param_infos.begin(), param_infos.end(), is_param_info);
    if (find_result != param_infos.end() && find_result->can_be_output) {
      if (binary_lhs) {
        if (!deref_op) {  // reference
          find_result->is_output = !find_result->is_pointer_ty;
        } else {  // pointer
          find_result->is_output = find_result->is_pointer_ty;
        }
      } else if (unary_lhs) {
        find_result->is_output = true;
      } else if (deref_member_call) {
        find_result->is_output = true;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1186
}  // namespace googlecpp
