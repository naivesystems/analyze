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

//=== misra-2012-18_1-PointerArith.cpp -------------------------*- C++ -*--===//
//
// For Rule 18.1 A pointer resulting from arithmetic on a pointer operand shall
// address an element of the same array as that pointer operand
//
// Amplification:
// This rule applies to these forms of pointer arithmetic:
// (1) integer + pointer
// (2) pointer + integer
// (3) pointer += integer
// (4) pointer - integer
// (5) pointer -= integer
// (6) pointer++
// (7) ++pointer
// (8) --pointer
// (9) pointer--
// (10) pointer [integer]
// (11) integer [pointer]
//
// And the pointer dereference:
// (12) pointer->field
// (13) *pointer
// For pointer to one beyond the end of the array, creating is permitted, but
// dereferencing is not permitted. The checker treats (10) - (13) as pointer
// dereference.
//
// This checker use the similar approach in the origin ArrayBoundChecker. For
// pointer arithmetic, the checker computes the result and check whether the
// result index is in the buffer:
// (1) - (5) are matched in check::PostStmt<BinaryOperator>,
// (6) - (9) are matched in check::PostStmt<UnaryOperator>
// (10) - (11) are matched in check::PostStmt<ArraySubscriptExpr>
// (12) is matched in check::PostStmt<MemberExpr>
// (13) is matched in check::PostStmt<UnaryOperator>
// then we will check pointer in function checkPointerAccess
// For detail implement, see the comments below.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"

using namespace clang;
using namespace ento;

namespace {
class PointerArithMisraChecker
    : public Checker<
          check::PostStmt<ArraySubscriptExpr>, check::PostStmt<BinaryOperator>,
          check::PostStmt<UnaryOperator>, check::PostStmt<MemberExpr>> {
  mutable std::unique_ptr<BuiltinBug> BT;

public:
  void checkPostStmt(const ArraySubscriptExpr *A, CheckerContext &C) const;
  void checkPostStmt(const BinaryOperator *B, CheckerContext &C) const;
  void checkPostStmt(const UnaryOperator *UO, CheckerContext &C) const;
  void checkPostStmt(const MemberExpr *M, CheckerContext &C) const;

private:
  // Any sub with non-zero value, or add value is greater than one to the
  // non ElementRegion, shall be reported error.
  // For example:
  // int x; int *p = &x; p++; /* It's good because &x can be ElementRegion*/
  // int y = p[0]; /* This is error, because it's pointer dereference */
  // After p++, p is not point to ElementRegion, so it will be reported here.
  const ElementRegion *getElementRegion(SVal PointerV, const Stmt *S,
                                        CheckerContext &C,
                                        bool isSubOrAddGreaterThanOne) const;
  bool isZeroSval(DefinedOrUnknownSVal s, CheckerContext &C) const;
  bool isOneSval(DefinedOrUnknownSVal s, CheckerContext &C) const;
  // report error
  void reportPointerArithMisuse(const Stmt *LoadS, CheckerContext &C) const;
  // check the index Idx is in bound of ElementRegion ER
  // if isDereference is true, this is a check for dereference pointer
  // or for one beyond the end of array.
  void checkPointerAccess(CheckerContext &C, const ElementRegion *ER,
                          DefinedOrUnknownSVal Idx, const Stmt *LoadS,
                          bool isDereference) const;
  // create new index base on old pointer index and arithmetic integer
  // For example:
  // - match 10 and 11 with checkLocation
  // - match
  // int arr[10]; int* p1 = arr + 3; int* p2 = p1 + 5;
  // For p2, old index is 3, step is 5, then we will check whether
  // index 8 is in bound of array arr in function checkPointerAccess
  Optional<DefinedOrUnknownSVal> createNewIndex(CheckerContext &C,
                                                BinaryOperatorKind op,
                                                SVal oldIndex, SVal step) const;
};
} // namespace

