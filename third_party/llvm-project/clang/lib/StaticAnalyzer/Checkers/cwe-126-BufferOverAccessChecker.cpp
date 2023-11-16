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

//=== cwe-126-BufferOverAccessChecker.cpp -----------------------*- C++ -*-===//
//
// The checker is reponsible for CWE-126.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/CharUnits.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/APSIntType.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace ento;

namespace {
class BufferOverAccessChecker :
    public Checker<check::Location> {
  mutable std::unique_ptr<BuiltinBug> BT;

  void reportOOB(CheckerContext &C, ProgramStateRef errorState,
                 std::unique_ptr<BugReporterVisitor> Visitor = nullptr) const;

public:
  void checkLocation(SVal l, bool isLoad, const Stmt*S,
                     CheckerContext &C) const;
};

// FIXME: Eventually replace RegionRawOffset with this class.
class RegionRawOffsetV2 {
private:
  const SubRegion *baseRegion;
  SVal byteOffset;

  RegionRawOffsetV2()
    : baseRegion(nullptr), byteOffset(UnknownVal()) {}

public:
  RegionRawOffsetV2(const SubRegion* base, SVal offset)
    : baseRegion(base), byteOffset(offset) {}

  NonLoc getByteOffset() const { return byteOffset.castAs<NonLoc>(); }
  const SubRegion *getRegion() const { return baseRegion; }

  static RegionRawOffsetV2 computeOffset(ProgramStateRef state,
                                         SValBuilder &svalBuilder,
                                         SVal location);

  void dump() const;
  void dumpToStream(raw_ostream &os) const;
};
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

void BufferOverAccessChecker::checkLocation(SVal location, bool isLoad,
                                        const Stmt* LoadS,
                                        CheckerContext &checkerContext) const {
  if (!isLoad) // CWE-126 only cares about read
    return;

  // NOTE: Instead of using ProgramState::assumeInBound(), we are prototyping
  // some new logic here that reasons directly about memory region extents.
  // Once that logic is more mature, we can bring it back to assumeInBound()
  // for all clients to use.
  //
  // The algorithm we are using here for bounds checking is to see if the
  // memory access is within the extent of the base region.  Since we
  // have some flexibility in defining the base region, we can achieve
  // various levels of conservatism in our buffer overflow checking.
  ProgramStateRef state = checkerContext.getState();

  SValBuilder &svalBuilder = checkerContext.getSValBuilder();
  const RegionRawOffsetV2 &rawOffset =
    RegionRawOffsetV2::computeOffset(state, svalBuilder, location);

  if (!rawOffset.getRegion())
    return;

  NonLoc rawOffsetVal = rawOffset.getByteOffset();

  do {
    // CHECK UPPER BOUND: Is byteOffset >= size(baseRegion)?  If so,
    // we are doing a load/store after the last valid offset.
    const MemRegion *MR = rawOffset.getRegion();
    DefinedOrUnknownSVal Size = getDynamicExtent(state, MR, svalBuilder);
    if (!isa<NonLoc>(Size))
      break;

    if (auto ConcreteSize = Size.getAs<nonloc::ConcreteInt>()) {
      std::pair<NonLoc, nonloc::ConcreteInt> simplifiedOffsets =
          getSimplifiedOffsets(rawOffset.getByteOffset(), *ConcreteSize,
                               svalBuilder);
      rawOffsetVal = simplifiedOffsets.first;
      Size = simplifiedOffsets.second;
    }

    SVal upperbound = svalBuilder.evalBinOpNN(state, BO_GE, rawOffsetVal,
                                              Size.castAs<NonLoc>(),
                                              svalBuilder.getConditionType());

    Optional<NonLoc> upperboundToCheck = upperbound.getAs<NonLoc>();
    if (!upperboundToCheck)
      break;

    ProgramStateRef state_exceedsUpperBound, state_withinUpperBound;
    std::tie(state_exceedsUpperBound, state_withinUpperBound) =
      state->assume(*upperboundToCheck);

    if (state_exceedsUpperBound) {
      if(state_withinUpperBound) return;
      reportOOB(checkerContext, state_exceedsUpperBound);
      return;
    }

    assert(state_withinUpperBound);
    state = state_withinUpperBound;
  }
  while (false);

  checkerContext.addTransition(state);
}

void BufferOverAccessChecker::reportOOB(
    CheckerContext &checkerContext, ProgramStateRef errorState,
    std::unique_ptr<BugReporterVisitor> Visitor) const {

  ExplodedNode *errorNode = checkerContext.generateErrorNode(errorState);
  if (!errorNode)
    return;

  if (!BT)
    BT.reset(new BuiltinBug(this, "Out-of-bound access"));

  // FIXME: This diagnostics are preliminary.  We should get far better
  // diagnostics for explaining buffer overruns.

  SmallString<256> buf;
  llvm::raw_svector_ostream os(buf);
  os << "Out of bound memory access ";

  auto BR = std::make_unique<PathSensitiveBugReport>(*BT, os.str(), errorNode);
  BR->addVisitor(std::move(Visitor));
  checkerContext.emitReport(std::move(BR));
}

#ifndef NDEBUG
LLVM_DUMP_METHOD void RegionRawOffsetV2::dump() const {
  dumpToStream(llvm::errs());
}

void RegionRawOffsetV2::dumpToStream(raw_ostream &os) const {
  os << "raw_offset_v2{" << getRegion() << ',' << getByteOffset() << '}';
}
#endif

// Lazily computes a value to be used by 'computeOffset'.  If 'val'
// is unknown or undefined, we lazily substitute '0'.  Otherwise,
// return 'val'.
static inline SVal getValue(SVal val, SValBuilder &svalBuilder) {
  return val.isUndef() ? svalBuilder.makeZeroArrayIndex() : val;
}

// Scale a base value by a scaling factor, and return the scaled
// value as an SVal.  Used by 'computeOffset'.
static inline SVal scaleValue(ProgramStateRef state,
                              NonLoc baseVal, CharUnits scaling,
                              SValBuilder &sb) {
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
                                                   SVal location)
{
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
        offset = addValue(state,
                          getValue(offset, svalBuilder),
                          scaleValue(state,
                          index.castAs<NonLoc>(),
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

void ento::registerBufferOverreadChecker(CheckerManager &mgr) {
  mgr.registerChecker<BufferOverAccessChecker>();
}

bool ento::shouldRegisterBufferOverreadChecker(const CheckerManager &mgr) {
  return true;
}
