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

//=== misrac-2012-22_5-FileDereferenceChecker.cpp - Check whether FILE object
// dereferenced ---*- C++ -*-===//
//
// The checker that is responsible for rule 22.5.
//
// The non-compliant cases:
//  1. Directly derefer a FILE object by dereference operator (f1 = *pf1)
//  2. Indirectly derefer a FILE object by array subscript expression (pf1[0])
//  3. Indirectly derefer a FILE object by member expression (pf1->pos)
//  4. Indirectly derefer a FILE object by calling to specific functions (memcpy
//  or memcmp)
//
// The general process is:
//  Three checkPreStmt functions match different cases above:
//   For each case, call checkFileDereference which returns true if the
//   dereferenced object is a pointer to a FILE object.
//   For the last case, the second argument of memcpy and the first two
//   arguments of memcmp will be checked
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

class FileDereferenceChecker
    : public Checker<check::PreStmt<UnaryOperator>,
                     check::PreStmt<ArraySubscriptExpr>,
                     check::PreStmt<MemberExpr>, check::PreCall> {
  CallDescription MemcpyFn, MemcmpFn;

  mutable std::unique_ptr<BugType> BT;

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "Wrong usage of FILE pointer",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT, "Dereference of a pointer of FILE", N);
    C.emitReport(std::move(R));
  }

  bool checkFileDereference(QualType Q) const;

  bool checkArgs(const CallEvent &Call, int Index,
                 const ASTContext &ACtx) const;

 public:
  FileDereferenceChecker();

  // process * operator
  void checkPreStmt(const UnaryOperator *U, CheckerContext &C) const;
  // process [] expression
  void checkPreStmt(const ArraySubscriptExpr *A, CheckerContext &C) const;
  // process -> expression
  void checkPreStmt(const MemberExpr *M, CheckerContext &C) const;
  // process memcpy or memcmp
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

}  // end anonymous namespace

FileDereferenceChecker::FileDereferenceChecker()
    : MemcpyFn({CDF_MaybeBuiltin, "memcpy", 3}),
      MemcmpFn({CDF_MaybeBuiltin, "memcmp", 3}) {}

bool FileDereferenceChecker::checkFileDereference(QualType Q) const {
  if (Q->isPointerType() && Q->getPointeeType().getAsString() == "FILE") {
    return true;
  }
  return false;
}

bool FileDereferenceChecker::checkArgs(const CallEvent &Call, int Index,
                                       const ASTContext &ACtx) const {
  return ((Call.getArgExpr(Index) &&
           checkFileDereference(
               Call.getArgExpr(Index)->IgnoreParenImpCasts()->getType())) ||
          (!Call.getArgSVal(Index).isUnknownOrUndef() &&
           checkFileDereference(Call.getArgSVal(Index).getType(ACtx))));
}

void FileDereferenceChecker::checkPreStmt(const UnaryOperator *U,
                                          CheckerContext &C) const {
  if (U->getOpcode() != UO_Deref) {
    return;
  }
  if (checkFileDereference(U->getSubExpr()->getType())) {
    reportBug(C);
  }
}

void FileDereferenceChecker::checkPreStmt(const ArraySubscriptExpr *A,
                                          CheckerContext &C) const {
  if (checkFileDereference(A->getLHS()->getType()) ||
      checkFileDereference(A->getRHS()->getType())) {
    reportBug(C);
  }
}

void FileDereferenceChecker::checkPreStmt(const MemberExpr *M,
                                          CheckerContext &C) const {
  if (checkFileDereference(M->getBase()->getType())) {
    reportBug(C);
  }
}

void FileDereferenceChecker::checkPreCall(const CallEvent &Call,
                                          CheckerContext &C) const {
  const ASTContext &ACtx = C.getASTContext();
  if (!matchesAny(Call, MemcpyFn, MemcmpFn)) {
    return;
  }
  if (checkArgs(Call, 0, ACtx) || checkArgs(Call, 1, ACtx)) {
    reportBug(C);
    return;
  }
}

void ento::registerFileDereferenceChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<FileDereferenceChecker>();
}

bool ento::shouldRegisterFileDereferenceChecker(const CheckerManager &mgr) {
  return true;
}
