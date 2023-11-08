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

#include "misra/rule_13_2/checker.h"

#include <glog/logging.h>

#include <cstdint>
#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {
struct Loc {
  int line;
  int count;
  string begin_loc;
};
void ReportError(string filename, int line, ResultsList* results_list) {
  std::string error_message =
      "[C1605][misra-c2012-13.2]: multiple related functions should not be called in the same expression";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, filename, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_13_2);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message,
                               filename, line);
}
}  // namespace

namespace misra {
namespace rule_13_2 {

/**
 * This checker will report when we found at least two function calls
 * in one full expr
 **/
class OperatorSideEffectCallback : public MatchFinder::MatchCallback {
 public:
  void Init(bool aggressive_mode, ResultsList* results_list,
            MatchFinder* finder) {
    results_list_ = results_list;
    aggressive_mode_ = aggressive_mode;
    finder->addMatcher(
        binaryOperation(
            unless(anyOf(hasOperatorName("&&"), hasOperatorName("||"),
                         hasOperatorName(","), hasOperatorName("?:"))),
            forEachDescendant(
                callExpr(unless(hasDescendant(callExpr()))).bind("call_expr")))
            .bind("bo"),
        this);
    finder->addMatcher(
        cxxOperatorCallExpr(
            forEachDescendant(
                callExpr(unless(hasDescendant(callExpr()))).bind("call_expr")))
            .bind("cxx_bo"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* bo = result.Nodes.getNodeAs<Expr>("bo");
    const CXXOperatorCallExpr* cxx_bo =
        result.Nodes.getNodeAs<CXXOperatorCallExpr>("cxx_bo");
    const CallExpr* call_expr = result.Nodes.getNodeAs<CallExpr>("call_expr");
    if (libtooling_utils::IsInSystemHeader(call_expr, result.Context)) {
      return;
    }
    int64_t operator_id;

    if (bo) {
      if (misra::libtooling_utils::IsInSystemHeader(bo, result.Context)) {
        return;
      }
      operator_id = bo->getID(*result.Context);
    }
    if (cxx_bo) {
      if (libtooling_utils::IsInSystemHeader(cxx_bo, result.Context)) {
        return;
      }
      operator_id = cxx_bo->getID(*result.Context);
    }

    // only report for const function when aggressive_mode on
    if (!aggressive_mode_) {
      if (isa<CXXMemberCallExpr>(call_expr)) {
        const CXXMethodDecl* method =
            cast<CXXMemberCallExpr>(call_expr)->getMethodDecl();
        if (method->isConst()) {
          return;
        }
      }
    }
    if (!call_expr->HasSideEffects(*result.Context)) {
      return;
    }
    libtooling_utils::ConstCallExprVisitor Visitor(result.Context);
    Visitor.Visit(call_expr);
    if (!Visitor.ShouldReport(aggressive_mode_)) {
      return;
    }

    int line =
        misra::libtooling_utils::GetLine(call_expr, result.SourceManager);
    string filename =
        misra::libtooling_utils::GetFilename(call_expr, result.SourceManager);
    auto call_loc =
        call_expr->getSourceRange().printToString(*result.SourceManager);

    // a call expr can be matched multiple time,
    // i.e. f() + g() + h()
    // ast is like:
    //         +(second)
    //  +(first)       h()
    // f()  g()
    if (calls_.insert(call_loc).second == false) {
      return;
    }

    auto find = operator_call_locations_.find(operator_id);

    // two exprs both have side effect and belong to the same parent node
    if (find != operator_call_locations_.end()) {
      find->second.count++;
      if (find->second.count > 2) {
        return;
      }
      ReportError(filename, line, results_list_);
    } else {
      AppendLocNode(call_expr, operator_id, 1, *result.SourceManager);
    }
  }

 private:
  ResultsList* results_list_;
  unordered_map<int64_t, struct Loc> operator_call_locations_;
  std::set<string> calls_;
  bool aggressive_mode_;

  void AppendLocNode(const Expr* expr, int64_t parent_id, int count,
                     const clang::SourceManager& SM) {
    struct Loc loc;
    loc.count = count;
    loc.begin_loc = expr->getBeginLoc().printToString(SM);
    operator_call_locations_[parent_id] = loc;
  }
};

// for case p->f(p), p->f(g(p))
// If we find at least 2 declRefExpr have the same name
// will report
// In the above example, we have two "p", it will report
class MemberExprCallCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        callExpr(
            hasDescendant(memberExpr(hasDescendant(declRefExpr().bind("first")))
                              .bind("member_expr")),
            forEachDescendant(callExpr(
                forEachDescendant(declRefExpr().bind("decl_ref_expr")))))
            .bind("call_expr"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* call_expr = result.Nodes.getNodeAs<CallExpr>("call_expr");
    if (misra::libtooling_utils::IsInSystemHeader(call_expr, result.Context)) {
      return;
    }
    // first will match the p before ->
    // then we just need to find if we can get
    // the second p
    auto* member_expr = result.Nodes.getNodeAs<MemberExpr>("member_expr");
    auto* first = result.Nodes.getNodeAs<DeclRefExpr>("first");
    auto* decl_ref = result.Nodes.getNodeAs<DeclRefExpr>("decl_ref_expr");
    int line =
        misra::libtooling_utils::GetLine(call_expr, result.SourceManager);
    string filename =
        misra::libtooling_utils::GetFilename(call_expr, result.SourceManager);
    // make sure the call expr is the callee of the member expr
    if (first == decl_ref ||
        call_expr->getCallee()->IgnoreImpCasts() != member_expr) {
      return;
    }
    if (first->getDecl() == decl_ref->getDecl()) {
      ReportError(filename, line, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(bool aggressive_mode,
                   analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new OperatorSideEffectCallback;
  callback_->Init(aggressive_mode, results_list_, &finder_);
  mem_callback_ = new MemberExprCallCallback;
  mem_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_13_2
}  // namespace misra
