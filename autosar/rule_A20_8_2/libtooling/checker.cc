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

#include "autosar/rule_A20_8_2/libtooling/checker.h"

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
      "A std::unique_ptr shall be used to represent exclusive ownership.";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace autosar {
namespace rule_A20_8_2 {
namespace libtooling {

struct Loc {
  const std::string Path;
  const int LineNumber;
};

struct PointerInfo {
  bool is_unique_ptr = true;
  const Loc* location;
  int use_count = 0;
};

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    // create a unique_ptr by make_unique()
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> unique_ptr =
        declStmt(has(varDecl().bind("uni_vd")),
                 hasDescendant(implicitCastExpr(
                     hasImplicitDestinationType(pointerType()),
                     hasSourceExpression(declRefExpr(hasDeclaration(
                         functionDecl(hasName("make_unique"),
                                      hasParent(functionTemplateDecl()))))))),
                 unless(isExpansionInSystemHeader()));
    // a decl ref of unique_ptr
    Matcher<Stmt> uni_ptr_ref =
        declRefExpr(hasDeclaration(varDecl(equalsBoundNode("uni_vd"))))
            .bind("decl_ref");
    // funcs that share the ownership
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> decl_stmt =
        declStmt(hasDescendant(cxxConstructExpr(has(callExpr(
            callee(functionDecl(hasName("std::move"))), has(uni_ptr_ref))))));
    // to match a unique_str that is used more than once (indicated by
    // use_count)
    finder->addMatcher(
        functionDecl(hasDescendant(unique_ptr), forEachDescendant(decl_stmt)),
        this);
    // the var decl that is not declared by unique_ptr
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> var_decl_ref =
        declRefExpr(hasDeclaration(
            varDecl(hasType(pointerType(pointee(recordType(hasDeclaration(
                        cxxRecordDecl(unless(hasName("std::unique_ptr")))))))))
                .bind("vd")));
    // to match a var decl that is used only once but not declared as unique_ptr
    // (indicated by is_unique_ptr)
    finder->addMatcher(
        declStmt(hasDescendant(cxxConstructExpr(
            has(var_decl_ref), has(materializeTemporaryExpr(hasDescendant(
                                   declRefExpr().bind("decl_ref"))))))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const VarDecl* uni_vd = result.Nodes.getNodeAs<VarDecl>("uni_vd");
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    string var_name;
    if (uni_vd != nullptr) {
      var_name = uni_vd->getQualifiedNameAsString();
    }
    if (vd != nullptr) {
      var_name = vd->getQualifiedNameAsString();
      pointer_map_[var_name].is_unique_ptr = false;
    }
    const DeclRefExpr* decl_ref =
        result.Nodes.getNodeAs<DeclRefExpr>("decl_ref");
    const Loc* decl_ref_loc =
        new Loc{GetFilename(decl_ref, result.SourceManager),
                GetLine(decl_ref, result.SourceManager)};
    pointer_map_[var_name].location = decl_ref_loc;
    pointer_map_[var_name].use_count++;
  }

  void Report() {
    for (auto it = pointer_map_.begin(); it != pointer_map_.end(); ++it) {
      const PointerInfo& info = it->second;
      if (!info.is_unique_ptr && info.use_count == 1) {
        ReportError(info.location->Path, info.location->LineNumber,
                    results_list_);
      }
      if (info.is_unique_ptr && info.use_count > 1) {
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
}  // namespace rule_A20_8_2
}  // namespace autosar
