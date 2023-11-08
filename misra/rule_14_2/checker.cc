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

#include "misra/rule_14_2/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

/*
 * Checker for MISRA_C_2012, rule 14.2, a forstmt should be well-formed.
 * Case 1, checked by FirstDefCallback:
    for(loop_counter_definition; second_expr; third_expr) { body_stmts }
    The loop_counter is the *only* modified referenced decl
     in loop_counter_definition. May raise ExprSetsMoreThanOneRefErr.
    Then run CheckSecondThirdBody with loop_counter.
 * Case 2, checked by FirstNotDefCallback:
    for(first_expr; second_expr; third_expr) { body_stmts }
    Firstly, first_expr should has no persistent side effect,
      except setting up loop_counter.
    The loop counter is the only modified referenced decl in third_expr.
    Then run CheckSecondThirdBody with loop_counter.
 * CheckSecondThirdBody
    second should have no side effect, may raise ExprHasSideEffectErr
    second should use loop_counter, may raise CounterNotUsedInExprErr
    second should have no invalid control flag, may raise ControlFlagErr
    third should have no side effect, except updating loop_counter,
      may raise ExprHasSideEffectErr, ExprSetsMoreThanOneRefErr
      and CounterNotUsedInExprErr
    body should not modify loop_counter, may raise CounterModifiedInBodyErr
    second and third should not use decls modified in body,
      may raise ExprUsesRefModifiedInBodyErr
 */

