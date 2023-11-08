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

#include "autosar/rule_A12_8_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/QualTypeNames.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message = "Moved_from object shall not be read-accessed.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A12_8_3 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        callExpr(
            callee(functionDecl(hasName("std::move"))),
            hasArgument(
                0, declRefExpr(hasType(qualType(hasDeclaration(namedDecl()))))
                       .bind("moved_arg")),
            hasAncestor(
                stmt(hasParent(compoundStmt().bind("cstmt"))).bind("stmt")),
            unless(isExpansionInSystemHeader()))
            .bind("ce"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("ce");
    const DeclRefExpr* moved_arg =
        result.Nodes.getNodeAs<DeclRefExpr>("moved_arg");
    const Decl* forwarded_decl = moved_arg->getDecl();
    const Stmt* stmt = result.Nodes.getNodeAs<Stmt>("stmt");
    const CompoundStmt* cstmt = result.Nodes.getNodeAs<CompoundStmt>("cstmt");
    if (!ce) return;
    const string type = TypeName::getFullyQualifiedName(
        moved_arg->getType().getUnqualifiedType().getDesugaredType(
            *result.Context),
        *result.Context, PrintingPolicy(result.Context->getLangOpts()), true);
    bool isExceptionType = (type.find("::std::unique_ptr") == 0 ||
                            type.find("::std::shared_ptr") == 0 ||
                            type.find("::std::weak_ptr") == 0 ||
                            type.find("::std::basic_ios") == 0 ||
                            type.find("::std::basic_filebuf") == 0 ||
                            type.find("::std::thread") == 0 ||
                            type.find("::std::unique_lock") == 0 ||
                            type.find("::std::future") == 0 ||
                            type.find("::std::shared_lock") == 0 ||
                            type.find("::std::promise") == 0 ||
                            type.find("::std::shared_future") == 0 ||
                            type.find("::std::packaged_task") == 0);
    if (isExceptionType) return;

    bool found_forward = false;
    for (const Stmt* child_stmt : cstmt->children()) {
      if (found_forward) {
        if (ContainsDecl(child_stmt, forwarded_decl)) {
          ReportError(misra::libtooling_utils::GetFilename(
                          child_stmt, result.SourceManager),
                      misra::libtooling_utils::GetLine(child_stmt,
                                                       result.SourceManager),
                      results_list_);
        }
      } else if (child_stmt == stmt) {
        found_forward = true;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  bool ContainsDecl(const Stmt* stmt, const Decl* decl) {
    if (!stmt) {
      return false;
    }
    if (const DeclRefExpr* decl_ref_expr = dyn_cast<DeclRefExpr>(stmt)) {
      return decl_ref_expr->getDecl() == decl;
    }
    for (const Stmt* child_stmt : stmt->children()) {
      if (ContainsDecl(child_stmt, decl)) {
        return true;
      }
    }
    return false;
  }
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A12_8_3
}  // namespace autosar