bool PointerArithMisraChecker::isZeroSval(DefinedOrUnknownSVal s,
                                          CheckerContext &C) const {
  ProgramStateRef state = C.getState();
  if (state->assume(s, false) && !state->assume(s, true)) {
    return true;
  }
  return false;
}

bool PointerArithMisraChecker::isOneSval(DefinedOrUnknownSVal s,
                                         CheckerContext &C) const {
  ProgramStateRef state = C.getState();
  ASTContext &ACtx = C.getASTContext();

  SValBuilder &SvalBuilder = C.getSValBuilder();
  NonLoc One = SvalBuilder.makeArrayIndex(1);
  QualType IdxTy = s.getType(ACtx);
  ProgramStateRef stateOne, stateNonOne;
  std::tie(stateOne, stateNonOne) =
      state->assume(SvalBuilder.evalEQ(state, s, One));
  return (stateOne && !stateNonOne);
}

const ElementRegion *PointerArithMisraChecker::getElementRegion(
    SVal PointerV, const Stmt *S, CheckerContext &C,
    bool isSubOrAddGreaterThanOne) const {
  const MemRegion *R = PointerV.getAsRegion();
  if (!R) {
    // For example: int* addr = (int*) 0x12354;
    reportPointerArithMisuse(S, C);
    return nullptr;
  }
  const ElementRegion *ER = dyn_cast<ElementRegion>(R);
  if (!ER) {
    if (isa<SymbolicRegion>(R)) {
      // ignore Symbolic region (eg: global variable)
      return nullptr;
    }
    // this is not a pointer to an array, and the add value is not zero or one.
    // The standard treats an object not a member of an array as if it
    // were an array with single element, so any sub with non-zero
    // value, or add value is greater than one is a violation.
    if (isSubOrAddGreaterThanOne)
      reportPointerArithMisuse(S, C);
    return nullptr;
  }
  return ER;
}

Optional<DefinedOrUnknownSVal> PointerArithMisraChecker::createNewIndex(
    CheckerContext &C, BinaryOperatorKind op, SVal oldIndex, SVal step) const {
  SValBuilder &SVB = C.getSValBuilder();
  ASTContext &ACtx = C.getASTContext();
  ProgramStateRef state = C.getState();

  return SVB.evalBinOp(state, op, oldIndex, step, oldIndex.getType(ACtx))
      .getAs<DefinedOrUnknownSVal>();
}

void PointerArithMisraChecker::reportPointerArithMisuse(
    const Stmt *LoadS, CheckerContext &C) const {
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    const SourceManager &SM = C.getSourceManager();
    SourceLocation Loc = LoadS->getBeginLoc();
    ASTContext &ACtx = C.getASTContext();
    SourceLocation MacroLoc = SM.getSpellingLoc(LoadS->getEndLoc());
    std::string message = "Pointer arithmetic may result in the pointer addressing a different array.";
    if (Loc.isMacroID() && MacroLoc.isValid()) {
      StringRef MacroName = Lexer::getImmediateMacroName(
          Loc, ACtx.getSourceManager(), ACtx.getLangOpts());
      std::string MacroLocStr = MacroLoc.printToString(SM);
      std::string prefix = "/src/";
      MacroLocStr = MacroLocStr.substr(0, prefix.length()) == prefix ? MacroLocStr.substr(prefix.length()) : MacroLocStr;
      std::string macroMessage;
      if (MacroName.empty()) {
         macroMessage = " It is expanded from " + MacroLocStr;
      } else {
        macroMessage = " It is expanded from macro '" + MacroName.str() + "' at " + MacroLocStr;
      }
      message += macroMessage;
    }
    if (!BT) {
      BT.reset(new BuiltinBug(this, "[misrac-2012-18.1] ",
                            message.c_str()));
    }
    auto R =
        std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);
    R->addRange(LoadS->getSourceRange());
    C.emitReport(std::move(R));
  }
}