namespace {

// Whether an expr has non-bool sub-expr,
// such as "hello" in ((i < 3) && "hello")
bool HasNonBoolSubExpr(const Expr* expr, const ASTContext* context) {
  if (const ParenExpr* p = dyn_cast<ParenExpr>(expr)) {
    return HasNonBoolSubExpr(p->getSubExpr(), context);
  }
  if (const BinaryOperator* binary = dyn_cast<BinaryOperator>(expr)) {
    clang::BinaryOperatorKind opcode = binary->getOpcode();
    if (opcode == clang::BO_LE || opcode == clang::BO_LT ||
        opcode == clang::BO_GT || opcode == clang::BO_GE ||
        opcode == clang::BO_EQ) {
      return false;
    }
    if (opcode != clang::BO_LAnd && opcode != clang::BO_LOr) return true;
    return HasNonBoolSubExpr(binary->getLHS(), context) ||
           HasNonBoolSubExpr(binary->getRHS(), context);
  }
  if (const UnaryOperator* unary = dyn_cast<UnaryOperator>(expr)) {
    clang::UnaryOperatorKind opcode = unary->getOpcode();
    if (opcode != clang::UO_LNot) return true;
    return HasNonBoolSubExpr(unary->getSubExpr(), context);
  }
  return misra::libtooling_utils::GetEssentialTypeCategory(expr, context) !=
         misra::libtooling_utils::kBoolean;
}

// TODO: For now all functions are impure
bool HasImpureFunctionCall(const Stmt* expr) {
  if (const CallExpr* callexpr = dyn_cast<CallExpr>(expr)) {
    return true;
  } else if (const UnaryOperator* unary = dyn_cast<UnaryOperator>(expr)) {
    return HasImpureFunctionCall(unary->getSubExpr());
  } else if (const BinaryOperator* binary = dyn_cast<BinaryOperator>(expr)) {
    return HasImpureFunctionCall(binary->getLHS()) ||
           HasImpureFunctionCall(binary->getRHS());
  } else if (const ImplicitCastExpr* implicit_cast =
                 dyn_cast<ImplicitCastExpr>(expr)) {
    return HasImpureFunctionCall(implicit_cast->getSubExpr());
  } else if (const ParenExpr* paren = dyn_cast<ParenExpr>(expr)) {
    return HasImpureFunctionCall(paren->getSubExpr());
  }
  return false;
}

// Find all reference of decls modified in stmt, and insert them into decls
void GetModifiedRefDeclsInStmt(const Stmt* stmt, std::set<const Decl*>* decls) {
  if (const UnaryOperator* unary_op = dyn_cast<UnaryOperator>(stmt)) {
    auto opcode = unary_op->getOpcode();
    // If a unary expression is not ++ or -- prefiexed/postfixed,
    // or it is shaped like (a+b)++, it modifies nothing
    if (opcode != clang::UO_PreDec && opcode != clang::UO_PreInc &&
        opcode != clang::UO_PostDec && opcode != clang::UO_PostInc) {
      return;
    } else if (auto decl_ref = dyn_cast<DeclRefExpr>(unary_op->getSubExpr())) {
      decls->insert(decl_ref->getDecl());
    }
  } else if (const BinaryOperator* binary_op = dyn_cast<BinaryOperator>(stmt)) {
    if (binary_op->getOpcode() == clang::BO_Comma) {
      GetModifiedRefDeclsInStmt(binary_op->getLHS(), decls);
      GetModifiedRefDeclsInStmt(binary_op->getRHS(), decls);
    } else if (!binary_op->isAssignmentOp()) {
      return;
    } else if (auto decl_ref = dyn_cast<DeclRefExpr>(binary_op->getLHS())) {
      decls->insert(decl_ref->getDecl());
    }
  } else if (const CompoundStmt* compound = dyn_cast<CompoundStmt>(stmt)) {
    for (CompoundStmt::const_body_iterator iter = compound->body_begin();
         iter != compound->body_end(); iter++) {
      GetModifiedRefDeclsInStmt(*iter, decls);
    }
  }
}

// Whether decl is used in expr
bool DeclUsedInExpr(const Expr* expr, const Decl* d) {
  if (const DeclRefExpr* ref_expr = dyn_cast<DeclRefExpr>(expr)) {
    return ref_expr->getDecl() == d;
  } else if (const UnaryOperator* unary = dyn_cast<UnaryOperator>(expr)) {
    return DeclUsedInExpr(unary->getSubExpr(), d);
  } else if (const BinaryOperator* binary = dyn_cast<BinaryOperator>(expr)) {
    return DeclUsedInExpr(binary->getLHS(), d) ||
           DeclUsedInExpr(binary->getRHS(), d);
  } else if (const ImplicitCastExpr* implicit_cast =
                 dyn_cast<ImplicitCastExpr>(expr)) {
    return DeclUsedInExpr(implicit_cast->getSubExpr(), d);
  } else if (const ParenExpr* paren = dyn_cast<ParenExpr>(expr)) {
    return DeclUsedInExpr(paren->getSubExpr(), d);
  }
  return false;
}

// Given decls, find one used in expr
const Decl* GetFirstDeclUsedInExpr(const Expr* expr,
                                   std::set<const Decl*>* decls) {
  if (const DeclRefExpr* ref_expr = dyn_cast<DeclRefExpr>(expr)) {
    auto decl = ref_expr->getDecl();
    if (decls->count(decl) > 0) return decl;
  } else if (const UnaryOperator* unary = dyn_cast<UnaryOperator>(expr)) {
    return GetFirstDeclUsedInExpr(unary->getSubExpr(), decls);
  } else if (const BinaryOperator* binary = dyn_cast<BinaryOperator>(expr)) {
    auto lhs_result = GetFirstDeclUsedInExpr(binary->getLHS(), decls);
    if (lhs_result != NULL) {
      return lhs_result;
    }
    return GetFirstDeclUsedInExpr(binary->getRHS(), decls);
  } else if (const ImplicitCastExpr* implicit_cast =
                 dyn_cast<ImplicitCastExpr>(expr)) {
    return GetFirstDeclUsedInExpr(implicit_cast->getSubExpr(), decls);
  } else if (const ParenExpr* paren = dyn_cast<ParenExpr>(expr)) {
    return GetFirstDeclUsedInExpr(paren->getSubExpr(), decls);
  }
  return NULL;
}

}  // namespace

namespace {

void ExprHasSideEffectErr(string path, int linenum, string which_expr,
                          ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1703][misra-c2012-14.2]: %s clause in for loop may have persistent side effect",
      which_expr);
  LOG(INFO) << error_message;
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_14_2_MAY_HAVE_PSE);
  result->set_which_expr(which_expr);
}

