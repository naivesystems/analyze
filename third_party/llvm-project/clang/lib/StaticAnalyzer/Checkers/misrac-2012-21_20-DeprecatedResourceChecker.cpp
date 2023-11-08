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

//=== misrac-2012-21_20-DeprecatedResourceChecker.cpp - Check returned pointer
// usage ---*- C++ -*-===//
//
// This checker implements rule 21.20.
// The main task of this checker:
// 1. Identify calls to interested functions.
// 2. Keep track of their current valid returns and record deprecated returns.
// 3. Check if the code accesses the deprecated pointers.
//
// Procedure:
// 1. For task 1, this checker utilizes CallDescriptionMap in CallEvent.h. Refer
//    to CStringChecker for its usage in call matching.
// 2. After matching the function call, the checker will move the previous valid
//    pointer to deprecated pointer set in ProgramState and then save the newly
//    returned pointer to valid map.
// 3. On each access to MemRegion (checkLocation), the checker will first check
//    if the operation is reassigning the pointee. If yes, then it is a valid
//    operation and checker returns. If no, then there are two possible bad
//    cases:
//    - access the content of the pointee by dereferencing the pointer.
//    - read the address of the pointer (e.g. pass the pointer as a paramter)
//    the checker will first try to get the symbolic region of the location and
//    check if the statement is accessing the content of pointee. If it finds
//    one of the symbolic bases is in the deprecated pointer set, an error will
//    be reported.
//    Then the checker will try to get the symbolic region of the object which
//    the location points to. Following the same logic, if it finds matching in
//    deprecated set, an error will be reported.
// 4. Once the returned symbol gets invalid, it will be deleted from the state
//    traits.
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
class DeprecatedResourceChecker
    : public Checker<check::PostCall, check::Location, check::DeadSymbols> {
public:
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  void checkLocation(SVal L, bool IsLoad, const Stmt *S,
                     CheckerContext &C) const;
  void checkDeadSymbols(SymbolReaper &SymReaper, CheckerContext &C) const;

private:
  mutable std::unique_ptr<BugType> BT;

  CallDescriptionMap<bool> FuncRegList = {
      {{CDF_MaybeBuiltin, "asctime", 1}, true},
      {{CDF_MaybeBuiltin, "ctime", 1}, true},
      {{CDF_MaybeBuiltin, "gmtime", 1}, true},
      {{CDF_MaybeBuiltin, "localtime", 1}, true},
      {{CDF_MaybeBuiltin, "localeconv", 0}, true},
      {{CDF_MaybeBuiltin, "getenv", 1}, true},
      {{CDF_MaybeBuiltin, "setlocale", 2}, true},
      {{CDF_MaybeBuiltin, "strerror", 1}, true},
  };

  bool checkSingleBase(const SymbolicRegion *SR, ProgramStateRef state) const;
  bool checkRegionChain(const SymbolicRegion *SR, ProgramStateRef state) const;

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "returned pointer ",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT,
        "[misrac-2012-21.20]: violate rule 21.20.",
        N);
    C.emitReport(std::move(R));
  }
};

class StringWrapper {
  const std ::string Str;

public:
  StringWrapper(const std ::string &S) : Str(S) {}
  const std ::string &get() const { return Str; }
  void Profile(llvm ::FoldingSetNodeID &ID) const { ID.AddString(Str); }
  bool operator==(const StringWrapper &RHS) const { return Str == RHS.Str; }
  bool operator<(const StringWrapper &RHS) const { return Str < RHS.Str; }
};

} // namespace

REGISTER_SET_WITH_PROGRAMSTATE(DeprecatedPtr, SymbolRef)
REGISTER_MAP_WITH_PROGRAMSTATE(CurrentPtrMap, StringWrapper, SymbolRef)

void DeprecatedResourceChecker::checkPostCall(const CallEvent &Call,
                                              CheckerContext &C) const {
  if (!FuncRegList.lookup(Call)) {
    return;
  }
  ProgramStateRef state = C.getState();
  StringWrapper Name(Call.getCalleeIdentifier()->getName().str());
  const SymbolRef *D = state->get<CurrentPtrMap>(Name);
  if (D) {
    state = state->add<DeprecatedPtr>(*D);
  }

  SVal R = Call.getReturnValue();
  const MemRegion *MR =  R.getAsRegion();
  if (!MR) {
    return;
  }
  const SymbolicRegion *SR = MR->getSymbolicBase();

  state = state->set<CurrentPtrMap>(Name, SR->getSymbol());

  C.addTransition(state);
}

void DeprecatedResourceChecker::checkLocation(SVal L, bool IsLoad,
                                              const Stmt *S,
                                              CheckerContext &C) const {
  ProgramStateRef state = C.getState();
  ASTContext &ACtx = C.getASTContext();

  const SymbolicRegion *LSR = dyn_cast_or_null<SymbolicRegion>(L.getAsRegion());

  if (!IsLoad && !LSR) {
    return;
  }

  if (checkRegionChain(LSR, state)) {
    reportBug(C);
    return;
  }

  SVal V = state->getSVal(L.castAs<Loc>());
  if (V.isUnknownOrUndef()) {
    return;
  }
  const SymbolicRegion *VSR = dyn_cast_or_null<SymbolicRegion>(V.getAsRegion());

  if (checkRegionChain(VSR, state)) {
    reportBug(C);
    return;
  }
}

void DeprecatedResourceChecker::checkDeadSymbols(SymbolReaper &SymReaper,
                                                 CheckerContext &C) const {
  ProgramStateRef state = C.getState();
  DeprecatedPtrTy TrackedPtrs = state->get<DeprecatedPtr>();
  for (auto I = TrackedPtrs.begin(), E = TrackedPtrs.end(); I != E; ++I) {
    if (SymReaper.isDead(*I)) {
      state = state->remove<DeprecatedPtr>(*I);
    }
  }
  if (state != C.getState()) {
    C.addTransition(state);
  }
}

bool DeprecatedResourceChecker::checkSingleBase(const SymbolicRegion *SR,
                                                ProgramStateRef state) const {
  SymbolRef SRef = SR->getSymbol();
  if (state->contains<DeprecatedPtr>(SRef)) {
    return true;
  }
  return false;
}

bool DeprecatedResourceChecker::checkRegionChain(const SymbolicRegion *SR,
                                                 ProgramStateRef state) const {
  while (SR) {
    if (checkSingleBase(SR, state)) {
      return true;
    }

    SymbolRef SRef = SR->getSymbol();
    const MemRegion *OMR = SRef->getOriginRegion();
    if (OMR) {
      SR = OMR->getSymbolicBase();
    } else {
      SR = nullptr;
    }
  }
  return false;
}

void ento::registerDeprecatedResourceChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<DeprecatedResourceChecker>();
}

bool ento::shouldRegisterDeprecatedResourceChecker(const CheckerManager &mgr) {
  return true;
}