void PointerArithMisraChecker::checkPointerAccess(CheckerContext &C,
                                                  const ElementRegion *ER,
                                                  DefinedOrUnknownSVal Idx,
                                                  const Stmt *LoadS,
                                                  bool isDereference) const {
  ProgramStateRef state = C.getState();

  // Zero index is always in bound, this also passes ElementRegions created for
  // pointer casts.
  if (isZeroSval(Idx, C)) {
    return;
  }

  // Get the size of the array.
  DefinedOrUnknownSVal ElementCount = getDynamicElementCount(
      state, ER->getSuperRegion(), C.getSValBuilder(), ER->getValueType());

  ProgramStateRef StInBound = state->assumeInBound(Idx, ElementCount, true);
  ProgramStateRef StOutBound = state->assumeInBound(Idx, ElementCount, false);
  if (StOutBound && !StInBound) {
    // possible out of bound
    if (isDereference) {
      // This is a dereference or check Idx - 1, report error
      reportPointerArithMisuse(LoadS, C);
    } else {
      // This could be a creation of a pointer to one beyond the end of the
      // array, so if the idx is not zero, we will check access to Idx - 1.
      // if Idx - 1 is out of bound, we will report error.
      SValBuilder &SVB = C.getSValBuilder();

      NonLoc Step = SVB.makeArrayIndex(1);
      auto newIdx = createNewIndex(C, BO_Sub, Idx, Step);
      if (!newIdx) {
        return;
      }
      checkPointerAccess(C, ER, *newIdx, LoadS, true);
    }
    return;
  }
}

void PointerArithMisraChecker::checkPostStmt(const ArraySubscriptExpr *A,
                                             CheckerContext &C) const {
  const Expr *lhs = A->getLHS();
  const Expr *rhs = A->getRHS();
  SVal PointerV;
  SVal IndexV;

  if (lhs->getType()->isPointerType() && rhs->getType()->isIntegerType()) {
    // pointer [integer]
    PointerV = C.getSVal(lhs);
    IndexV = C.getSVal(rhs);
  } else if (rhs->getType()->isPointerType() &&
             lhs->getType()->isIntegerType()) {
    // integer [pointer]
    PointerV = C.getSVal(rhs);
    IndexV = C.getSVal(lhs);
  } else {
    return;
  }
  // For array non-zero indexing, if the checker cannot convert the pointer to
  // an ElementRegion, it will report error. The checker treats array indexing
  // as dereference.
  // For example: int x; int *p = &x; int y = p[0]; p++; /* This is good */
  // int z = p[0];                                /* This is a violation */
  // TODO: fix false positive case
  // int *p2 = &p[0];
  if (IndexV.isUndef()) {
    reportPointerArithMisuse(A, C);
    return;
  }
  DefinedOrUnknownSVal duv = IndexV.castAs<DefinedOrUnknownSVal>();
  bool isZeroIndex = false;
  if (isZeroSval(duv, C)) {
    isZeroIndex = true;
  }
  const ElementRegion *ER = getElementRegion(PointerV, A, C, !isZeroIndex);
  if (!ER)
    return;
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
  auto newIdx = createNewIndex(C, BO_Add, Idx, IndexV);
  if (!newIdx) {
    return;
  }
  checkPointerAccess(C, ER, *newIdx, A, true);
}

