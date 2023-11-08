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

#include "misra_cpp_2008/rule_5_0_4/libtooling/checker.h"

#include <glog/logging.h>

#include <climits>
#include <unordered_map>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace misra_cpp_2008 {
namespace rule_5_0_4 {
namespace libtooling {

void ReportError(string filename, int line,
                 analyzer::proto::ResultsList* results_list) {
  std::string error_message = "隐式整型转换不得改变底层类型的符号性";
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list, filename, line,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_0_4);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message,
                               filename, line);
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
  class QualTypeExt : public QualType {
   public:
    QualTypeExt(QualType qual_type) : QualType(qual_type){};
    bool override_unsigned = false;
    bool override_signed = false;
    bool error = false;
  };

 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // Case 1: when the expression is used as the argument when calling a
    // function that is declared with T2 as parameter
    finder->addMatcher(
        callExpr(unless(isExpansionInSystemHeader())).bind("func_call"), this);
    finder->addMatcher(
        cxxConstructExpr(unless(isExpansionInSystemHeader())).bind("ctor_call"),
        this);
    // Case 2: when the expression is used as an operand with an operator that
    // expects T2
    finder->addMatcher(
        binaryOperator(unless(isExpansionInSystemHeader())).bind("bo"), this);
    // Case 3: when initializing a new object of type T2, including return
    // statement in a function returning T2
    finder->addMatcher(varDecl(unless(isExpansionInSystemHeader()),
                               hasInitializer(expr().bind("init")))
                           .bind("var_decl"),
                       this);
    finder->addMatcher(
        returnStmt(unless(isExpansionInSystemHeader()),
                   hasAncestor(functionDecl().bind("ret_func_decl")))
            .bind("return"),
        this);
  }

  bool integral_signedness_differ(const QualTypeExt type1,
                                  const QualTypeExt type2) {
    if (type1.isNull() || type2.isNull()) return false;
    if (type1.error || type2.error) return true;
    bool type1_signed = (type1.override_signed ||
                         type1->isSignedIntegerOrEnumerationType()) &&
                        !type1.override_unsigned,
         type2_signed = (type2.override_signed ||
                         type2->isSignedIntegerOrEnumerationType()) &&
                        !type2.override_unsigned;
    return type1_signed != type2_signed;
  };

  QualTypeExt get_no_promotion_qual_type(const Expr* expr) {
    const Expr* pure_expr = expr->IgnoreImplicit()->IgnoreParens();
    int64_t id = expr->getID(*context_);
    auto it = qual_type_cache_.find(id);
    if (it != qual_type_cache_.end()) return it->second;
    const BinaryOperator* bo = dyn_cast<BinaryOperator>(pure_expr);
    if (!bo) return pure_expr->getType();
    StringRef op_code = bo->getOpcodeStr();
    if (op_code.equals(",")) return pure_expr->getType();
    QualTypeExt type = bo->IgnoreImplicit()->IgnoreParens()->getType();
    if (op_code.equals(">>") || op_code.equals("<<")) return type;
    const QualTypeExt LHS_type = get_no_promotion_qual_type(bo->getLHS());
    const QualTypeExt RHS_type = get_no_promotion_qual_type(bo->getRHS());
    if (integral_signedness_differ(LHS_type, RHS_type)) {
      type.error = true;
    } else if (LHS_type->isSignedIntegerOrEnumerationType() &&
               !LHS_type.override_unsigned) {
      type.override_signed = true;
    } else {
      type.override_unsigned = true;
    }
    qual_type_cache_.insert({id, type});
    return type;
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    auto report = [&result, this](const Stmt* stmt) {
      std::string filename =
          misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
      int line = misra::libtooling_utils::GetLine(stmt, result.SourceManager);
      ReportError(filename, line, this->results_list_);
    };
    context_ = result.Context;
    const CallExpr* func_call = result.Nodes.getNodeAs<CallExpr>("func_call");
    const CXXConstructExpr* ctor_call =
        result.Nodes.getNodeAs<CXXConstructExpr>("ctor_call");
    const BinaryOperator* bo = result.Nodes.getNodeAs<BinaryOperator>("bo");
    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("var_decl");
    const Expr* init = result.Nodes.getNodeAs<Expr>("init");
    const ReturnStmt* ret = result.Nodes.getNodeAs<ReturnStmt>("return");
    const FunctionDecl* ret_func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("ret_func_decl");
    if (func_call || ctor_call) {
      ArrayRef<ParmVarDecl*> parameters;
      if (func_call) {
        const FunctionDecl* func_decl = func_call->getDirectCallee();
        if (!func_decl) return;
        parameters = func_decl->parameters();
      } else if (ctor_call) {
        const CXXConstructorDecl* ctor_decl = ctor_call->getConstructor();
        if (!ctor_decl) return;
        parameters = ctor_decl->parameters();
      }
      const unsigned int num_args =
          func_call ? func_call->getNumArgs() : ctor_call->getNumArgs();
      unsigned int i = 0;
      for (const ParmVarDecl* param : parameters) {
        if (i >= num_args) {
          break;
        }
        const QualType param_type = param->getType();
        const QualTypeExt arg_type =
            func_call ? get_no_promotion_qual_type(func_call->getArg(i))
                      : get_no_promotion_qual_type(ctor_call->getArg(i));
        ++i;
        if (!integral_signedness_differ(param_type, arg_type)) continue;
        func_call ? report(func_call) : report(ctor_call);
      }
    } else if (bo) {
      StringRef op_code = bo->getOpcodeStr();
      // https://en.cppreference.com/w/cpp/language/operator_arithmetic
      const array<string, 11> arithmetic_operators{
          "=", "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>"};
      if (find(arithmetic_operators.begin(), arithmetic_operators.end(),
               op_code.str()) == arithmetic_operators.end())
        return;
      if (!bo->getLHS() || !bo->getRHS()) return;
      if (!integral_signedness_differ(get_no_promotion_qual_type(bo->getLHS()),
                                      get_no_promotion_qual_type(bo->getRHS())))
        return;
      report(bo);
    } else if (var_decl && init) {
      if (!integral_signedness_differ(var_decl->getType(),
                                      get_no_promotion_qual_type(init)))
        return;
      report(init);
    } else if (ret && ret->getRetValue() && ret_func_decl) {
      if (!integral_signedness_differ(
              ret_func_decl->getReturnType(),
              get_no_promotion_qual_type(ret->getRetValue())))
        return;
      report(ret);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  ASTContext* context_;
  unordered_map<int64_t, QualTypeExt> qual_type_cache_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_5_0_4
}  // namespace misra_cpp_2008
