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

//=== cwe-124-127-BufferUnderAccessChecker.cpp ------------------*- C++ -*-===//
//
// The checker that is responsible for both CWE-124 and CWE-127.
//
// We will check all dereferences, if they are Element MemRegion,
// including a[k] or *(a+k), in the callback checkLocation().
// For library functions, we check the args that we are interested in.
//
//===----------------------------------------------------------------------===//

#include "InterCheckerAPI.h"
#include "clang/Basic/CharInfo.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Checkers/NaiveCStdLibFunctionsInfo.h"
#include "clang/StaticAnalyzer/Checkers/Taint.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramStateTrait.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SVals.h"
#include "clang/include/clang/StaticAnalyzer/Core/Checker.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <functional>
#include <stdexcept>

using namespace clang;
using namespace ento;
using namespace naive;
using namespace std::placeholders;

namespace {

constexpr llvm::StringLiteral MsgNegWrite =
    "Try to write to memory that may be prior to beginning of the buffer."
    "(CWE-124: Buffer Underwrite)";
constexpr llvm::StringLiteral MsgNegRead =
    "Try to read memory that may be prior to beginning of the buffer."
    "(CWE-127: Buffer Underread)";

class BufferUnderAccessChecker
    : public Checker<check::PreCall, check::Location> {
  enum AccessType { Read, Write };

public:
  struct BufferUnderAccessFilter {
    bool CheckBufferUnderread = false;
    bool CheckBufferUnderwrite = false;
  } Filter;

  BufferUnderAccessChecker();

  std::unique_ptr<BugType> BufferUnderAccessBugType;
  void checkLocation(SVal Loc, bool IsLoad, const Stmt *S,
                     CheckerContext &) const;
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;

private:
  void checkNegElement(CheckerContext &C, SVal Loc, AccessType Type) const;
  void emitBug(CheckerContext &C, ProgramStateRef errorState,
               AccessType Type) const;
};

// FIXME: Eventually replace RegionRawOffset with this class.
class RegionRawOffsetV2 {
private:
  const SubRegion *baseRegion;
  SVal byteOffset;

  RegionRawOffsetV2() : baseRegion(nullptr), byteOffset(UnknownVal()) {}

public:
  RegionRawOffsetV2(const SubRegion *base, SVal offset)
      : baseRegion(base), byteOffset(offset) {}

  NonLoc getByteOffset() const { return byteOffset.castAs<NonLoc>(); }
  const SubRegion *getRegion() const { return baseRegion; }

  static RegionRawOffsetV2
  computeOffset(ProgramStateRef state, SValBuilder &svalBuilder, SVal location);
};
} // end anonymous namespace

static SVal computeExtentBegin(SValBuilder &svalBuilder,
                               const MemRegion *region) {
  // If we only get a char*, assume it's begin
  return svalBuilder.makeZeroArrayIndex();
}

// TODO: once the constraint manager is smart enough to handle non simplified
// symbolic expressions remove this function. Note that this can not be used in
// the constraint manager as is, since this does not handle overflows. It is
// safe to assume, however, that memory offsets will not overflow.
static std::pair<NonLoc, nonloc::ConcreteInt>
getSimplifiedOffsets(NonLoc offset, nonloc::ConcreteInt extent,
                     SValBuilder &svalBuilder) {
  Optional<nonloc::SymbolVal> SymVal = offset.getAs<nonloc::SymbolVal>();
  if (SymVal && SymVal->isExpression()) {
    if (const SymIntExpr *SIE = dyn_cast<SymIntExpr>(SymVal->getSymbol())) {
      llvm::APSInt constant =
          APSIntType(extent.getValue()).convert(SIE->getRHS());
      switch (SIE->getOpcode()) {
      case BO_Mul:
        // The constant should never be 0 here, since it the result of scaling
        // based on the size of a type which is never 0.
        if ((extent.getValue() % constant) != 0)
          return std::pair<NonLoc, nonloc::ConcreteInt>(offset, extent);
        else
          return getSimplifiedOffsets(
              nonloc::SymbolVal(SIE->getLHS()),
              svalBuilder.makeIntVal(extent.getValue() / constant),
              svalBuilder);
      case BO_Add:
        return getSimplifiedOffsets(
            nonloc::SymbolVal(SIE->getLHS()),
            svalBuilder.makeIntVal(extent.getValue() - constant), svalBuilder);
      default:
        break;
      }
    }
  }

  return std::pair<NonLoc, nonloc::ConcreteInt>(offset, extent);
}

