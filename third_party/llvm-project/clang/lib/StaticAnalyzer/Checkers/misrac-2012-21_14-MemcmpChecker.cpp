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

//=== misrac-2012-21_14-MemcmpChecker.cpp - Check memcmp buffer arguments ---*- C++ -*-===//
//
// The checker that is responsible for rule 21.14.
//
// Rule 21.14:
//  The Standard Library function memcmp shall not be used to compare
//  null terminated strings.
//
// The non-compliant case for the memcmp() arguments:
//  1. Both of the buffer arguments are char (signed) arrays / pointers / string
//     literals, and their contents are strings (have value \0 for some
//     elements).
//  2. At the same time, the length of either string is less than the third
//     argument n.
// All other cases are complicant.
//
// Based on that, the tasks of the checker are as follow:
//  1. Check before every function call to match the call to memcmp().
//  2. Check whether the string reaches its end within the range of comparison
//     when both of the arugments are char arrays / char pointers / string
//     literals.
//
// The general process is:
//  1. Function matchMemcmp() will try to match the function call with the help
//     of matchSignature() and matchFuncName()..
//  2. Once matched, check if both the arguments are of char array / char
//     pointer / string literal. If yes, check their content by
//     checkZeroInBuffer().
//  3. In checkZeroInBuffer(), the checker iterates through the buffer and
//     inspect the symbolic value of each element. If it can sentence that the
//     value of certain element within the range is \0, the function will report
//     false.
//  4. Once the checkZeroInBuffer() returns false, the checker will report a
//     bug.
//
// Problems:
//  - The checker relies on the symbolic execution engine of clang static
//  checker. For some library functions like strcpy(), their source code is not
//  always available for analyzing, which makes the checker fails at checking
//  the bad case bad1: it calls strcpy() to assign two arrays with strings,
//  however the checker can not see the effect of calling that function. One
//  possible solution is to manully modeling the library functions, like they do
//  to ctype.h functions in checker StdLibraryFunctionsChecker.
//  - When the third argument of memcmp is undecidable, the checker will not
//  report any bug. The correct behavior under that situation is still TBD.
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
class MemcmpBufferArgumentChecker : public Checker<check::PreCall> {
public:
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;

private:
  mutable std::unique_ptr<BugType> BT;

  bool matchFuncName(const FunctionDecl &FD, const char *Name) const {
    return FD.getDeclName().getAsString() == Name;
  }

  bool matchSignature(const FunctionDecl &D,
                      const std::vector<QualType> ParmTys,
                      const QualType RetTy) const {
    // Mismatched parameter length
    if (ParmTys.size() != D.getNumParams()) {
      return false;
    }

    // Mismatched return type
    QualType FDRetTy = D.getReturnType().getCanonicalType();
    if (RetTy != FDRetTy) {
      return false;
    }

    // mismatched parameter type
    for (size_t I = 0, E = ParmTys.size(); I != E; ++I) {
      QualType ParmTy = ParmTys[I];
      QualType FDParmTy = D.getParamDecl(I)->getType().getCanonicalType();
      // remove 'restrict' to avoid disturbance on matching
      if (!D.getASTContext().getLangOpts().C99) {
        FDParmTy.removeLocalRestrict();
      }
      if (ParmTy != FDParmTy) {
        return false;
      }
    }

    return true;
  }

  bool matchMemcmp(const CallEvent &Call, CheckerContext &C) const {
    ASTContext &ACtx = C.getASTContext();

    const QualType SizeTy = ACtx.getSizeType();
    const QualType IntTy = ACtx.IntTy;
    const QualType ConstVoidPtrTy =
        ACtx.getPointerType(ACtx.VoidTy.withConst());

    const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());
    if (!FD) {
      return false;
    }

