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

//=== misrac-2012-22_6-UseClosedFileChecker.cpp - Check whether using the closed
// FILE ---*- C++ -*-===//
//
// The checker that is responsible for rule 22.6.
//
// The non-compliant cases:
//  Use the pointer of a FILE which has been closed
//
// There is a set StreamSet which records the opening file stream
//
// The general process is:
//  (1) checkPreCall function matches fclose and removes the corresponding
//  SymbolRef from StreamSet
//
//  (2) checkPostCal function matches fopen and add the SymbolRef of its return
//  value to StreamSet

//  (3) checkPreStmt function matches DeclRefExpr which may use pointer to a
//  closed FILE:
//   If DeclRefExpr is not a NULL constant pointer and SymbolRef of it does not
//   exists in StreamSet, report a bug
//
// Details:
//  (1) how to get SymbolRef of candidate DeclRefExpr:
//   Directly use C.getSVal or State.getSVal will get NULL. So I use getBinding
//   to the get the read SVal binding to the DeclRefExpr
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerHelpers.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"

using namespace clang;
using namespace clang::ento;

namespace {

class UseClosedFileChecker
    : public Checker<check::PreCall, check::PostCall, check::PreStmt<Expr>> {
  mutable std::unique_ptr<BugType> BT;
  CallDescription OpenFn, TempFn, CloseFn;

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "Wrong usage of FILE pointer",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT, "Use closed FILE stream", N);
    C.emitReport(std::move(R));
  }

 public:
  UseClosedFileChecker();
  /// Process fclose
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
  /// Process fopen.
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  /// Process DeclRefExpr which may use pointer to a closed FILE
  void checkPreStmt(const Expr *D, CheckerContext &C) const;
};

}  // end anonymous namespace

REGISTER_SET_WITH_PROGRAMSTATE(StreamSet, SymbolRef)

UseClosedFileChecker::UseClosedFileChecker()
    : OpenFn({CDF_MaybeBuiltin, "fopen"}),
    TempFn({CDF_MaybeBuiltin, "tmpfile"}),
    CloseFn({CDF_MaybeBuiltin, "fclose", 1}) {}

void UseClosedFileChecker::checkPreCall(const CallEvent &Call,
                                        CheckerContext &C) const {
  if (!Call.isGlobalCFunction()) return;

  if (!CloseFn.matches(Call)) return;

  // Get the symbolic value corresponding to the file handle.
  SymbolRef FileDesc = Call.getArgSVal(0).getAsSymbol();
  if (!FileDesc) return;

  // Generate the next transition (an edge in the exploded graph).
  ProgramStateRef State = C.getState();
  if (State->contains<StreamSet>(FileDesc)) {
    State = State->remove<StreamSet>(FileDesc);
  }
  C.addTransition(State);
}

void UseClosedFileChecker::checkPostCall(const CallEvent &Call,
                                         CheckerContext &C) const {
  if (!Call.isGlobalCFunction()) return;

  if (!OpenFn.matches(Call) && !TempFn.matches(Call)) return;

  // Get the symbolic value corresponding to the file handle.
  SymbolRef FileDesc = Call.getReturnValue().getAsSymbol();
  if (!FileDesc) return;

  // Generate the next transition.
  ProgramStateRef State = C.getState();
  State = State->add<StreamSet>(FileDesc);
  C.addTransition(State);
}

void UseClosedFileChecker::checkPreStmt(const Expr *E,
                                        CheckerContext &C) const {
  if (E->IgnoreParenImpCasts()->getStmtClass() != Stmt::DeclRefExprClass) {
    return;
  }
  const DeclRefExpr *DRE = cast<DeclRefExpr>(E->IgnoreParenImpCasts());
  if (!DRE->getType()->isPointerType() ||
      DRE->getType()->getPointeeType().getAsString() != "FILE") {
    return;
  }
  ProgramStateRef State = C.getState();
  StoreManager &SM = C.getStoreManager();
  const VarDecl *VD = cast<VarDecl>(DRE->getDecl());
  std::string Name = VD->getNameAsString();
  if (Name == "stderr" || Name == "stdin" || Name == "stdout") {
    return;
  }
  const Loc L = SM.getLValueVar(VD, C.getLocationContext());
  Store S = State->getStore();
  SymbolRef FileDesc = SM.getBinding(S, L).getAsSymbol();
  if (!DRE->isNullPointerConstant(C.getASTContext(),
                                  Expr::NPC_ValueDependentIsNotNull) &&
      !State->contains<StreamSet>(FileDesc)) {
    reportBug(C);
  }
}

void ento::registerUseClosedFileChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<UseClosedFileChecker>();
}

bool ento::shouldRegisterUseClosedFileChecker(const CheckerManager &mgr) {
  return true;
}