// Check a MemRegion may read negative index
void BufferUnderAccessChecker::checkNegElement(CheckerContext &C, SVal Loc,
                                               AccessType Type) const {
  // NOTE: Instead of using ProgramState::assumeInBound(), we are prototyping
  // some new logic here that reasons directly about memory region extents.
  // Once that logic is more mature, we can bring it back to assumeInBound()
  // for all clients to use.
  //
  // The algorithm we are using here for bounds checking is to see if the
  // memory access is within the extent of the base region.  Since we
  // have some flexibility in defining the base region, we can achieve
  // various levels of conservatism in our buffer overflow checking.
  ProgramStateRef state = C.getState();

  SValBuilder &svalBuilder = C.getSValBuilder();
  const RegionRawOffsetV2 &rawOffset =
      RegionRawOffsetV2::computeOffset(state, svalBuilder, Loc);

  if (!rawOffset.getRegion())
    return;

  NonLoc rawOffsetVal = rawOffset.getByteOffset();

  // CHECK LOWER BOUND: Is byteOffset < extent begin?
  //  If so, we are doing a load/store
  //  before the first valid offset in the memory region.

  SVal extentBegin = computeExtentBegin(svalBuilder, rawOffset.getRegion());

  if (Optional<NonLoc> NV = extentBegin.getAs<NonLoc>()) {
    if (auto ConcreteNV = NV->getAs<nonloc::ConcreteInt>()) {
      std::pair<NonLoc, nonloc::ConcreteInt> simplifiedOffsets =
          getSimplifiedOffsets(rawOffset.getByteOffset(), *ConcreteNV,
                               svalBuilder);
      rawOffsetVal = simplifiedOffsets.first;
      *NV = simplifiedOffsets.second;
    }

    SVal lowerBound = svalBuilder.evalBinOpNN(state, BO_LT, rawOffsetVal, *NV,
                                              svalBuilder.getConditionType());

    Optional<NonLoc> lowerBoundToCheck = lowerBound.getAs<NonLoc>();
    if (!lowerBoundToCheck)
      return;

    ProgramStateRef state_precedesLowerBound, state_withinLowerBound;
    std::tie(state_precedesLowerBound, state_withinLowerBound) =
        state->assume(*lowerBoundToCheck);

    // Are we constrained enough to definitely precede the lower bound?
    if (state_precedesLowerBound) {
      emitBug(C, state_precedesLowerBound, Type);
      return;
    }

    // Otherwise, assume the constraint of the lower bound.
    assert(state_withinLowerBound);
    state = state_withinLowerBound;
  }
}

void BufferUnderAccessChecker::checkLocation(SVal Loc, bool IsLoad,
                                             const Stmt *S,
                                             CheckerContext &C) const {
  if (!IsLoad && Filter.CheckBufferUnderwrite) // cwe124 only cares write
    checkNegElement(C, Loc, Write);
  if (IsLoad && Filter.CheckBufferUnderread) // cwe127 only cares read
    checkNegElement(C, Loc, Read);
}

void BufferUnderAccessChecker::checkPreCall(const CallEvent &Call,
                                            CheckerContext &C) const {
  const CallExpr *CE = dyn_cast_or_null<CallExpr>(Call.getOriginExpr());
  if (!CE)
    return;
  const std::pair<ArgSet, ArgSet> *Args = FuncArgsMayReadOrWrite.lookup(Call);
  if (!Args)
    return;

  // Evaluate the args that the function may access.
  auto CheckArg = [&](const ArgSet &Args, const AccessType &Type) {
    for (ArgIdxTy i = 0; i != Call.getNumArgs(); i++) {
      if (Args.contains(i)) {
        SVal arg = Call.getArgSVal(i);
        checkNegElement(C, arg, Type);
      }
    }
  };
  if (Filter.CheckBufferUnderread)
    CheckArg(Args->first, Read);
  if (Filter.CheckBufferUnderwrite)
    CheckArg(Args->second, Write);
}

