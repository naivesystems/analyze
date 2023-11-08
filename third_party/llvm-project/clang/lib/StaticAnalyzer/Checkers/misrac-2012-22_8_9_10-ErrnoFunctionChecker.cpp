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

//=== misrac-2012-22_8_9_10-ErrnoFunctionChecker.cpp - errno function checker ---*- C++ -*--===//
//
// The checker that is responsible for rule 22.8, 22.9, 22.10.
//
// - 22.8: The value of errno shall be set to zero prior to a call to an
//         errno-setting-function (SetErrnoMisraChecker)
// - 22.9: The value of errno shall be tested against zero after calling an
//         errno-setting-function (TestErrnoMisraChecker)
// - 22.10: The value of errno shall only be tested when the last function to be
//         called was an errno-setting-function (MisusedTestErrnoMisraChecker)
//
// In this checker, errno setting functions is:
//
// ftell, fgetpos, fsetpos, fgetwc, fputwc
// strtoimax, strtoumax, strtol, strtoul, strtoll, strtoull, strtof, strtod,
// strtold wcstoimax, wcstoumax, wcstol, wcstoul, wcstoll, wcstoull, wcstof,
// wcstod, wcstold wcrtomb, wcsrtombs, mbrtowc
//
// Some errno setting functions can use its return value to determined whether
// an error has occurred(for example: fsetpos), some can not.
//
// In our system header(lhw09), errno is defined as:
//
// #define errno (*__errno_location())
//
// They are all function returns an int pointer.
// For other kinds of errno location functions, see ErrnoLocationCalls.
//
// The general process:
//
// First, we use a ErrnoState to track the state of errno function calls:
//
// REGISTER_LIST_WITH_PROGRAMSTATE(ErrnoStateValue, ErrnoState)
//
// (1) PreCalled: previous to any errno function calls.
//     - in this case, if any test for errno happened, 22.10 will report error.
//     - in this case, if a call to errno setting function happened, but errno
//     is not set to zero, 22.8 will report error.
//     - a call to errno setting function will change the state to (2).
// (2) AfterCalled: after an errno setting function call which return value
// cannot determined errno value, before any test for errno.
//     - in this case, if any call to subrouting happened, or return value of
//     function is used, 22.9 will report error.
//     - if the program reach the end of function, 22.9 will report error.
//     - a test between errno and zero will change the state back to (1)
// (3) AfterReturnDeterminedCalled: after a return value determined errno
// setting function call.
//     - in this case, if any call to subrouting happened, or the program reach
//     the end of function, 22.9 will report error.
//     - the return value of function can be used.
//     - a test between errno and zero, or test for return value, will
//     change the state back to (1).
// (4) AfterErrnoCalledAndReportError: after a errno setting function call
// which has report error. Used to avoid more unnecessary positive for 22.9.
//
// For 22.8, we use evalCall to model __errno_location(), and record the loc of
// errno value. We can use this ErrnoLocValue to get or set errno value:
//
// REGISTER_LIST_WITH_PROGRAMSTATE(ErrnoLocValue, DefinedSVal)
//
// For 22.9, the return value of function is recorded in
// ErrnoSetFunctionResultSet. After binding the return value to another
// variable(Lvalue to Rvalue), any load or store to the return value is
// prohibited. We will use checkBind and checkLocation to check this.
//
// REGISTER_SET_WITH_PROGRAMSTATE(ErrnoSetFunctionResultSet, SymbolRef)
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace ento;

namespace {

struct ErrnoState {
private:
  enum Kind {
    PreCalled,
    AfterCalled,
    AfterReturnDeterminedCalled,
    AfterErrnoCalledAndReportError
  } K;
  const CallEvent *call;
  ErrnoState(Kind InK, const CallEvent *Call) : K(InK), call(Call) {}

public:
  bool operator==(const ErrnoState &X) const { return K == X.K; }
  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(K); }
  bool isPreCalled() const { return K == PreCalled; }
  bool isAfterCalled() const { return K == AfterCalled; }
  bool isAfterReturnDeterminedCalled() const {
    return K == AfterReturnDeterminedCalled;
  }
  const CallEvent &getCallEvent() {
    assert(isAfterCalled() || isAfterReturnDeterminedCalled());
    return *call;
  }
  static ErrnoState getAfterErrnoCalledAndReportError() {
    return ErrnoState(AfterErrnoCalledAndReportError, nullptr);
  }
  static ErrnoState getPreCalled() { return ErrnoState(PreCalled, nullptr); }
  static ErrnoState getAfterReturnDeterminedCalled(const CallEvent &call) {
    return ErrnoState(AfterReturnDeterminedCalled, &call);
  }
  static ErrnoState getAfterCalled(const CallEvent &call) {
    return ErrnoState(AfterCalled, &call);
  }
};

