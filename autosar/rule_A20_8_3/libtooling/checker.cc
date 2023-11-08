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

#include "autosar/rule_A20_8_3/libtooling/checker.h"

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

void ReportError(string path, int line_number, string previous_loc, string loc,
                 ResultsList* results_list) {
  string error_message =
      "A std::shared_ptr shall be used to represent shared ownership.";
  std::vector<std::string> locations{previous_loc, loc};
  AddMultipleLocationsResultToResultsList(results_list, path, line_number,
                                          error_message, locations);
}

}  // namespace

namespace autosar {
namespace rule_A20_8_3 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    Matcher<Stmt> ptr_ref =
        declRefExpr(
            hasDeclaration(
                varDecl(
                    hasType(pointerType(pointee(recordType(hasDeclaration(
                        cxxRecordDecl(unless(hasName("std::shared_ptr")))))))))
                    .bind("vd")))
            .bind("decl_ref");
    finder->addMatcher(
        declStmt(hasDescendant(cxxConstructExpr(
            anyOf(has(callExpr(callee(functionDecl(hasName("std::move"))),
                               has(ptr_ref))),
                  has(ptr_ref))))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    const DeclRefExpr* decl_ref =
        result.Nodes.getNodeAs<DeclRefExpr>("decl_ref");
    const string var_name = vd->getQualifiedNameAsString();
    if (pointer_map_.find(var_name) != pointer_map_.end()) {
      // use more than once but not declared as shared_ptr
      ReportError(
          GetFilename(vd, result.SourceManager),
          GetLine(vd, result.SourceManager),
          misra::libtooling_utils::GetLocation(pointer_map_[var_name],
                                               result.SourceManager),
          misra::libtooling_utils::GetLocation(decl_ref, result.SourceManager),
          results_list_);
      return;
    }
    pointer_map_[var_name] = decl_ref;
  }

 private:
  ResultsList* results_list_;
  std::unordered_map<std::string, const DeclRefExpr*> pointer_map_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A20_8_3
}  // namespace autosar
