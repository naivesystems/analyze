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

//=== misrac-2012-22_7-EOFComparisonChecker.cpp - Check whether comparing EOF
// with modified value ---*- C++ -*-===//
//
// The checker that is responsible for rule 22.7.
//
// The non-compliant cases:
//  Compare macro EOF with value which is subject to type conversions
//
// The general process is:
//  1. Match comparison operator which is in the set BOKindSet (<, >, !=, ==,
//  <=, >=, <=>)
//
//  2. isEOF function checks whehter the Expr is EOF macro by checking whether
//  its spelling location (where the actual character data for the token came
//  from) has the same fileID and line number with EOF macro
//
// 3. checkTypeConversion function checks whether the Expr is subject to any
// type conversions by checking whether any of its initialization type is not
// int
//
// 4. isEOF returns true for one of the operand and checkTypeConversion returns
// true for the other, report a bug
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerHelpers.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"

using namespace clang;
using namespace clang::ento;

namespace {

class EOFComparisoChecker : public Checker<check::PreStmt<BinaryOperator>> {
  mutable std::unique_ptr<BugType> BT;
  std::set<BinaryOperatorKind> BOKindSet = {BO_LT, BO_GT, BO_NE, BO_EQ,
                                            BO_LE, BO_GE, BO_Cmp};

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "Wrong usage of EOF",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT, "Comparing EOF with modified value", N);
    C.emitReport(std::move(R));
  }

  bool isEOF(const Expr *E, const SourceLocation EOFLoc,
             const SourceManager &SM) const;
  bool isTypeConversionOccured(const Expr *E) const;

 public:
  void checkPreStmt(const BinaryOperator *D, CheckerContext &C) const;
};

}  // end anonymous namespace

bool EOFComparisoChecker::isEOF(const Expr *E, const SourceLocation EOFLoc,
                                const SourceManager &SM) const {
  const SourceLocation ExprLoc = SM.getSpellingLoc(E->getBeginLoc());
  return SM.getFileID(ExprLoc) == SM.getFileID(EOFLoc) &&
         SM.getSpellingLineNumber(ExprLoc) == SM.getSpellingLineNumber(EOFLoc);
}

bool EOFComparisoChecker::isTypeConversionOccured(const Expr *E) const {
  if (E->IgnoreParenCasts()->getStmtClass() != Stmt::DeclRefExprClass) {
    return false;
  }
  QualType Ty = E->IgnoreParenCasts()->getType();
  const DeclRefExpr *DRE = cast<DeclRefExpr>(E->IgnoreParenCasts());
  const VarDecl *VD = cast<VarDecl>(DRE->getDecl());
  while (VD->getInit()) {
    if (Ty->isCharType() || !Ty->isIntegerType()) {
      return true;
    }
    if (VD->getInit()->IgnoreParenCasts()->getStmtClass() !=
        Stmt::DeclRefExprClass) {
      break;
    }
    DRE = cast<DeclRefExpr>(VD->getInit()->IgnoreParenCasts());
    VD = cast<VarDecl>(DRE->getDecl());
    Ty = VD->getType();
  }
  TypeInfo Info = VD->getASTContext().getTypeInfo(VD->getType());
  return !Ty->isIntegerType() || Info.Width / 8 != sizeof(int);
}

void EOFComparisoChecker::checkPreStmt(const BinaryOperator *BO,
                                       CheckerContext &C) const {
  const Preprocessor &PP = C.getPreprocessor();
  const SourceManager &SM = C.getSourceManager();
  const auto *MacroII = PP.getIdentifierInfo("EOF");
  if (!MacroII) return;
  const MacroInfo *MI = PP.getMacroInfo(MacroII);
  if (!MI) return;
  const SourceLocation EOFLoc = MI->getDefinitionLoc();

  if (BOKindSet.find(BO->getOpcode()) == BOKindSet.end()) {
    return;
  }

  if ((isEOF(BO->getLHS(), EOFLoc, SM) &&
       isTypeConversionOccured(BO->getRHS())) ||
      (isEOF(BO->getRHS(), EOFLoc, SM) &&
       isTypeConversionOccured(BO->getLHS()))) {
    reportBug(C);
  }
}

void ento::registerEOFComparisonChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<EOFComparisoChecker>();
}

bool ento::shouldRegisterEOFComparisonChecker(const CheckerManager &mgr) {
  return true;
}