class ErrnoFunctionChecker
    : public Checker<check::PostCall, eval::Call, check::LiveSymbols,
                     check::PostStmt<BinaryOperator>, check::Bind,
                     check::Location, check::EndFunction> {
  mutable std::unique_ptr<BuiltinBug> BT;
  void reportErrnoFuncPriorSet(const CallEvent &Call, CheckerContext &C) const;
  void reportErrnoFuncWithoutTest(const CallEvent &Call,
                                  CheckerContext &C) const;
  void reportErrnoTestMisuse(const BinaryOperator *B, CheckerContext &C) const;

public:
  struct ErrnoChecksFilter {
    bool CheckSetErrnoMisraChecker = false;
    bool CheckTestErrnoMisraChecker = false;
    bool CheckMisusedTestErrnoMisraChecker = false;

    CheckerNameRef CheckNameSetErrnoMisraChecker;
    CheckerNameRef CheckNameTestErrnoMisraChecker;
    CheckerNameRef CheckNameMisusedTestErrnoMisraChecker;
  };

  ErrnoChecksFilter Filter;
  // eval errno location call
  bool evalCall(const CallEvent &Call, CheckerContext &C) const;
  // check the errno setting functions and other functions
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  // mark errno as live, So it will not be auto removed
  void checkLiveSymbols(ProgramStateRef State, SymbolReaper &SR) const;
  // check for tested against zero
  void checkPostStmt(const BinaryOperator *B, CheckerContext &C) const;
  // If the state reaches end of function, but ErrnoState is AfterCalled.
  // an error of 22.9 should be reported.
  void checkEndFunction(const ReturnStmt *RS, CheckerContext &Ctx) const;
  // check for accessing the result of an errno setting function.
  void checkLocation(SVal Loc, bool IsLoad, const Stmt *S,
                     CheckerContext &) const;
  // we will allow binding the return value to a lvalue, but prohibit using it.
  void checkBind(SVal Loc, SVal Val, const Stmt *S, CheckerContext &) const;

private:
  // {{CallDescriptionFlags, func name, param count},
  // whether the return value determines errno }
  CallDescriptionMap<bool> ErrnoSetFuncList = {
      {{CDF_MaybeBuiltin, "ftell", 1}, true},
      {{CDF_MaybeBuiltin, "fgetpos", 2}, true},
      {{CDF_MaybeBuiltin, "fsetpos", 2}, true},
      {{CDF_MaybeBuiltin, "fgetwc", 1}, false},
      {{CDF_MaybeBuiltin, "fputwc", 2}, false},
      {{CDF_MaybeBuiltin, "strtoimax", 3}, false},
      {{CDF_MaybeBuiltin, "strtoumax", 3}, false},
      {{CDF_MaybeBuiltin, "strtol", 3}, false},
      {{CDF_MaybeBuiltin, "strtoul", 3}, false},
      {{CDF_MaybeBuiltin, "strtoll", 3}, false},
      {{CDF_MaybeBuiltin, "strtoull", 3}, false},
      {{CDF_MaybeBuiltin, "strtof", 2}, false},
      {{CDF_MaybeBuiltin, "strtod", 2}, false},
      {{CDF_MaybeBuiltin, "strtold", 2}, false},
      {{CDF_MaybeBuiltin, "wcstoimax", 3}, false},
      {{CDF_MaybeBuiltin, "wcstoumax", 3}, false},
      {{CDF_MaybeBuiltin, "wcstol", 3}, false},
      {{CDF_MaybeBuiltin, "wcstoul", 3}, false},
      {{CDF_MaybeBuiltin, "wcstoll", 3}, false},
      {{CDF_MaybeBuiltin, "wcstoull", 3}, false},
      {{CDF_MaybeBuiltin, "wcstof", 2}, false},
      {{CDF_MaybeBuiltin, "wcstod", 2}, false},
      {{CDF_MaybeBuiltin, "wcstold", 2}, false},
      {{CDF_MaybeBuiltin, "wcrtomb", 3}, false},
      {{CDF_MaybeBuiltin, "wcsrtombs", 4}, false},
      {{CDF_MaybeBuiltin, "mbrtowc", 4}, true},
  };
  CallDescriptionMap<bool> ErrnoLocationCalls = {
      {{CDF_MaybeBuiltin, "__errno_location", 0}, false},
      {{CDF_MaybeBuiltin, "___errno", 0}, false},
      {{CDF_MaybeBuiltin, "__errno", 0}, false},
      {{CDF_MaybeBuiltin, "_errno", 0}, false},
      {{CDF_MaybeBuiltin, "__error", 0}, false},
  };
  // whether the Expr is an Errno MACRO, like: (*__errno_location())
  bool isErrnoMacro(const Expr *E, CheckerContext &C) const;
  bool isZeroConstant(Expr *expr, CheckerContext &C) const;
  void checkTestErrnoSetFunctionResult(const BinaryOperator *B,
                                       CheckerContext &C) const;
  void checkErrnoTestWithZero(const BinaryOperator *B, CheckerContext &C) const;
  // get the Loc SVal in StreamSet for Errno: &errno
  DefinedOrUnknownSVal getErrnoLoc(ProgramStateRef &state) const;
  // get the int value in Errno
  DefinedOrUnknownSVal getErrnoValue(CheckerContext &C) const;
  // set the Errno state
  ProgramStateRef setErrnoState(ProgramStateRef &state, ErrnoState kind) const;
  // get the errno state
  ErrnoState getErrnoState(CheckerContext &C) const;
};
} // namespace

