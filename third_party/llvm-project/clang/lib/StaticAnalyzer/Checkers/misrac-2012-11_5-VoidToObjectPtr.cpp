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

//=== misrac-2012-11_5-VoidToObjectPtr.cpp ----------------------*- C++ -*-===//
//
// The checker that is responsible for misrac-2012-11_5.
//
// The implementation is based on DereferenceChecker.
// This file only checks NULL void pointer passed to function argument and cast
// to object pointer in misra c 2012 rule 11.5 exception. The rest of the cases
// are covered in the libtooling checker.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprObjC.h"
#include "clang/AST/ExprOpenMP.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerHelpers.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace ento;

namespace {
class VoidToObjectPtrChecker : public Checker<check::PreCall> {
  void reportBug(const Stmt *S, CheckerContext &C) const;

  void checkNotNull(SVal location, const Stmt *S, CheckerContext &C) const;

public:
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

} // end anonymous namespace

void VoidToObjectPtrChecker::reportBug(const Stmt *S, CheckerContext &C) const {
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    const BugType *BT = new BuiltinBug(this, "[misrac-2012-11.5]",
                                       "violation of misra_c_2012: rule_11_5");
    auto R =
        std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);
    R->addRange(S->getSourceRange());
    C.emitReport(std::move(R));
  }
}

void VoidToObjectPtrChecker::checkNotNull(SVal l, const Stmt *S,
                                       CheckerContext &C) const {
  DefinedOrUnknownSVal location = l.castAs<DefinedOrUnknownSVal>();

  if (!isa<Loc>(location))
    return;

  ProgramStateRef state = C.getState();

  ProgramStateRef notNullState, nullState;
  std::tie(notNullState, nullState) = state->assume(location);

  if (notNullState) {
    // report all non-null void pointer to object pointer cast.
    reportBug(S, C);
    return;
  }

  // From this point forward, we know that the location is null.
  C.addTransition(nullState);
}

void VoidToObjectPtrChecker::checkPreCall(const CallEvent &Call,
                                          CheckerContext &C) const {
  const FunctionDecl *calleeDecl =
      dyn_cast<FunctionDecl>(Call.getCalleeAnalysisDeclContext()->getDecl());
  if (!calleeDecl)
    return;
  for (unsigned int i = 0U; i != Call.getNumArgs(); i++) {
    const Expr *S = Call.getArgExpr(i);
    if (!S->IgnoreImplicit()->getType()->isVoidPointerType() ||
        calleeDecl->getParamDecl(i)->getType()->isVoidPointerType())
      continue;
    if (const DeclRefExpr *Decl = dyn_cast<DeclRefExpr>(S->IgnoreImpCasts())) {
      const ValueDecl *VD = Decl->getDecl();
      // Don't check function arguments inside the callee function
      // We assume that arguments are checked in caller
      if (isa<ParmVarDecl>(VD))
        continue;
    }
    SVal l = Call.getArgSVal(i);
    checkNotNull(l, S, C);
  }
}

void ento::registerVoidToObjectPtrChecker(CheckerManager &mgr) {
  auto *Chk = mgr.registerChecker<VoidToObjectPtrChecker>();
}

bool ento::shouldRegisterVoidToObjectPtrChecker(const CheckerManager &mgr) {
  return true;
}
