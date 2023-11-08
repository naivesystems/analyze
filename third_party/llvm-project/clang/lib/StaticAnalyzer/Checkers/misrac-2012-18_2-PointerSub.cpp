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

//=== misrac-2012-18_2-PointerSub.cpp - Pointer subtraction checker ---*- C++ -*--===//
//
// This files defines misrac-2012=pointerSub Checker, it's modified on top of
// the original CSA PointerSubChecker.
//
// rule 18.2:
// Subtraction between pointers shall only be applied to pointers that address
// elements of the same array.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include <clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h>

using namespace clang;
using namespace ento;

namespace {
class PointerSubMisraChecker
  : public Checker< check::PreStmt<BinaryOperator> > {
  mutable std::unique_ptr<BuiltinBug> BT;
  void reportPointerSubMisuse(const BinaryOperator *B,
                              CheckerContext &C) const;
  bool isArrayRegion(const MemRegion *Region, CheckerContext &C);
public:
  void checkPreStmt(const BinaryOperator *B, CheckerContext &C) const;
  void checkElementInBuffer(CheckerContext &C, const BinaryOperator *B,
                     SVal Element) const;
};
}

void PointerSubMisraChecker::reportPointerSubMisuse(const BinaryOperator *B,
                                                    CheckerContext &C) const {
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    if (!BT)
      BT.reset(
          new BuiltinBug(this, "[misrac-2012-18.2] ",
                         "Pointer subtraction violation of misra_c_2012: rule_18_2"));
    auto R =
        std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);
    R->addRange(B->getSourceRange());
    C.emitReport(std::move(R));
  }
}

void PointerSubMisraChecker::checkElementInBuffer(CheckerContext &C,
                                           const BinaryOperator *B,
                                           SVal Element) const {
  // Check for out of bound array element access.
  ProgramStateRef state = C.getState();
  SValBuilder &SVB = C.getSValBuilder();
  const MemRegion *R = Element.getAsRegion();
  if (!R)
    return;
  const auto *ER = dyn_cast<ElementRegion>(R);
  if (!ER)
    return;

  // Get the size of the array.
  const auto *superReg = cast<SubRegion>(ER->getSuperRegion());
  DefinedOrUnknownSVal ElementCount = getDynamicElementCount(
      state, ER->getSuperRegion(), SVB, ER->getValueType());
  ;
  // Get the index of the accessed element.
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
  ProgramStateRef StInBound = state->assumeInBound(Idx, ElementCount, true);
  ProgramStateRef StOutBound = state->assumeInBound(Idx, ElementCount, false);
  if (!StOutBound && StInBound) {
    // In bound
    return;
  }

  // if the Element is one beyond the end of array, this should not
  // report error. So the next step is check whether Idx-1 is in bound.
  Idx = SVB.evalBinOp(state, BO_Sub, Idx, SVB.makeArrayIndex(1),
                      Idx.getType(C.getASTContext()))
            .castAs<DefinedOrUnknownSVal>();
  StInBound = state->assumeInBound(Idx, ElementCount, true);
  StOutBound = state->assumeInBound(Idx, ElementCount, false);
  if (!StOutBound && StInBound) {
    // In bound
    return;
  }
  reportPointerSubMisuse(B, C);
}

void PointerSubMisraChecker::checkPreStmt(const BinaryOperator *B,
                                     CheckerContext &C) const {
  if (B->getOpcode() != BO_Sub)
    return;

  // Both LHS and RHS must be PointerType
  if (!B->getLHS()->getType()->isPointerType() ||
      !B->getRHS()->getType()->isPointerType())
    return;

  SVal LV = C.getSVal(B->getLHS());
  SVal RV = C.getSVal(B->getRHS());

  const MemRegion *LR = LV.getAsRegion();
  const MemRegion *RR = RV.getAsRegion();

  if (!(LR && RR)){
    reportPointerSubMisuse(B, C);
    return;
  }

  const MemRegion *BaseLR = LR->getBaseRegion();
  const MemRegion *BaseRR = RR->getBaseRegion();

  // this rule only applies to array, not struct
  if (RR->getKind() == MemRegion::FieldRegionKind) {
    reportPointerSubMisuse(B, C);
    return;
  }

  if (BaseLR == BaseRR) {
    checkElementInBuffer(C, B, LV);
    checkElementInBuffer(C, B, RV);
    return;
  }

  // When doing pointer subtraction, if the two pointers do not point to the
  // same memory chunk, emit a warning.
  reportPointerSubMisuse(B, C);
}

void ento::registerPointerSubMisraChecker(CheckerManager &mgr) {
  mgr.registerChecker<PointerSubMisraChecker>();
}

bool ento::shouldRegisterPointerSubMisraChecker(const CheckerManager &mgr) {
  return true;
}