// A DefinedSVal is used to record the errno loc
REGISTER_LIST_WITH_PROGRAMSTATE(ErrnoLocValue, DefinedSVal)
REGISTER_LIST_WITH_PROGRAMSTATE(ErrnoStateValue, ErrnoState)
REGISTER_SET_WITH_PROGRAMSTATE(ErrnoSetFunctionResultSet, SymbolRef)

void ErrnoFunctionChecker::reportErrnoFuncPriorSet(const CallEvent &Call,
                                                   CheckerContext &C) const {
  if (!Filter.CheckSetErrnoMisraChecker) {
    return;
  }
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    if (!BT)
      BT.reset(new BuiltinBug(Filter.CheckNameSetErrnoMisraChecker,
                              "[misrac-2012-22.8]: ",
                              "call errno setting funtion without set errno"));
    auto R =
        std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);
    R->addRange(Call.getSourceRange());
    C.emitReport(std::move(R));
  }
}

void ErrnoFunctionChecker::reportErrnoFuncWithoutTest(const CallEvent &Call,
                                                      CheckerContext &C) const {
  if (!Filter.CheckTestErrnoMisraChecker) {
    return;
  }
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    if (!BT)
      BT.reset(new BuiltinBug(
          Filter.CheckNameTestErrnoMisraChecker, "[misrac-2012-22.9]: ",
          "miss errno value test after Errno setting function"));
    auto R =
        std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);
    R->addRange(Call.getSourceRange());
    C.emitReport(std::move(R));
  }
}

void ErrnoFunctionChecker::reportErrnoTestMisuse(const BinaryOperator *B,
                                                 CheckerContext &C) const {
  if (!Filter.CheckMisusedTestErrnoMisraChecker) {
    return;
  }
  if (ExplodedNode *N = C.generateNonFatalErrorNode()) {
    if (!BT)
      BT.reset(new BuiltinBug(
          Filter.CheckNameMisusedTestErrnoMisraChecker,
          "[misrac-2012-22.10]: ", "misuse of errno value test"));
    auto R =
        std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);
    R->addRange(B->getSourceRange());
    C.emitReport(std::move(R));
  }
}