void CounterNotUsedInExprErr(string path, int linenum, string counter_name,
                             string which_expr, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1703][misra-c2012-14.2]: loop counter '%s' is not used in %s clause",
      counter_name, which_expr);
  LOG(INFO) << error_message;
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_14_2_COUNTER_NOT_USED);
  result->set_counter_name(counter_name);
  result->set_which_expr(which_expr);
}

void CounterModifiedInBodyErr(string path, int linenum, string counter_name,
                              ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1703][misra-c2012-14.2]: loop counter '%s' is modified in loop body",
      counter_name);
  LOG(INFO) << error_message;
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_14_2_COUNTER_MODIFIED_IN_BODY);
  result->set_counter_name(counter_name);
}

void ExprUsesRefModifiedInBodyErr(string path, int linenum, string which_expr,
                                  string ref_name, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1703][misra-c2012-14.2]: %s clause uses reference '%s' modified in loop body",
      which_expr, ref_name);
  LOG(INFO) << error_message;
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_14_2_REF_MODIFIED_IN_BODY);
  result->set_which_expr(which_expr);
  result->set_ref_name(ref_name);
}

void ExprSetsNotOneRefErr(string path, int linenum, string which_expr,
                          ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1703][misra-c2012-14.2]: %s clause should set and only set the value of loop counter",
      which_expr);
  LOG(INFO) << error_message;
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_14_2_CLAUSE_SET_OTHER_VALUE);
  result->set_which_expr(which_expr);
}

void ControlFlagErr(string path, int linenum, ResultsList* results_list) {
  string error_message =
      "[C1703][misra-c2012-14.2]: second clause uses non-boolean control flag";
  LOG(INFO) << error_message;
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_14_2_2ND_CLAUSE_USE_NON_BOOL);
}

void CheckSecondThirdBody(const Decl* loop_counter, const ASTContext* context,
                          SourceManager* source_manager, const Expr* second,
                          const Expr* third, const Stmt* body,
                          ResultsList* results_list) {
  const NamedDecl* named_loop_counter = dyn_cast<NamedDecl>(loop_counter);
  if (second->HasSideEffects(*context) || HasImpureFunctionCall(second)) {
    ExprHasSideEffectErr(
        misra::libtooling_utils::GetFilename(second, source_manager),
        misra::libtooling_utils::GetLine(second, source_manager), "second",
        results_list);
    return;
  }
  if (!DeclUsedInExpr(second, loop_counter)) {
    CounterNotUsedInExprErr(
        misra::libtooling_utils::GetFilename(second, source_manager),
        misra::libtooling_utils::GetLine(second, source_manager),
        named_loop_counter->getNameAsString(), "second", results_list);
    return;
  }
  if (HasNonBoolSubExpr(second, context)) {
    ControlFlagErr(misra::libtooling_utils::GetFilename(second, source_manager),
                   misra::libtooling_utils::GetLine(second, source_manager),
                   results_list);
    return;
  }
  if (HasImpureFunctionCall(third)) {
    ExprHasSideEffectErr(
        misra::libtooling_utils::GetFilename(second, source_manager),
        misra::libtooling_utils::GetLine(second, source_manager), "third",
        results_list);
    return;
  }
  std::set<const Decl*> declsModifiedInThird;
  GetModifiedRefDeclsInStmt(third, &declsModifiedInThird);
  if (declsModifiedInThird.size() != 1) {
    ExprSetsNotOneRefErr(
        misra::libtooling_utils::GetFilename(third, source_manager),
        misra::libtooling_utils::GetLine(third, source_manager), "third",
        results_list);
    return;
  }
  if (*declsModifiedInThird.begin() != loop_counter) {
    CounterNotUsedInExprErr(
        misra::libtooling_utils::GetFilename(third, source_manager),
        misra::libtooling_utils::GetLine(third, source_manager),
        named_loop_counter->getNameAsString(), "third", results_list);
    return;
  }
  std::set<const Decl*> declsModifiedInBody;
  GetModifiedRefDeclsInStmt(body, &declsModifiedInBody);
  if (declsModifiedInBody.count(loop_counter) > 0) {
    CounterModifiedInBodyErr(
        misra::libtooling_utils::GetFilename(body, source_manager),
        misra::libtooling_utils::GetLine(body, source_manager),
        named_loop_counter->getNameAsString(), results_list);
    return;
  }
  auto declUsedInSecond = GetFirstDeclUsedInExpr(second, &declsModifiedInBody);
  if (declUsedInSecond != NULL) {
    auto namedDecl = dyn_cast<NamedDecl>(declUsedInSecond);
    ExprUsesRefModifiedInBodyErr(
        misra::libtooling_utils::GetFilename(second, source_manager),
        misra::libtooling_utils::GetLine(second, source_manager), "second",
        namedDecl->getNameAsString(), results_list);
    return;
  }
  auto declUsedInThird = GetFirstDeclUsedInExpr(third, &declsModifiedInBody);
  if (declUsedInThird != NULL) {
    auto namedDecl = dyn_cast<NamedDecl>(declUsedInThird);
    ExprUsesRefModifiedInBodyErr(
        misra::libtooling_utils::GetFilename(third, source_manager),
        misra::libtooling_utils::GetLine(third, source_manager), "third",
        namedDecl->getNameAsString(), results_list);
    return;
  }
}

}  // namespace