void PointerArithMisraChecker::checkPostStmt(const BinaryOperator *B,
                                             CheckerContext &C) const {
  BinaryOperator::Opcode opcode = B->getOpcode();
  if (opcode != BO_Add && opcode != BO_AddAssign && opcode != BO_Sub &&
      opcode != BO_SubAssign) {
    return;
  }
  Expr *lhs = B->getLHS();
  Expr *rhs = B->getRHS();
  SVal PointerV;
  SVal IndexV;
  bool isSubOrAddGreaterThanOne = false;

  if (opcode == BO_Add || opcode == BO_AddAssign) {
    if (lhs->getType()->isPointerType() && rhs->getType()->isIntegerType()) {
      // pointer + int
      PointerV = C.getSVal(lhs);
      IndexV = C.getSVal(rhs);
    } else if (rhs->getType()->isPointerType() &&
               lhs->getType()->isIntegerType()) {
      // int + pointer
      PointerV = C.getSVal(rhs);
      IndexV = C.getSVal(lhs);
    } else {
      // not related to this rule
      return;
    }
    opcode = BO_Add;
  } else if (opcode == BO_Sub || opcode == BO_SubAssign) {
    if (lhs->getType()->isPointerType() && rhs->getType()->isIntegerType()) {
      // pointer - int
      PointerV = C.getSVal(lhs);
      IndexV = C.getSVal(rhs);
    } else {
      // not related to this rule
      return;
    }
    opcode = BO_Sub;
    isSubOrAddGreaterThanOne = true;
  } else {
    return;
  }
  if (IndexV.isUndef()) {
    // if index is not initialized, report error
    reportPointerArithMisuse(B, C);
    return;
  }
  DefinedOrUnknownSVal duv = IndexV.castAs<DefinedOrUnknownSVal>();
  if (isZeroSval(duv, C)) {
    // all add or sub zero expr is permitted
    return;
  }
  if (!isOneSval(duv, C)) {
    isSubOrAddGreaterThanOne = true;
  }
  const ElementRegion *ER =
      getElementRegion(PointerV, B, C, isSubOrAddGreaterThanOne);
  if (!ER)
    return;
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
  auto newIdx = createNewIndex(C, opcode, Idx, IndexV);
  if (!newIdx) {
    return;
  }
  checkPointerAccess(C, ER, *newIdx, B, false);
}

void PointerArithMisraChecker::checkPostStmt(const UnaryOperator *UO,
                                             CheckerContext &C) const {
  Expr *expr = UO->getSubExpr();
  if (!expr->getType()->isPointerType()) {
    return;
  }
  BinaryOperator::Opcode opcode;
  if (UO->isIncrementOp()) {
    // check pointer++ or ++pointer
    opcode = BO_Add;
  } else if (UO->isDecrementOp()) {
    // check pointer-- or --pointer
    opcode = BO_Sub;
  } else if (UO->getOpcode() == UO_Deref) {
    // process *p
    opcode = BO_Add;
  } else {
    // not related to this rule
    return;
  }
  SVal PointerV = C.getSVal(expr);
  // isSubOrAddGreaterThanOne is true only when it's a Decrement op.
  const ElementRegion *ER =
      getElementRegion(PointerV, UO, C, UO->isDecrementOp());
  if (!ER)
    return;
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
  SValBuilder &SVB = C.getSValBuilder();

  // if it's Increment or Decrement, we should add or sub 1 to the pointer value
  NonLoc Step = SVB.makeArrayIndex(UO->isIncrementDecrementOp());
  auto newIdx = createNewIndex(C, opcode, Idx, Step);
  if (!newIdx) {
    return;
  }
  // if it's a dereference operator, isDereference shall be true.
  checkPointerAccess(C, ER, *newIdx, UO, UO->getOpcode() == UO_Deref);
}

void PointerArithMisraChecker::checkPostStmt(const MemberExpr *M,
                                             CheckerContext &C) const {
  if (!M->getBase()->getType()->isPointerType()) {
    return;
  }
  SVal l = C.getSVal(M->getBase());
  const MemRegion *R = l.getAsRegion();
  if (!R)
    // This checker doesn't report bugs like NULL->something
    return;

  const ElementRegion *ER = getElementRegion(l, M, C, false);
  if (!ER)
    return;
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
  checkPointerAccess(C, ER, Idx, M, true);
}

void ento::registerPointerArithMisraChecker(CheckerManager &mgr) {
  mgr.registerChecker<PointerArithMisraChecker>();
}

bool ento::shouldRegisterPointerArithMisraChecker(const CheckerManager &mgr) {
  return true;
}