bool ErrnoFunctionChecker::isErrnoMacro(const Expr *E,
                                        CheckerContext &C) const {
  const Preprocessor &PP = C.getPreprocessor();
  const SourceManager &SM = C.getSourceManager();
  const auto *MacroII = PP.getIdentifierInfo("errno");
  if (!MacroII) {
    return false;
  }
  const MacroInfo *MI = PP.getMacroInfo(MacroII);
  if (!MI) {
    return false;
  }
  const SourceLocation ErrnoSLoc = MI->getDefinitionLoc();
  const SourceLocation ExprLoc = SM.getSpellingLoc(E->getBeginLoc());
  return SM.getFileID(ExprLoc) == SM.getFileID(ErrnoSLoc) &&
         SM.getSpellingLineNumber(ExprLoc) ==
             SM.getSpellingLineNumber(ErrnoSLoc);
}

DefinedOrUnknownSVal
ErrnoFunctionChecker::getErrnoLoc(ProgramStateRef &state) const {
  // if we found a errno, return it;
  auto Entries = state->get<ErrnoLocValue>();
  if (Entries.isEmpty()) {
    // errno not found. No calls to __errno_location before.
    return UnknownVal();
  }
  return *Entries.begin();
}

ErrnoState ErrnoFunctionChecker::getErrnoState(CheckerContext &C) const {
  auto Entries = C.getState()->get<ErrnoStateValue>();
  if (Entries.isEmpty()) {
    // errno not found. No calls to __errno_location before.
    return ErrnoState::getPreCalled();
  }
  return *Entries.begin();
}

ProgramStateRef ErrnoFunctionChecker::setErrnoState(ProgramStateRef &state,
                                                    ErrnoState kind) const {
  auto Entries = state->get<ErrnoStateValue>();
  if (!Entries.isEmpty()) {
    state = state->remove<ErrnoStateValue>();
  }
  state = state->add<ErrnoStateValue>(kind);
  return state;
}

DefinedOrUnknownSVal
ErrnoFunctionChecker::getErrnoValue(CheckerContext &C) const {
  ProgramStateRef state = C.getState();

  // get errno value
  DefinedOrUnknownSVal errnoLoc = getErrnoLoc(state);
  auto ref = errnoLoc.getAs<Loc>();
  if (!ref) {
    return UnknownVal();
  }

  const MemRegion *region = ref->getAsRegion();
  if (!region) {
    return UnknownVal();
  }
  MemRegionManager &rm = state->getStateManager().getRegionManager();
  ASTContext &aCtx = C.getASTContext();
  auto &svalBuilder = C.getSValBuilder();
  NonLoc idx = svalBuilder.makeZeroArrayIndex();
  const ElementRegion *er =
      rm.getElementRegion(aCtx.IntTy, idx, dyn_cast<SubRegion>(region), aCtx);
  if (!er) {
    return UnknownVal();
  }
  SVal errnoSval = state->getSVal(er);
  // if we have errnoLoc defined, this is surely DefinedOrUnknownSVal.
  return errnoSval.castAs<DefinedOrUnknownSVal>();
}

void ErrnoFunctionChecker::checkLiveSymbols(ProgramStateRef State,
                                            SymbolReaper &SR) const {
  auto Entries = State->get<ErrnoLocValue>();

  // mark errno as live
  for (const auto ent : Entries) {
    SR.markLive(ent.getAsSymbol(true));
  }
}

bool ErrnoFunctionChecker::isZeroConstant(Expr *expr, CheckerContext &C) const {
  auto intc = expr->getIntegerConstantExpr(C.getASTContext());
  if (!intc) {
    return false;
  }
  if (intc->isNullValue()) {
    return true;
  }
  return false;
}

void ErrnoFunctionChecker::checkErrnoTestWithZero(const BinaryOperator *B,
                                                  CheckerContext &C) const {
  if ((isZeroConstant(B->getLHS(), C) && isErrnoMacro(B->getRHS(), C)) ||
      (isZeroConstant(B->getRHS(), C) && isErrnoMacro(B->getLHS(), C))) {
    if (getErrnoState(C).isPreCalled()) {
      reportErrnoTestMisuse(B, C);
    }
    auto state = C.getState();
    state = setErrnoState(state, ErrnoState::getPreCalled());
    C.addTransition(state);
  }
}