    return matchFuncName(*FD, "memcmp") &&
           matchSignature(*FD, {ConstVoidPtrTy, ConstVoidPtrTy, SizeTy}, IntTy);
  }

  bool hasTypeCharBuffer(const Expr *E) const {
    // char* or char[]
    QualType Ty = E->getType().getCanonicalType();
    if (!Ty->isArrayType() && !Ty->isPointerType()) {
      return false;
    }
    const Type *EltTy = Ty->getPointeeOrArrayElementType();
    if (!EltTy->isCharType() || EltTy->isUnsignedIntegerType()) {
      return false;
    }
    return true;
  }

  bool hasTypeStrLiteral(const Expr *E) const {
    return isa_and_nonnull<StringLiteral>(E);
  }

  bool checkZeroInBuffer(SVal Buffer, SVal Size, CheckerContext &C) const {
    const MemRegion *R = Buffer.getAsRegion();
    if (!isa_and_nonnull<ElementRegion>(R)) {
      //TODO: settle down whether to report undecidable input length
      return false;
    }

    const MemRegion *SR = dyn_cast<ElementRegion>(R)->getSuperRegion();
    if (!isa_and_nonnull<SubRegion>(SR)) {
      return false;
    }

    MemRegionManager &MRM = SR->getMemRegionManager();

    ASTContext &ACtx = C.getASTContext();
    SValBuilder &SVB = C.getSValBuilder();

    QualType CharTy = ACtx.CharTy;

    ProgramStateRef state = C.getState();

    // TODO: handle the case when Size is undecidable
    auto EC = Size.getAs<DefinedOrUnknownSVal>();
    if (!EC) {
      return false;
    }

    NonLoc Idx = SVB.makeZeroArrayIndex();
    NonLoc Step = SVB.makeArrayIndex(1);
    QualType IdxTy = Idx.getType(ACtx);

    ProgramStateRef StInBound = state->assumeInBound(Idx, *EC, true);
    ProgramStateRef StOutBound = state->assumeInBound(Idx, *EC, false);

    while (StInBound && !StOutBound) {
      const ElementRegion *ER =
          MRM.getElementRegion(CharTy, Idx, dyn_cast<SubRegion>(SR), ACtx);
      SVal Val = state->getSVal(ER);

      if (!Val.isUnknownOrUndef()) {
        auto N = Val.getAs<NonLoc>();
        if (state->assume(*N, false) && !state->assume(*N, true)) {
          return true;
        }
      }
      auto newIdx =
          SVB.evalBinOp(state, BO_Add, Idx, Step, IdxTy).getAs<NonLoc>();
      if (!newIdx.hasValue()) {
        llvm_unreachable("Index must have value!");
      }
      Idx = newIdx.getValue();
      StInBound = state->assumeInBound(Idx, *EC, true);
      StOutBound = state->assumeInBound(Idx, *EC, false);
    }
    return false;
  }

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "Wrong usage of function",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT, "memcmp() should not be used to compare null terminated strings.",
        N);
    C.emitReport(std::move(R));
  }
};
} // namespace

void MemcmpBufferArgumentChecker::checkPreCall(const CallEvent &Call,
                                               CheckerContext &C) const {
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());
  if (!FD) {
    return;
  }

  // Match call to memcmp()
  if (!matchMemcmp(Call, C)) {
    return;
  }

  const Expr *ArgA = Call.getArgExpr(0U)->IgnoreImpCasts();
  const Expr *ArgB = Call.getArgExpr(1U)->IgnoreImpCasts();

  // Check buffer content
  if ((hasTypeCharBuffer(ArgA) || hasTypeStrLiteral(ArgA)) &&
      (hasTypeCharBuffer(ArgB) || hasTypeStrLiteral(ArgB))) {
    SVal BufferA = Call.getArgSVal(0U);
    SVal BufferB = Call.getArgSVal(1U);
    SVal Size = Call.getArgSVal(2U);
    if (checkZeroInBuffer(BufferA, Size, C) ||
        checkZeroInBuffer(BufferB, Size, C)) {
      reportBug(C);
    }
    return;
  }
}

void ento::registerMemcmpBufferArgumentChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<MemcmpBufferArgumentChecker>();
}

bool ento::shouldRegisterMemcmpBufferArgumentChecker(
    const CheckerManager &mgr) {
  return true;
}
