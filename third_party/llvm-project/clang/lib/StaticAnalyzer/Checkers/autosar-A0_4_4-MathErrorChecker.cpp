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

//=== autosar-A0_4_4-MathErrorChecker.cpp -----------------------*- C++ -*-===//
//
// Implementation of autosar A0_4_4.
// MathErrorChecker reports errors when using math functions but not checking
// range, domain and pole errors.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include <cfenv>

using namespace clang;
using namespace ento;

namespace {

class MathErrorChecker : public Checker<check::PostCall, check::EndFunction> {
  mutable std::unique_ptr<BuiltinBug> BT;
  void reportBug(ExplodedNode *N, CheckerContext &C,
                 std::unique_ptr<BugReporterVisitor> Visitor = nullptr) const;

  // int is the error flag of the function
  CallDescriptionMap<int> mathFns = {
      // Functions not subject to any error conditions in math_errhandling:
      // abs, fabs, fabsf, fabsl, nan, nanf, nanl
      // fmax, fmaxf, fmaxl, fmin, fminf, fminl
      // ceil, ceilf, ceill, floor, floorf, floorl
      // trunc, truncf, truncl, nearbyint, nearbyintf, nearbyintl
      // frexp, frexpf, frexpl, modf, modff, modfl
      // copysign, copysignf, copysignl
      {{"fmod", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"fmodf", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"fmodl", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"remainder", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"remaindef", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"remaindel", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"remquo", 3}, FE_INVALID | FE_UNDERFLOW},
      {{"remquof", 3}, FE_INVALID | FE_UNDERFLOW},
      {{"remquol", 3}, FE_INVALID | FE_UNDERFLOW},
      {{"fma", 3}, FE_INVALID | FE_UNDERFLOW | FE_OVERFLOW},
      {{"fmaf", 3}, FE_INVALID | FE_UNDERFLOW | FE_OVERFLOW},
      {{"fmal", 3}, FE_INVALID | FE_UNDERFLOW | FE_OVERFLOW},
      {{"fdim", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"fdimf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"fdiml", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"exp", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"expf", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"expl", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"exp2", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"exp2f", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"exp2l", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"expm1", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"expm1f", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"expm1l", 1}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"log", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"logf", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"logl", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log10", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log10f", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log10l", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log2", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log2f", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log2l", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"log1p", 1}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW},
      {{"log1pf", 1}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW},
      {{"log1pl", 1}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW},
      {{"pow", 2}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW},
      {{"powf", 2}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW},
      {{"powl", 2}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW},
      {{"sqrt", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"sqrtf", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"sqrtl", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"cbrt", 1}, FE_UNDERFLOW},
      {{"cbrtf", 1}, FE_UNDERFLOW},
      {{"cbrtl", 1}, FE_UNDERFLOW},
      {{"hypot", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"hypotf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"hypotl", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"sin", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"sinf", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"sinl", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"cos", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"cosf", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"cosl", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"tan", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"tanf", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"tanl", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"asin", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"asinf", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"asinl", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"acos", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"acosf", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"acosl", 1}, FE_INVALID | FE_UNDERFLOW},
      {{"atan", 1}, FE_UNDERFLOW},
      {{"atanf", 1}, FE_UNDERFLOW},
      {{"atanl", 1}, FE_UNDERFLOW},
      {{"atan2", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"atan2f", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"atan2l", 2}, FE_INVALID | FE_UNDERFLOW},
      {{"erf", 1}, FE_UNDERFLOW},
      {{"erff", 1}, FE_UNDERFLOW},
      {{"erfl", 1}, FE_UNDERFLOW},
      {{"erfc", 1}, FE_UNDERFLOW},
      {{"erfcf", 1}, FE_UNDERFLOW},
      {{"erfcl", 1}, FE_UNDERFLOW},
      {{"tgamma", 1}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW},
      {{"tgammaf", 1}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW},
      {{"tgammal", 1}, FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW},
      {{"lgamma", 1}, FE_DIVBYZERO | FE_OVERFLOW},
      {{"lgammaf", 1}, FE_DIVBYZERO | FE_OVERFLOW},
      {{"lgammal", 1}, FE_DIVBYZERO | FE_OVERFLOW},
      {{"round", 1}, FE_INVALID},
      {{"roundf", 1}, FE_INVALID},
      {{"roundl", 1}, FE_INVALID},
      {{"lround", 1}, FE_INVALID},
      {{"lroundf", 1}, FE_INVALID},
      {{"lroundl", 1}, FE_INVALID},
      {{"llround", 1}, FE_INVALID},
      {{"llroundf", 1}, FE_INVALID},
      {{"int", 1}, FE_INVALID},
      {{"intf", 1}, FE_INVALID},
      {{"intl", 1}, FE_INVALID},
      {{"lint", 1}, FE_INVALID},
      {{"lintf", 1}, FE_INVALID},
      {{"lintl", 1}, FE_INVALID},
      {{"llint", 1}, FE_INVALID},
      {{"llintf", 1}, FE_INVALID},
      {{"ldexp", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"ldexpf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"ldexpl", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"scalbn", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"scalbnf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"scalbnl", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"scalbln", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"scalblnf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"scalblnl", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"ilogb", 1}, FE_INVALID},
      {{"ilogbf", 1}, FE_INVALID},
      {{"ilogbl", 1}, FE_INVALID},
      {{"logb", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"logbf", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"logbl", 1}, FE_INVALID | FE_DIVBYZERO},
      {{"nextafter", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"nextafterf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"nextafterl", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"nexttoward", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"nexttowardf", 2}, FE_UNDERFLOW | FE_OVERFLOW},
      {{"nexttowardl", 2}, FE_UNDERFLOW | FE_OVERFLOW},
  };
  // function "fetestexcept" checks the math error flag
  CallDescription checkFn = {CDF_MaybeBuiltin, "fetestexcept", 1};
  ProgramStateRef reportErrorAndClearState(ProgramStateRef state,
                                           CheckerContext &C) const;

public:
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  void checkEndFunction(const ReturnStmt *RS, CheckerContext &C) const;
};
} // namespace

REGISTER_TRAIT_WITH_PROGRAMSTATE(ErrorFlag, int); // flags to be checked
REGISTER_TRAIT_WITH_PROGRAMSTATE(ErrorNode, ExplodedNode *); // error node

void MathErrorChecker::reportBug(
    ExplodedNode *N, CheckerContext &C,
    std::unique_ptr<BugReporterVisitor> Visitor) const {
  if (!BT)
    BT.reset(new BuiltinBug(this, "Unchecked range, domain and pole errors"));

  auto R = std::make_unique<PathSensitiveBugReport>(
      *BT, "Unchecked range, domain and pole errors", N);
  R->addVisitor(std::move(Visitor));
  C.emitReport(std::move(R));
}

ProgramStateRef
MathErrorChecker::reportErrorAndClearState(ProgramStateRef state,
                                           CheckerContext &C) const {
  // Report error if the state has some error node and unchecked flags
  if (ExplodedNode *N = state->get<ErrorNode>())
    if (state->get<ErrorFlag>() != 0)
      reportBug(N, C);

  // Remove the current error node
  state = state->remove<ErrorFlag>();
  state = state->remove<ErrorNode>();
  return state;
}

void MathErrorChecker::checkPostCall(const CallEvent &Call,
                                     CheckerContext &C) const {
  ProgramStateRef state = C.getState();
  if (const int *flag = mathFns.lookup(Call)) {
    // Report current error node if needed
    state = reportErrorAndClearState(state, C);
    // Set the current error node
    state = state->set<ErrorFlag>(*flag);
    state = state->set<ErrorNode>(C.generateNonFatalErrorNode(state));
    C.addTransition(state);
  }

  if (checkFn.matches(Call)) {
    // There is no math function to be checked
    if (!state->get<ErrorNode>())
      return;

    // Get the integer flag n of fetestexcept(n)
    const Expr *E = Call.getArgExpr(0);
    Expr::EvalResult result;
    if (E->EvaluateAsInt(result, C.getASTContext())) {
      if (result.Val.isInt()) {
        int flag = state->get<ErrorFlag>();
        int checkedFlag = result.Val.getInt().getExtValue();

        ProgramStateRef stateBad, stateGood;
        // In checkPostCall, the return value is always defined
        std::tie(stateBad, stateGood) =
            state->assume(Call.getReturnValue().castAs<DefinedOrUnknownSVal>());
        // On succuss, remove checkedFlag from flag
        stateGood = stateGood->set<ErrorFlag>(flag - (flag & checkedFlag));
        // On failure, it is okay to end function now, no need to report error
        // Example: if (!fetestexcept(FE_INVALID)) return; (good)
        stateBad = stateBad->remove<ErrorNode>();
        C.addTransition(stateGood);
        C.addTransition(stateBad);
      }
    }
  }
}

void MathErrorChecker::checkEndFunction(const ReturnStmt *RS,
                                        CheckerContext &C) const {
  reportErrorAndClearState(C.getState(), C);
}

void ento::registerMathErrorChecker(CheckerManager &mgr) {
  mgr.registerChecker<MathErrorChecker>();
}

bool ento::shouldRegisterMathErrorChecker(const CheckerManager &mgr) {
  return true;
}