void ErrnoFunctionChecker::checkTestErrnoSetFunctionResult(
    const BinaryOperator *B, CheckerContext &C) const {
  auto state = C.getState();
  SVal rv = state->getSVal(B->getRHS(), C.getLocationContext());
  SVal lv = state->getSVal(B->getLHS(), C.getLocationContext());
  // Check whether we have test the return value. The return value which
  // represents error may not be zero(eg. -1), so we can ignore the other
  // constant.
  if (state->contains<ErrnoSetFunctionResultSet>(rv.getAsSymbol()) ||
      state->contains<ErrnoSetFunctionResultSet>(lv.getAsSymbol())) {
    if (getErrnoState(C).isAfterReturnDeterminedCalled()) {
      auto state = C.getState();
      state = setErrnoState(state, ErrnoState::getPreCalled());
      C.addTransition(state);
    }
  }
}

void ErrnoFunctionChecker::checkPostStmt(const BinaryOperator *B,
                                         CheckerContext &C) const {
  if (B->getOpcode() != BO_EQ && B->getOpcode() != BO_NE)
    return;
  checkErrnoTestWithZero(B, C);
  checkTestErrnoSetFunctionResult(B, C);
}

void ErrnoFunctionChecker::checkEndFunction(const ReturnStmt *RS,
                                            CheckerContext &C) const {
  auto ErrnoState = getErrnoState(C);
  if (ErrnoState.isAfterCalled() ||
      ErrnoState.isAfterReturnDeterminedCalled()) {
    // if the program reach the end of function without test errno, 22.9 will
    // report error.
    reportErrnoFuncWithoutTest(ErrnoState.getCallEvent(), C);
  }
  // for all errno rules in MISRA, set and tests shall happended in the same
  // function.
  auto state = C.getState()->remove<ErrnoLocValue>();
  state = state->remove<ErrnoSetFunctionResultSet>();
  state = state->remove<ErrnoStateValue>();
  C.addTransition(state);
}

void ErrnoFunctionChecker::checkLocation(SVal Loc, bool IsLoad, const Stmt *S,
                                         CheckerContext &C) const {
  auto ErrnoState = getErrnoState(C);
  // allow using the result from Return Determined Calls, but forbbiden the
  // result from others.
  if (ErrnoState.isAfterCalled()) {
    ProgramStateRef state = C.getState();
    if (state->contains<ErrnoSetFunctionResultSet>(Loc.getLocSymbolInBase())) {
      reportErrnoFuncWithoutTest(ErrnoState.getCallEvent(), C);
      auto state = C.getState();
      state =
          setErrnoState(state, ErrnoState::getAfterErrnoCalledAndReportError());
      C.addTransition(state);
    }
  }
}

void ErrnoFunctionChecker::checkBind(SVal Loc, SVal Val, const Stmt *S,
                                     CheckerContext &C) const {
  auto ErrnoState = getErrnoState(C);
  if (ErrnoState.isAfterCalled()) {
    ProgramStateRef state = C.getState();
    // Allow binding the return value to a lvalue, but will continue forbbiden
    // using it.
    if (state->contains<ErrnoSetFunctionResultSet>(Val.getAsSymbol())) {
      state = state->add<ErrnoSetFunctionResultSet>(Loc.getAsLocSymbol(true));
      C.addTransition(state);
    }
  }
}

