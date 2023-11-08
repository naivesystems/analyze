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

#include "googlecpp/g1216/libtooling/checker.h"

#include <clang/Basic/Builtins.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "Don't use nonstandard extensions";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1216 {
namespace libtooling {

template <class MatchT, class NodeT>
void Callback<MatchT, NodeT>::Init(ResultsList* results_list,
                                   MatchFinder* finder, MatchT matcher) {
  results_list_ = results_list;
  finder->addMatcher(matcher, this);
}

template <class MatchT, class NodeT>
void Callback<MatchT, NodeT>::run(const MatchFinder::MatchResult& result) {
  const NodeT* target = result.Nodes.getNodeAs<NodeT>("target");
  SourceLocation loc = target->getBeginLoc();
  ReportError(
      misra::libtooling_utils::GetLocationFilename(loc, result.SourceManager),
      misra::libtooling_utils::GetLocationLine(loc, result.SourceManager),
      results_list_);
}

// special for intrinsticf_callback_, see README.builtin for details
template <>
void FunctionUseCallback::run(const MatchFinder::MatchResult& result) {
  const FunctionDecl* fun = result.Nodes.getNodeAs<FunctionDecl>("fun");
  const DeclRefExpr* use = result.Nodes.getNodeAs<DeclRefExpr>("use");
  SourceLocation loc = use->getBeginLoc();
  // returned value will be 0 for functions that do not correspond to a builtin
  unsigned int id = fun->getBuiltinID();
  if (id == 0) return;
  if (!result.Context->BuiltinInfo.getHeaderName(id))
    ReportError(
        misra::libtooling_utils::GetLocationFilename(loc, result.SourceManager),
        misra::libtooling_utils::GetLocationLine(loc, result.SourceManager),
        results_list_);
}

// special for attr_callback
template <>
void DeclCallback::run(const MatchFinder::MatchResult& result) {
  const Decl* decl = result.Nodes.getNodeAs<Decl>("decl");
  SourceLocation loc = decl->getBeginLoc();
  if (decl->getPreviousDecl()) return;
  if (const FileScopeAsmDecl* asmdecl = dyn_cast<FileScopeAsmDecl>(decl)) {
    ReportError(
        misra::libtooling_utils::GetLocationFilename(loc, result.SourceManager),
        misra::libtooling_utils::GetLocationLine(loc, result.SourceManager),
        results_list_);
    return;
  }
  if (const FunctionDecl* fd = dyn_cast<FunctionDecl>(decl)) {
    // See README.noreturn for this use of getNoReturnAttr()
    if (fd->getType()->getAs<FunctionType>()->getNoReturnAttr()) {
      ReportError(
          misra::libtooling_utils::GetLocationFilename(loc,
                                                       result.SourceManager),
          misra::libtooling_utils::GetLocationLine(loc, result.SourceManager),
          results_list_);
      return;
    }
  }
  if (!decl->hasAttrs()) return;
  for (const auto& attr : decl->attrs()) {
    string spelling = attr->getSpelling();
    if (absl::StrContains(spelling, "__attribute__")) {
      ReportError(
          misra::libtooling_utils::GetLocationFilename(loc,
                                                       result.SourceManager),
          misra::libtooling_utils::GetLocationLine(loc, result.SourceManager),
          results_list_);
      return;
    }
  }
}

void ASTChecker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  DeclarationMatcher attr_matcher =
      decl(unless(isExpansionInSystemHeader())).bind("decl");
  StatementMatcher stmtexpr_matcher =
      stmtExpr(unless(isExpansionInSystemHeader())).bind("target");
  StatementMatcher cond_matcher =
      binaryConditionalOperator(unless(isExpansionInSystemHeader()))
          .bind("target");
  StatementMatcher addrlabel_matcher =
      addrLabelExpr(unless(isExpansionInSystemHeader())).bind("target");
  StatementMatcher choose_matcher =
      chooseExpr(unless(isExpansionInSystemHeader())).bind("target");
  StatementMatcher gnunull_matcher =
      gnuNullExpr(unless(isExpansionInSystemHeader())).bind("target");
  StatementMatcher caserange_matcher =
      caseStmt(unless(hasCaseConstant(expr()))).bind("target");
  StatementMatcher intrinsticf_matcher =
      declRefExpr(
          to(functionDecl(unless(isExpansionInSystemHeader())).bind("fun")))
          .bind("use");
  StatementMatcher asm_matcher =
      asmStmt(unless(isExpansionInSystemHeader())).bind("target");
  StatementMatcher predefined_matcher =
      predefinedExpr(unless(isExpansionInSystemHeader())).bind("target");
  StatementMatcher designated_matcher =
      designatedInitExpr(unless(isExpansionInSystemHeader())).bind("target");
  TypeLocMatcher va_matcher = typeLoc(loc(variableArrayType())).bind("target");
  attr_callback_ = new DeclCallback;
  stmtexpr_callback_ = new StmtCallback;
  cond_callback_ = new StmtCallback;
  addrlabel_callback_ = new StmtCallback;
  choose_callback_ = new StmtCallback;
  gnunull_callback_ = new StmtCallback;
  caserange_callback_ = new StmtCallback;
  intrinsticf_callback_ = new FunctionUseCallback;
  asm_callback_ = new StmtCallback;
  predefined_callback_ = new StmtCallback;
  designated_callback_ = new StmtCallback;
  va_callback_ = new TypeLocCallback;
  attr_callback_->Init(results_list_, &finder_, attr_matcher);
  stmtexpr_callback_->Init(results_list_, &finder_, stmtexpr_matcher);
  cond_callback_->Init(results_list_, &finder_, cond_matcher);
  addrlabel_callback_->Init(results_list_, &finder_, addrlabel_matcher);
  choose_callback_->Init(results_list_, &finder_, choose_matcher);
  gnunull_callback_->Init(results_list_, &finder_, gnunull_matcher);
  caserange_callback_->Init(results_list_, &finder_, caserange_matcher);
  intrinsticf_callback_->Init(results_list_, &finder_, intrinsticf_matcher);
  asm_callback_->Init(results_list_, &finder_, asm_matcher);
  predefined_callback_->Init(results_list, &finder_, predefined_matcher);
  designated_callback_->Init(results_list, &finder_, designated_matcher);
  va_callback_->Init(results_list_, &finder_, va_matcher);
}

void MacroCallback::MacroExpands(const clang::Token& macro_name_tok,
                                 const clang::MacroDefinition& md,
                                 clang::SourceRange range,
                                 const clang::MacroArgs* args) {
  SourceLocation loc = range.getBegin();
  if (source_manager_->isInSystemHeader(loc)) return;
  if (!md.getMacroInfo()->isBuiltinMacro()) return;
  StringRef name = macro_name_tok.getIdentifierInfo()->getName();
  if (name == "__DATE__" || name == "__TIME__" || name == "__FILE__" ||
      name == "__LINE__")
    return;
  ReportError(
      misra::libtooling_utils::GetLocationFilename(loc, source_manager_),
      misra::libtooling_utils::GetLocationLine(loc, source_manager_),
      results_list_);
}

}  // namespace libtooling
}  // namespace g1216
}  // namespace googlecpp
