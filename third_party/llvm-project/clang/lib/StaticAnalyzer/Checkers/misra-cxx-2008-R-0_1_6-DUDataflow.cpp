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

//=== misra-cxx-2008-R-0_1_6-DUDataflow.cpp --------------------*- C++ -*--===//

#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace ento;

namespace {
class DUDataflowChecker
    : public Checker<check::PostStmt<DeclStmt>, check::PreStmt<Expr>,
                     check::EndFunction> {

  void ReportError(SourceLocation Loc, CheckerContext &C) const;

  mutable std::unique_ptr<BuiltinBug> BT_DUDataflow;

public:
  void checkPostStmt(const DeclStmt *S, CheckerContext &C) const;
  void checkPreStmt(const Expr *E, CheckerContext &C) const;
  void checkEndFunction(const ReturnStmt *RS, CheckerContext &C) const;
};
} // end namespace

REGISTER_MAP_WITH_PROGRAMSTATE(DefinedVars, const VarDecl *, SourceLocation)

void DUDataflowChecker::ReportError(SourceLocation Loc,
                                    CheckerContext &C) const {
  if (Loc.isInvalid())
    return;
  if (!BT_DUDataflow)
    BT_DUDataflow.reset(
        new BuiltinBug(this, "[misracxx-2008-0.1.6]",
                       "violation of misra_cxx_2008: rule_0_1_6"));
  PathDiagnosticLocation Pos(Loc, C.getSourceManager());
  auto R = std::make_unique<BasicBugReport>(
      *BT_DUDataflow, BT_DUDataflow->getDescription(), Pos);
  C.emitReport(std::move(R));
}

void DUDataflowChecker::checkPreStmt(const Expr *E, CheckerContext &C) const {
  ProgramStateRef State = C.getState();
  E = E->IgnoreParenLValueCasts();
  if (const DeclRefExpr *RE = dyn_cast<DeclRefExpr>(E)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(RE->getDecl())) {
      State = State->remove<DefinedVars>(VD);
    }
    C.addTransition(State);
    return;
  }
  if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    if (!UO->isIncrementDecrementOp())
      return;
    if (const DeclRefExpr *RE = dyn_cast<DeclRefExpr>(UO->getSubExpr())) {
      if (const VarDecl *VD = dyn_cast<VarDecl>(RE->getDecl())) {
        State = State->set<DefinedVars>(VD, RE->getLocation());
      }
      C.addTransition(State);
    }
    return;
  }
  if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    if (!BO->isAssignmentOp())
      return;
    if (const DeclRefExpr *RE = dyn_cast<DeclRefExpr>(BO->getLHS())) {
      if (const VarDecl *VD = dyn_cast<VarDecl>(RE->getDecl())) {
        State = State->set<DefinedVars>(VD, RE->getLocation());
      }
      C.addTransition(State);
    }
    return;
  }
}

void DUDataflowChecker::checkPostStmt(const DeclStmt *S,
                                      CheckerContext &C) const {
  ProgramStateRef State = C.getState();
  for (auto it = S->decl_begin(); it != S->decl_end(); it++) {
    if (auto vd = dyn_cast<VarDecl>(*it)) {
      State = State->set<DefinedVars>(vd, vd->getLocation());
    }
  }
  C.addTransition(State);
}

void DUDataflowChecker::checkEndFunction(const ReturnStmt *RS,
                                         CheckerContext &C) const {
  ProgramStateRef State = C.getState();
  DefinedVarsTy DefinedMap = State->get<DefinedVars>();
  if (DefinedMap.isEmpty()) {
    return;
  }
  for (const auto &kv : DefinedMap) {
    ReportError(kv.second, C);
    State->remove<DefinedVars>(kv.first);
  }
  C.addTransition(State);
}

void ento::registerDUDataflowChecker(CheckerManager &mgr) {
  mgr.registerChecker<DUDataflowChecker>();
}

bool ento::shouldRegisterDUDataflowChecker(const CheckerManager &mgr) {
  return true;
}