void BufferUnderAccessChecker::emitBug(CheckerContext &C,
                                       ProgramStateRef errorState,
                                       AccessType Type) const {
  if (ExplodedNode *ErrNode = C.generateErrorNode()) {
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BufferUnderAccessBugType, Type == Write ? MsgNegWrite : MsgNegRead,
        ErrNode);
    C.emitReport(std::move(R));
  }
}

BufferUnderAccessChecker::BufferUnderAccessChecker() {
  BufferUnderAccessBugType.reset(
      new BugType(this, "Buffer UnderAccess", "Out-of-bounds Access Error"));
}

// Lazily computes a value to be used by 'computeOffset'.  If 'val'
// is unknown or undefined, we lazily substitute '0'.  Otherwise,
// return 'val'.
static inline SVal getValue(SVal val, SValBuilder &svalBuilder) {
  return val.isUndef() ? svalBuilder.makeZeroArrayIndex() : val;
}

// Scale a base value by a scaling factor, and return the scaled
// value as an SVal.  Used by 'computeOffset'.
static inline SVal scaleValue(ProgramStateRef state, NonLoc baseVal,
                              CharUnits scaling, SValBuilder &sb) {
  return sb.evalBinOpNN(state, BO_Mul, baseVal,
                        sb.makeArrayIndex(scaling.getQuantity()),
                        sb.getArrayIndexType());
}

// Add an SVal to another, treating unknown and undefined values as
// summing to UnknownVal.  Used by 'computeOffset'.
static SVal addValue(ProgramStateRef state, SVal x, SVal y,
                     SValBuilder &svalBuilder) {
  // We treat UnknownVals and UndefinedVals the same here because we
  // only care about computing offsets.
  if (x.isUnknownOrUndef() || y.isUnknownOrUndef())
    return UnknownVal();

  return svalBuilder.evalBinOpNN(state, BO_Add, x.castAs<NonLoc>(),
                                 y.castAs<NonLoc>(),
                                 svalBuilder.getArrayIndexType());
}

/// Compute a raw byte offset from a base region.  Used for array bounds
/// checking.
RegionRawOffsetV2 RegionRawOffsetV2::computeOffset(ProgramStateRef state,
                                                   SValBuilder &svalBuilder,
                                                   SVal location) {
  const MemRegion *region = location.getAsRegion();
  SVal offset = UndefinedVal();

  while (region) {
    switch (region->getKind()) {
    default: {
      if (const SubRegion *subReg = dyn_cast<SubRegion>(region)) {
        offset = getValue(offset, svalBuilder);
        if (!offset.isUnknownOrUndef())
          return RegionRawOffsetV2(subReg, offset);
      }
      return RegionRawOffsetV2();
    }
    case MemRegion::ElementRegionKind: {
      const ElementRegion *elemReg = cast<ElementRegion>(region);
      SVal index = elemReg->getIndex();
      if (!isa<NonLoc>(index))
        return RegionRawOffsetV2();
      QualType elemType = elemReg->getElementType();
      // If the element is an incomplete type, go no further.
      ASTContext &astContext = svalBuilder.getContext();
      if (elemType->isIncompleteType())
        return RegionRawOffsetV2();

      // Update the offset.
      offset = addValue(state, getValue(offset, svalBuilder),
                        scaleValue(state, index.castAs<NonLoc>(),
                                   astContext.getTypeSizeInChars(elemType),
                                   svalBuilder),
                        svalBuilder);

      if (offset.isUnknownOrUndef())
        return RegionRawOffsetV2();

      region = elemReg->getSuperRegion();
      continue;
    }
    }
  }
  return RegionRawOffsetV2();
}

void ento::registerBufferUnderwriteChecker(CheckerManager &Mgr) {
  BufferUnderAccessChecker *checker =
      Mgr.registerChecker<BufferUnderAccessChecker>();
  checker->Filter.CheckBufferUnderwrite = true;
}

bool ento::shouldRegisterBufferUnderwriteChecker(const CheckerManager &mgr) {
  return true;
}

void ento::registerBufferUnderreadChecker(CheckerManager &Mgr) {
  BufferUnderAccessChecker *checker =
      Mgr.registerChecker<BufferUnderAccessChecker>();
  checker->Filter.CheckBufferUnderread = true;
}

bool ento::shouldRegisterBufferUnderreadChecker(const CheckerManager &mgr) {
  return true;
}
