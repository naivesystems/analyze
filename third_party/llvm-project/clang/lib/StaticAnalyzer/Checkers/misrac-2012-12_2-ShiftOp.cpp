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

//=== misrac-2012-12_2-ShiftOp.cpp - shift operation overflow checker ---*- C++ -*--===//
//
// This files defines misrac-2012=ShiftOp Checker, it's modified part of the
// code of the original CSA undefinedResultChecker.
//
// rule 12.2:
// The simplest way to make sure the shift operation is not overflow is that:
// 1. the RHS should be a constant (can not be negative)
// 2. LHS should be an unsigned integer
// 3. the value of RHS should less than the bit width of LHS
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace ento;

namespace {
class ShiftOpChecker : public Checker<check::PreStmt<BinaryOperator> > {
 public:
  void checkPreStmt(const BinaryOperator *B, CheckerContext &C) const;

 private:
  mutable std::unique_ptr<BuiltinBug> BT;

  void report(const BinaryOperator *B, CheckerContext &C) const {
    if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
      if (!BT)
        BT.reset(new BuiltinBug(this, "[misrac-2012-12.2]",
                                "violation of misra_c_2012: rule_12_2"));
      auto R = std::make_unique<PathSensitiveBugReport>(
          *BT, BT->getDescription(), N);
      R->addRange(B->getSourceRange());
      C.emitReport(std::move(R));
    }
  }

  bool isShiftOverflow(const BinaryOperator *B, CheckerContext &C) const {
    auto LHSType = B->getLHS()->IgnoreImpCasts()->getType();
    Expr *RHS = B->getRHS();

    if (LHSType->isCharType()) {
      return C.isGreaterOrEqual(RHS, 8);
    }
    if (const auto *BT = dyn_cast<BuiltinType>(LHSType.getCanonicalType())) {
      if (BT->getKind() == BuiltinType::UShort) {
        return C.isGreaterOrEqual(RHS, 16);
      }
    }
    if (const auto *BT = dyn_cast<BuiltinType>(LHSType.getCanonicalType())) {
      if (BT->getKind() == BuiltinType::UInt) {
        if (IntegerLiteral *intLiteral = dyn_cast<IntegerLiteral>(B->getLHS())){
          // TODO: handle different data size among architectures
          if(intLiteral->getValue().ult(256))
            return C.isGreaterOrEqual(RHS, 8);
          if(intLiteral->getValue().ult(65536))
            return C.isGreaterOrEqual(RHS, 16);
        }
        return C.isGreaterOrEqual(RHS, 32);
      }
    }
    if (const auto *BT = dyn_cast<BuiltinType>(LHSType.getCanonicalType())) {
      if (BT->getKind() == BuiltinType::ULong) {
        return C.isGreaterOrEqual(RHS, 64);
      }
    }
    if (const auto *BT = dyn_cast<BuiltinType>(LHSType.getCanonicalType())) {
      if (BT->getKind() == BuiltinType::ULongLong) {
        return C.isGreaterOrEqual(RHS, 128);
      }
    }
    return C.isGreaterOrEqual(B->getRHS(),
                              C.getASTContext().getIntWidth(LHSType));
  }
};
}  // namespace

void ShiftOpChecker::checkPreStmt(const BinaryOperator *B,
                                  CheckerContext &C) const {
  if (!(B->isShiftOp() || B->getOpcode() == BO_ShlAssign ||
           B->getOpcode() == BO_ShrAssign)) {
    return;
  }

  // TODO: handle if two operands are undefined
  if (C.getSVal(B->getLHS()).isUndef()) {
    return;
  }
  if (C.getSVal(B->getRHS()).isUndef()) {
    return;
  }

  // if RHS is negtive
  if (C.isNegative(B->getRHS())) {
    report(B, C);
  }

  // shift can only apply on unsigned integer otherwise it violatess rule 10.1
  if (B->getLHS()->IgnoreImpCasts()->getType()->isSignedIntegerType()) {
    return;
  }

  if (isShiftOverflow(B, C)) {
    report(B, C);
  }
}

void ento::registerShiftOpChecker(CheckerManager &mgr) {
  mgr.registerChecker<ShiftOpChecker>();
}

bool ento::shouldRegisterShiftOpChecker(const CheckerManager &mgr) {
  return true;
}