bool ErrnoFunctionChecker::evalCall(const CallEvent &Call,
                                    CheckerContext &C) const {
  if (!ErrnoLocationCalls.lookup(Call)) {
    return false;
  }

  ProgramStateRef state = C.getState();
  const auto *CE = dyn_cast<CallExpr>(Call.getOriginExpr());
  if (!CE) {
    return false;
  }
  const LocationContext *LCtx = C.getLocationContext();

  // if we found a errno, return it;
  DefinedOrUnknownSVal sval = getErrnoLoc(state);
  if (!sval.isUnknown()) {
    state = state->BindExpr(CE, LCtx, sval);
    C.addTransition(state);
    return true;
  }

  // else we create a new errno. This will never be removed.
  // So only one errno in a function.
  auto &svalBuilder = C.getSValBuilder();
  // I haven't found out how to create a loc symbol value on
  // GlobalSystemSpaceRegion. So, a SymbolVal on heap is used instead.
  // TODO: using the CSA origin errno modeling when it's stabled.
  DefinedSVal RetVal =
      svalBuilder.getConjuredHeapSymbolVal(CE, LCtx, 1).castAs<DefinedSVal>();
  state = state->BindExpr(CE, LCtx, RetVal);
  state = state->add<ErrnoLocValue>(RetVal);
  C.addTransition(state);
  return true;
}

void ErrnoFunctionChecker::checkPostCall(const CallEvent &Call,
                                         CheckerContext &C) const {
  if (!ErrnoLocationCalls.lookup(Call)) {
    // If an errno setting function is called without a
    // test with zero, following any other function calls, an violation of 22.9
    // should be reported.
    auto ErrnoState = getErrnoState(C);
    if (ErrnoState.isAfterCalled() ||
        ErrnoState.isAfterReturnDeterminedCalled()) {
      reportErrnoFuncWithoutTest(Call, C);
      // set errno state to After Error to avoid more unnecessary error reports.
      auto state = C.getState();
      state =
          setErrnoState(state, ErrnoState::getAfterErrnoCalledAndReportError());
      C.addTransition(state);
    }
  }
  const bool *isReturnDetermined = ErrnoSetFuncList.lookup(Call);
  if (isReturnDetermined == NULL) {
    // not an errno setting function, ignored.
    return;
  }

  // This is an errno setting function. Trying to get errno value.
  ProgramStateRef state = C.getState();
  DefinedOrUnknownSVal errnoVal = getErrnoValue(C);
  if (errnoVal.isUnknown()) {
    reportErrnoFuncPriorSet(Call, C);
    return;
  }
  DefinedSVal dval = errnoVal.castAs<DefinedSVal>();
  if (!state->assume(dval, false) || state->assume(dval, true)) {
    // if errno value is not zero, report error. (22.8)
    reportErrnoFuncPriorSet(Call, C);
  }

  // if getErrnoValue success, this is sure to be DefinedSVal.
  Loc errnoLoc = getErrnoLoc(state).castAs<Loc>();
  // set the errno value back to UnknownVal.
  state = state->bindLoc(errnoLoc, UnknownVal(), C.getLocationContext());

  // clear the previous return value set.
  SVal returnVal = Call.getReturnValue();
  state = state->remove<ErrnoSetFunctionResultSet>();
  state = state->add<ErrnoSetFunctionResultSet>(returnVal.getAsSymbol());
  if (!*isReturnDetermined) {
    // This is an errno setting function, so ErrnoState is set to AfterCalled.
    state = setErrnoState(state, ErrnoState::getAfterCalled(Call));
  } else {
    // The return value of function can be used to determined errno.
    // no need to be tested.
    state =
        setErrnoState(state, ErrnoState::getAfterReturnDeterminedCalled(Call));
  }
  C.addTransition(state);
}

void ento::registerErrnoFunctionChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<ErrnoFunctionChecker>();
}

bool ento::shouldRegisterErrnoFunctionChecker(const CheckerManager &mgr) {
  return true;
}

#define REGISTER_CHECKER(name)                                                 \
  void ento::register##name(CheckerManager &mgr) {                             \
    ErrnoFunctionChecker *checker = mgr.getChecker<ErrnoFunctionChecker>();    \
    checker->Filter.Check##name = true;                                        \
    checker->Filter.CheckName##name = mgr.getCurrentCheckerName();             \
  }                                                                            \
                                                                               \
  bool ento::shouldRegister##name(const CheckerManager &mgr) { return true; }

REGISTER_CHECKER(SetErrnoMisraChecker)
REGISTER_CHECKER(TestErrnoMisraChecker)
REGISTER_CHECKER(MisusedTestErrnoMisraChecker)
