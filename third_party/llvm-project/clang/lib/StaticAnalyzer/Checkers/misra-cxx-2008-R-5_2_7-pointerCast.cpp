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

//=== misra-cxx-2008-R-5_2_7-pointerCast.cpp -------------------*- C++ -*--===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace ento;

namespace llvm {
template <> struct FoldingSetTrait<std::string> {
  static inline void Profile(std::string X, FoldingSetNodeID &ID) {
    ID.AddString(X);
  }
};
} // namespace llvm

namespace {
class PointerCastChecker
    : public Checker<check::PostStmt<CastExpr>> {
  void reportPointerCastMisuse(const Expr *E, CheckerContext &C) const;

  mutable std::unique_ptr<BuiltinBug> BT_pointerCast;

public:
  void checkPostStmt(const CastExpr *CE, CheckerContext &C) const;
};
} // end namespace

REGISTER_MAP_WITH_PROGRAMSTATE(RegionType, const MemRegion *, std::string)

void PointerCastChecker::reportPointerCastMisuse(const Expr *E,
                                                   CheckerContext &C) const {
  SourceRange SR = E->getSourceRange();
  if (SR.isInvalid())
    return;
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    if (!BT_pointerCast)
      BT_pointerCast.reset(new BuiltinBug(this, "[misracxx-2008-5.2.7]",
                              "violation of misra_cxx_2008: rule_5_2_7"));
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT_pointerCast, BT_pointerCast->getDescription(), N);
    R->addRange(SR);
    C.emitReport(std::move(R));
  }

}

void PointerCastChecker::checkPostStmt(const CastExpr *CE,
                                        CheckerContext &C) const {

  // skip the node that is part of explicit cast
  if(CE->getCastKind() == clang::CK_LValueToRValue) {
    return;
  }
  /// CK_BitCast - A conversion which causes a bit pattern of one type
  /// to be reinterpreted as a bit pattern of another type.  Generally
  /// the operands must have equivalent size and unrelated types.
  if (CE->getCastKind() == CastKind::CK_BitCast){
    if(CE->getSubExpr()->getType()->isPointerType()) {
      reportPointerCastMisuse(CE, C);
    }
  }

  const Expr *CastedExpr = CE->getSubExpr();
  ProgramStateRef State = C.getState();
  SVal CastedVal = C.getSVal(CastedExpr);

  const MemRegion *Region = CastedVal.getAsRegion();
  if (!Region)
    return;

  const MemRegion *Base = Region->getBaseRegion();
  if(CE->getCastKind() == clang::CK_IntegralToPointer) {
    if(auto *originType = State->get<RegionType>(Region)) {
      auto destType =CE->getType().getAsString();
      if(*originType != destType) {
        reportPointerCastMisuse(CE, C);
      }
    }
  }

  // store the origin type of current memory region
  if(CE->getCastKind() == clang::CK_PointerToIntegral) {
    State = State->set<RegionType>(Region, CE->getSubExpr()->getType().getAsString());
    C.addTransition(State);
  }

}

void ento::registerPointerCastChecker(CheckerManager &mgr) {
  mgr.registerChecker<PointerCastChecker>();
}

bool ento::shouldRegisterPointerCastChecker(const CheckerManager &mgr) {
  return true;
}
