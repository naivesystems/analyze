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

#include "autosar/rule_A20_8_4/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "A std::unique_ptr shall be used over std::shared_ptr if ownership sharing is not required.";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace autosar {
namespace rule_A20_8_4 {
namespace libtooling {

struct Loc {
  const std::string Path;
  const int LineNumber;
};

struct PointerInfo {
  const Loc* location;
  int use_count = 0;
};

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    // create a shared_ptr by make_shared()
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> shared_ptr =
        declStmt(has(varDecl().bind("vd")),
                 hasDescendant(implicitCastExpr(
                     hasImplicitDestinationType(pointerType()),
                     hasSourceExpression(declRefExpr(hasDeclaration(
                         functionDecl(hasName("make_shared"),
                                      hasParent(functionTemplateDecl()))))))),
                 unless(isExpansionInSystemHeader()));
    // funcs that share the ownership
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> decl_stmt =
        declStmt(hasDescendant(cxxConstructExpr(
            has(declRefExpr(hasDeclaration(varDecl(equalsBoundNode("vd"))))
                    .bind("decl_ref")))));
    // with std::move
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> decl_stmt_move =
        declStmt(hasDescendant(cxxConstructExpr(has(callExpr(
            callee(functionDecl(hasName("std::move"))),
            has(declRefExpr(hasDeclaration(varDecl(equalsBoundNode("vd"))))
                    .bind("decl_ref")))))));
    // to match a shared_ptr that only be used once
    finder->addMatcher(functionDecl(hasDescendant(shared_ptr),
                                    anyOf(forEachDescendant(decl_stmt),
                                          forEachDescendant(decl_stmt_move))),
                       this);
    // to match a single shared_ptr var decl
    finder->addMatcher(
        functionDecl(hasDescendant(shared_ptr),
                     unless(anyOf(hasDescendant(decl_stmt),
                                  hasDescendant(decl_stmt_move)))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    string var_name = vd->getQualifiedNameAsString();
    const DeclRefExpr* decl_ref =
        result.Nodes.getNodeAs<DeclRefExpr>("decl_ref");
    if (vd != nullptr && decl_ref == nullptr) {
      ReportError(GetFilename(vd, result.SourceManager),
                  GetLine(vd, result.SourceManager), results_list_);
      return;
    }
    if (decl_ref != nullptr) {
      if (pointer_map_.find(var_name) == pointer_map_.end()) {
        const Loc* decl_ref_loc =
            new Loc{GetFilename(decl_ref, result.SourceManager),
                    GetLine(decl_ref, result.SourceManager)};
        pointer_map_[var_name].location = decl_ref_loc;
      }
      pointer_map_[var_name].use_count++;
    }
  }

  void Report() {
    for (auto it = pointer_map_.begin(); it != pointer_map_.end(); ++it) {
      const PointerInfo& info = it->second;
      if (info.use_count == 1) {
        ReportError(info.location->Path, info.location->LineNumber,
                    results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
  std::unordered_map<string, PointerInfo> pointer_map_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

void Checker::Report() { callback_->Report(); }

}  // namespace libtooling
}  // namespace rule_A20_8_4
}  // namespace autosar
