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

#include "autosar/rule_A4_10_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Only nullptr literal shall be used as the null-pointer-constant.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A4_10_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      implicitCastExpr(unless(isExpansionInSystemHeader()),
                       anyOf(hasCastKind(CK_NullToPointer),
                             hasCastKind(CK_NullToMemberPointer)),
                       unless(has(cxxNullPtrLiteralExpr())),
                       unless(has(declRefExpr())))
          .bind("cast"),
      this);
  finder->addMatcher(
      implicitCastExpr(unless(isExpansionInSystemHeader()),
                       hasCastKind(CK_IntegralCast), has(gnuNullExpr()))
          .bind("cast"),
      this);
  finder->addMatcher(
      implicitCastExpr(unless(isExpansionInSystemHeader()),
                       anyOf(hasCastKind(CK_NullToPointer),
                             hasCastKind(CK_NullToMemberPointer)),
                       has(declRefExpr().bind("decl")))
          .bind("cast"),
      this);
  finder->addMatcher(
      gnuNullExpr(unless(isExpansionInSystemHeader()), hasParent(callExpr()))
          .bind("null"),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const ImplicitCastExpr* cast =
      result.Nodes.getNodeAs<ImplicitCastExpr>("cast");
  const GNUNullExpr* null = result.Nodes.getNodeAs<GNUNullExpr>("null");
  if (!cast && !null) return;
  const DeclRefExpr* decl = result.Nodes.getNodeAs<DeclRefExpr>("decl");
  if (decl && decl->getDecl()->getType()->isNullPtrType()) return;
  const Stmt* stmt = cast;
  if (!cast) stmt = null;
  std::string path =
      misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(stmt, result.SourceManager);
  ReportError(path, line_number, results_list_);
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A4_10_1
}  // namespace autosar
