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

// This rule only focus on one use, so it's not a dead store problem

#include "misra_cpp_2008/rule_0_1_4/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;
using llvm::isa;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list_) {
  std::string error_message = absl::StrFormat(
      "项目不得含有仅使用一次的非易失性（non-volatile）POD变量");
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_0_1_4);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_0_1_4 {
namespace libtooling {

/**
 * This is one use question, it's different with deadstore. It has a larger
 * scope. For example: int y = 20; y = x; The above code is a deadstore and but
 * not violate this rule.
 */

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  finder_.addMatcher(varDecl(unless(parmVarDecl())).bind("var_decl"), this);
  finder_.addMatcher(
      functionDecl(hasBody(compoundStmt()),
                   forEachDescendant(parmVarDecl().bind("param_val_decl")))
          .bind("function_decl"),
      this);
  finder_.addMatcher(declRefExpr().bind("decl_ref"), this);
}

void Checker::run(const MatchFinder::MatchResult& result) {
  clang::ASTContext* context = result.Context;
  if (auto varDecl = result.Nodes.getNodeAs<clang::VarDecl>("var_decl")) {
    // ignore class static data member or non-POD variable
    if (varDecl->isStaticDataMember()) {
      return;
    }
    if (!varDecl->getType().isPODType(*result.Context)) {
      return;
    }
    checkVarDecl(const_cast<VarDecl*>(varDecl),
                 context->getFullLoc(varDecl->getBeginLoc()),
                 result.SourceManager);
  }
  if (auto paramDecl =
          result.Nodes.getNodeAs<clang::ParmVarDecl>("param_val_decl")) {
    auto functionDecl =
        result.Nodes.getNodeAs<clang::FunctionDecl>("function_decl");
    if (!functionDecl) {
      return;
    }
    // ignore default function case, like default constructor.
    if (functionDecl->isDefaulted()) {
      return;
    }
    if (!paramDecl->getType().isPODType(*result.Context)) {
      return;
    }
    checkVarDecl(const_cast<ParmVarDecl*>(paramDecl),
                 context->getFullLoc(paramDecl->getBeginLoc()),
                 result.SourceManager);
  }
  if (auto declRef = result.Nodes.getNodeAs<clang::DeclRefExpr>("decl_ref")) {
    if (!declRef->getType().isPODType(*result.Context)) {
      return;
    }
    checkDeclRef(const_cast<DeclRefExpr*>(declRef));
  }
}

void Checker::checkVarDecl(VarDecl* varDecl, FullSourceLoc location,
                           SourceManager* sourceManager) {
  // If a variable is declared qualified, we should not check it for this rule
  if (varDecl->getType().isVolatileQualified()) {
    return;
  }
  if (location.isValid() && !location.isInSystemHeader()) {
    std::string filename =
        misra::libtooling_utils::GetFilename(varDecl, sourceManager);
    int line_number = misra::libtooling_utils::GetLine(varDecl, sourceManager);
    varDecl_record_[varDecl] = make_pair(filename, line_number);
    if (varDecl_reference_count_.find(varDecl) ==
        varDecl_reference_count_.end()) {
      varDecl_reference_count_[varDecl] = 0;
    }
    // ParmVar won't init
    if (varDecl->hasInit() || varDecl->getKind() == Decl::ParmVar) {
      varDecl_reference_count_[varDecl]++;
    }
    if (varDecl_reference_count_[varDecl] >= 2) {
      varDecl_reference_count_.erase(varDecl);
    }
  }
}

void Checker::checkDeclRef(DeclRefExpr* declRef) {
  if (isa<VarDecl>(declRef->getDecl())) {
    VarDecl* varDecl = cast<VarDecl>(declRef->getDecl());
    // if we didn't find decl before then ignore.
    if (varDecl_reference_count_.find(varDecl) ==
        varDecl_reference_count_.end()) {
      return;
    }
    if (++varDecl_reference_count_[varDecl] >= 2) {
      varDecl_reference_count_.erase(varDecl);
    }
  }
}

void Checker::reportInValidVarDecl() {
  for (auto pair : varDecl_reference_count_) {
    ReportError(varDecl_record_[pair.first].first,
                varDecl_record_[pair.first].second, results_list_);
  }
}

}  // namespace libtooling
}  // namespace rule_0_1_4
}  // namespace misra_cpp_2008