namespace misra {
namespace rule_14_2 {

class FirstNotDefCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(forStmt(unless(hasLoopInit(declStmt())),
                               hasCondition(expr().bind("second")),
                               hasIncrement(expr().bind("third")),
                               hasBody(stmt().bind("body")))
                           .bind("root"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const ForStmt* root = result.Nodes.getNodeAs<ForStmt>("root");
    const Expr* second = result.Nodes.getNodeAs<Expr>("second");
    const Expr* third = result.Nodes.getNodeAs<Expr>("third");
    const Stmt* body = result.Nodes.getNodeAs<Stmt>("body");
    const ASTContext* context = result.Context;

    // skip system header
    if (libtooling_utils::IsInSystemHeader(root, result.Context)) {
      return;
    }
    auto first = root->getInit();
    if (first == NULL) return;
    if (HasImpureFunctionCall(first)) {
      ExprHasSideEffectErr(
          misra::libtooling_utils::GetFilename(first, result.SourceManager),
          misra::libtooling_utils::GetLine(first, result.SourceManager),
          "first", results_list_);
      return;
    }

    std::set<const Decl*> declsModifiedInThird;
    GetModifiedRefDeclsInStmt(third, &declsModifiedInThird);
    if (declsModifiedInThird.size() != 1) {
      ExprSetsNotOneRefErr(
          misra::libtooling_utils::GetFilename(third, result.SourceManager),
          misra::libtooling_utils::GetLine(third, result.SourceManager),
          "third", results_list_);
      return;
    }

    const Decl* loop_counter = *declsModifiedInThird.begin();
    CheckSecondThirdBody(loop_counter, context, result.SourceManager, second,
                         third, body, results_list_);
  }

 private:
  ResultsList* results_list_;
};

class FirstDefCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(forStmt(hasLoopInit(declStmt().bind("first")),
                               hasCondition(expr().bind("second")),
                               hasIncrement(expr().bind("third")),
                               hasBody(stmt().bind("body"))),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const DeclStmt* first = result.Nodes.getNodeAs<DeclStmt>("first");
    const Expr* second = result.Nodes.getNodeAs<Expr>("second");
    const Expr* third = result.Nodes.getNodeAs<Expr>("third");
    const Stmt* body = result.Nodes.getNodeAs<Stmt>("body");
    const ASTContext* context = result.Context;
    // skip system header
    if (libtooling_utils::IsInSystemHeader(first, result.Context)) {
      return;
    }
    if (!first->isSingleDecl()) {
      ExprSetsNotOneRefErr(
          misra::libtooling_utils::GetFilename(first, result.SourceManager),
          misra::libtooling_utils::GetLine(first, result.SourceManager),
          "first", results_list_);
      return;
    }
    const Decl* loop_counter = first->getSingleDecl();
    CheckSecondThirdBody(loop_counter, context, result.SourceManager, second,
                         third, body, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  def_callback_ = new FirstDefCallback;
  def_callback_->Init(results_list_, &finder_);
  nodef_callback_ = new FirstNotDefCallback;
  nodef_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_14_2
}  // namespace misra
