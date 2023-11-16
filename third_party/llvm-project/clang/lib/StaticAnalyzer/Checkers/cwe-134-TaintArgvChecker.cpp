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

//=== cwe-134-TaintArgvChecker.cpp ------------------------------*- C++ -*-===//
//
// TaintArgvChecker marks argv tainted from "int main(int argc, char** argv)".
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Checkers/Taint.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace ento;
using namespace taint;

namespace {

class TaintArgvChecker : public Checker<check::PreCall> {
public:
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

// Check if the region the expression evaluates to is the argv parameter.
// argv    = SymRegion{reg_$1<char ** argv>}
const SymbolicRegion *isArgv(SVal Val, CheckerContext &C) {
  const SymbolicRegion *SymReg =
      dyn_cast_or_null<SymbolicRegion>(Val.getAsRegion());
  if (!SymReg)
    return nullptr;
  // NOTE: currently, clang static analyzer handles one top-level function
  // parameter as a NonParamVarRegion R and R.getDecl() returns a ParamVarDecl.
  // This behavior is uncommon and may change in the future.
  const auto *DeclReg = dyn_cast_or_null<NonParamVarRegion>(
      SymReg->getSymbol()->getOriginRegion());
  if (!DeclReg)
    return nullptr;
  if (const ParmVarDecl *VD = dyn_cast<ParmVarDecl>(DeclReg->getDecl())) {
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(VD->getDeclContext())) {
      if (FD->getDeclName().isIdentifier() && FD->getName() == "main" && FD->getNumParams() == 2 &&
          VD->getFunctionScopeIndex() == 1 && VD->getType()->isPointerType())
        return SymReg;
    }
  }
  return nullptr;
}

// Check if the region the expression evaluates to is the argv[i]
// argv[1] = &SymRegion{reg_$2<char * Element{SymRegion{reg_$1<char ** argv>},1
// S64b,char *}>}
const SymbolicRegion *isArgvScript(SVal Val, CheckerContext &C) {
  const SymbolicRegion *SymReg =
      dyn_cast_or_null<SymbolicRegion>(Val.getAsRegion());
  if (!SymReg)
    return nullptr;
  const ElementRegion *ElmReg =
      dyn_cast_or_null<ElementRegion>(SymReg->getSymbol()->getOriginRegion());
  if (!ElmReg)
    return nullptr;
  SymReg = dyn_cast_or_null<SymbolicRegion>(ElmReg->getBaseRegion());
  if (!SymReg)
    return nullptr;
  return isArgv(C.getSValBuilder().makeLoc(SymReg), C);
}
} // namespace

void TaintArgvChecker::checkPreCall(const CallEvent &Call,
                                    CheckerContext &C) const {
  ProgramStateRef State = C.getState();
  for (int i = 0; i < Call.getNumArgs(); i++) {
    SVal Val = Call.getArgSVal(i);
    if (const SymbolicRegion *SymReg = isArgv(Val, C)) {
      State = addTaint(State, SymReg);
      continue;
    }
    if (const SymbolicRegion *SymReg = isArgvScript(Val, C)) {
      State = addTaint(State, SymReg);
      continue;
    }
  }
  C.addTransition(State);
}

void ento::registerTaintArgvChecker(CheckerManager &mgr) {
  mgr.registerChecker<TaintArgvChecker>();
}

bool ento::shouldRegisterTaintArgvChecker(const CheckerManager &mgr) {
  return true;
}
