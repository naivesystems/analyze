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

//=== misrac-2012-21_17-CStringBoundChecker.cpp - Checks calls to C string
// functions ---*- C++ -*-===//
//
// According to rule 21.17, an access beyond the bounds of the objects may
// occur in:
// - a write to the dest buffer (this is supported by origin CStringChecker)
// - a read to the buffer as there is no null terminator.
//
// This checker is modified on top of the original clang static checker
// CStringChecker.cpp. A read bound check and many new functions support
// are added, while some memory related function and overlap checker
// are removed.
//
// The original checker not only checks the validity of arguments, it also
// models functions behaviors because the source code of library functions may
// not be available for analysis. For rule 21.17, we add many new functions but
// will not model their functions behaviors currently.
//
// Add a new helper function:
// - checkNullInBuffer
//   - return false if the buffer cannot be check
//     (mostly buffer is a wrong type) or the buffer is not null terminated.
//   - else return true
// the checker will report warning if the checkNullInBuffer returns false.
// checkNullInBuffer is called in getCStringLength.
//
// The following string handling C functions are currently supported for 21.17:
//
//   strcat    strchr   strcmp
//   strcoll   strcpy   strcspn
//   strlen    strpbrk  strrchr
//   strspn    strstr
//   strtok
//
//===----------------------------------------------------------------------===//

#include "InterCheckerAPI.h"
#include "clang/Basic/CharInfo.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramStateTrait.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace ento;

namespace {
struct AnyArgExpr {
  // FIXME: Remove constructor in C++17 to turn it into an aggregate.
  AnyArgExpr(const Expr *Expression, unsigned ArgumentIndex)
      : Expression{Expression}, ArgumentIndex{ArgumentIndex} {}
  const Expr *Expression;
  unsigned ArgumentIndex;
};

struct SourceArgExpr : AnyArgExpr {
  using AnyArgExpr::AnyArgExpr; // FIXME: Remove using in C++17.
};

struct DestinationArgExpr : AnyArgExpr {
  using AnyArgExpr::AnyArgExpr; // FIXME: Same.
};

struct SizeArgExpr : AnyArgExpr {
  using AnyArgExpr::AnyArgExpr; // FIXME: Same.
};

using ErrorMessage = SmallString<128>;
enum class AccessKind { write, read };

static ErrorMessage createOutOfBoundErrorMsg(StringRef FunctionDescription,
                                             AccessKind Access) {
  ErrorMessage Message;
  llvm::raw_svector_ostream Os(Message);

  // Function classification like: Memory copy function
  Os << toUppercase(FunctionDescription.front())
     << &FunctionDescription.data()[1];

  if (Access == AccessKind::write) {
    Os << " overflows the destination buffer";
  } else { // read access
    Os << " accesses out-of-bound array element";
  }

  return Message;
}

enum class ConcatFnKind { none = 0, strcat = 1, strlcat = 2 };
class CStringBoundMisraChecker
    : public Checker<eval::Call, check::PreStmt<DeclStmt>, check::LiveSymbols,
                     check::DeadSymbols, check::RegionChanges> {
  mutable std::unique_ptr<BugType> BT_Null, BT_Bounds, BT_Overlap,
      BT_NotCString, BT_AdditionOverflow;

  mutable const char *CurrentFunctionDescription;

public:
  static void *getTag() {
    static int tag;
    return &tag;
  }

  bool evalCall(const CallEvent &Call, CheckerContext &C) const;
  void checkPreStmt(const DeclStmt *DS, CheckerContext &C) const;
  void checkLiveSymbols(ProgramStateRef state, SymbolReaper &SR) const;
  void checkDeadSymbols(SymbolReaper &SR, CheckerContext &C) const;

  ProgramStateRef
  checkRegionChanges(ProgramStateRef state, const InvalidatedSymbols *,
                     ArrayRef<const MemRegion *> ExplicitRegions,
                     ArrayRef<const MemRegion *> Regions,
                     const LocationContext *LCtx, const CallEvent *Call) const;

  typedef void (CStringBoundMisraChecker::*FnCheck)(CheckerContext &,
                                                    const CallExpr *) const;
  CallDescriptionMap<FnCheck> Callbacks = {
      {{CDF_MaybeBuiltin, "strcpy", 2}, &CStringBoundMisraChecker::evalStrcpy},
      {{CDF_MaybeBuiltin, "strncpy", 3},
       &CStringBoundMisraChecker::evalStrncpy},
      {{CDF_MaybeBuiltin, "stpcpy", 2}, &CStringBoundMisraChecker::evalStpcpy},
      {{CDF_MaybeBuiltin, "strlcpy", 3},
       &CStringBoundMisraChecker::evalStrlcpy},
      {{CDF_MaybeBuiltin, "strcat", 2}, &CStringBoundMisraChecker::evalStrcat},
      {{CDF_MaybeBuiltin, "strncat", 3},
       &CStringBoundMisraChecker::evalStrncat},
      {{CDF_MaybeBuiltin, "strlcat", 3},
       &CStringBoundMisraChecker::evalStrlcat},
      {{CDF_MaybeBuiltin, "strlen", 1},
       &CStringBoundMisraChecker::evalstrLength},
      {{CDF_MaybeBuiltin, "strnlen", 2},
       &CStringBoundMisraChecker::evalstrnLength},
      {{CDF_MaybeBuiltin, "strcmp", 2}, &CStringBoundMisraChecker::evalStrcmp},
      {{CDF_MaybeBuiltin, "strncmp", 3},
       &CStringBoundMisraChecker::evalStrncmp},
      {{CDF_MaybeBuiltin, "strcasecmp", 2},
       &CStringBoundMisraChecker::evalStrcasecmp},
      {{CDF_MaybeBuiltin, "strncasecmp", 3},
       &CStringBoundMisraChecker::evalStrncasecmp},
      {{CDF_MaybeBuiltin, "strchr", 2}, &CStringBoundMisraChecker::evalStrchr},
      {{CDF_MaybeBuiltin, "strrchr", 2}, &CStringBoundMisraChecker::evalStrchr},
      {{CDF_MaybeBuiltin, "strcoll", 2}, &CStringBoundMisraChecker::evalStrcmp},
      {{CDF_MaybeBuiltin, "strspn", 2}, &CStringBoundMisraChecker::evalStrspn},
      {{CDF_MaybeBuiltin, "strcspn", 2}, &CStringBoundMisraChecker::evalStrspn},
      {{CDF_MaybeBuiltin, "strstr", 2}, &CStringBoundMisraChecker::evalStrstr},
      {{CDF_MaybeBuiltin, "strtok", 2}, &CStringBoundMisraChecker::evalStrtok},
      {{CDF_MaybeBuiltin, "strpbrk", 2},
       &CStringBoundMisraChecker::evalStrpbrk},
      {{CDF_MaybeBuiltin, "strsep", 2}, &CStringBoundMisraChecker::evalStrsep},
  };

  // These require a bit of special handling.
  CallDescription StdCopy{{"std", "copy"}, 3},
      StdCopyBackward{{"std", "copy_backward"}, 3};

  FnCheck identifyCall(const CallEvent &Call, CheckerContext &C) const;

  void evalstrLength(CheckerContext &C, const CallExpr *CE) const;
  void evalstrnLength(CheckerContext &C, const CallExpr *CE) const;
  void evalstrLengthCommon(CheckerContext &C, const CallExpr *CE,
                           bool IsStrnlen = false) const;

  void evalStrcpy(CheckerContext &C, const CallExpr *CE) const;
  void evalStrncpy(CheckerContext &C, const CallExpr *CE) const;
  void evalStpcpy(CheckerContext &C, const CallExpr *CE) const;
  void evalStrlcpy(CheckerContext &C, const CallExpr *CE) const;
  void evalStrcpyCommon(CheckerContext &C, const CallExpr *CE, bool ReturnEnd,
                        bool IsBounded, ConcatFnKind appendK,
                        bool returnPtr = true) const;

  void evalStrcat(CheckerContext &C, const CallExpr *CE) const;
  void evalStrncat(CheckerContext &C, const CallExpr *CE) const;
  void evalStrlcat(CheckerContext &C, const CallExpr *CE) const;

  void evalStrcmp(CheckerContext &C, const CallExpr *CE) const;
  void evalStrncmp(CheckerContext &C, const CallExpr *CE) const;
  void evalStrcasecmp(CheckerContext &C, const CallExpr *CE) const;
  void evalStrncasecmp(CheckerContext &C, const CallExpr *CE) const;
  void evalStrcmpCommon(CheckerContext &C, const CallExpr *CE,
                        bool IsBounded = false, bool IgnoreCase = false) const;

  void evalStrchr(CheckerContext &C, const CallExpr *CE) const;
  void evalStrspn(CheckerContext &C, const CallExpr *CE) const;
  void evalStrtok(CheckerContext &C, const CallExpr *CE) const;
  void evalStrstr(CheckerContext &C, const CallExpr *CE) const;
  void evalStrpbrk(CheckerContext &C, const CallExpr *CE) const;
  void evalStrsep(CheckerContext &C, const CallExpr *CE) const;

  // Utility methods
  std::pair<ProgramStateRef, ProgramStateRef> static assumeZero(
      CheckerContext &C, ProgramStateRef state, SVal V, QualType Ty);

  static ProgramStateRef setCStringLength(ProgramStateRef state,
                                          const MemRegion *MR, SVal strLength);
  static SVal getCStringLengthForRegion(CheckerContext &C,
                                        ProgramStateRef &state, const Expr *Ex,
                                        const MemRegion *MR, bool hypothetical);
  SVal getCStringLength(CheckerContext &C, ProgramStateRef &state,
                        const Expr *Ex, SVal Buf,
                        bool hypothetical = false) const;

  const StringLiteral *getCStringLiteral(CheckerContext &C,
                                         ProgramStateRef &state,
                                         const Expr *expr, SVal val) const;

  static ProgramStateRef InvalidateBuffer(CheckerContext &C,
                                          ProgramStateRef state, const Expr *Ex,
                                          SVal V, bool IsSourceBuffer,
                                          const Expr *Size);

  static bool SummarizeRegion(raw_ostream &os, ASTContext &Ctx,
                              const MemRegion *MR);
  // return false if the buffer cannot be check(mostly buffer is a wrong type)
  // or the buffer is not null terminated.
  static bool checkNullInBuffer(SVal Buffer, CheckerContext &C);

  // Re-usable checks
  ProgramStateRef checkNonNull(CheckerContext &C, ProgramStateRef State,
                               AnyArgExpr Arg, SVal l) const;
  ProgramStateRef CheckLocation(CheckerContext &C, ProgramStateRef state,
                                AnyArgExpr Buffer, SVal Element,
                                AccessKind Access) const;

  void emitNullArgBug(CheckerContext &C, ProgramStateRef State, const Stmt *S,
                      StringRef WarningMsg) const;
  void emitOutOfBoundsBug(CheckerContext &C, ProgramStateRef State,
                          const Stmt *S, StringRef WarningMsg) const;
  void emitNotCStringBug(CheckerContext &C, ProgramStateRef State,
                         const Stmt *S, StringRef WarningMsg) const;

  // Return true if the destination buffer of the copy function may be in bound.
  // Expects SVal of Size to be positive and unsigned.
  // Expects SVal of FirstBuf to be a FieldRegion.
  static bool IsFirstBufInBound(CheckerContext &C, ProgramStateRef state,
                                const Expr *FirstBuf, const Expr *Size);
};

} // end anonymous namespace

REGISTER_MAP_WITH_PROGRAMSTATE(CStringLength, const MemRegion *, SVal)

//===----------------------------------------------------------------------===//
// Individual checks and utility methods.
//===----------------------------------------------------------------------===//

std::pair<ProgramStateRef, ProgramStateRef>
CStringBoundMisraChecker::assumeZero(CheckerContext &C, ProgramStateRef state,
                                     SVal V, QualType Ty) {
  Optional<DefinedSVal> val = V.getAs<DefinedSVal>();
  if (!val)
    return std::pair<ProgramStateRef, ProgramStateRef>(state, state);

  SValBuilder &svalBuilder = C.getSValBuilder();
  DefinedOrUnknownSVal zero = svalBuilder.makeZeroVal(Ty);
  return state->assume(svalBuilder.evalEQ(state, *val, zero));
}

ProgramStateRef CStringBoundMisraChecker::checkNonNull(CheckerContext &C,
                                                       ProgramStateRef State,
                                                       AnyArgExpr Arg,
                                                       SVal l) const {
  // If a previous check has failed, propagate the failure.
  if (!State)
    return nullptr;

  ProgramStateRef stateNull, stateNonNull;
  std::tie(stateNull, stateNonNull) =
      assumeZero(C, State, l, Arg.Expression->getType());

  if (stateNull && !stateNonNull) {
    SmallString<80> buf;
    llvm::raw_svector_ostream OS(buf);
    assert(CurrentFunctionDescription);
    OS << "Null pointer passed as " << (Arg.ArgumentIndex + 1)
       << llvm::getOrdinalSuffix(Arg.ArgumentIndex + 1) << " argument to "
       << CurrentFunctionDescription;

    emitNullArgBug(C, stateNull, Arg.Expression, OS.str());
    return nullptr;
  }

  // From here on, assume that the value is non-null.
  assert(stateNonNull);
  return stateNonNull;
}

// FIXME: This was originally copied from ArrayBoundChecker.cpp. Refactor?
ProgramStateRef CStringBoundMisraChecker::CheckLocation(
    CheckerContext &C, ProgramStateRef state, AnyArgExpr Buffer, SVal Element,
    AccessKind Access) const {

  // If a previous check has failed, propagate the failure.
  if (!state)
    return nullptr;

  // Check for out of bound array element access.
  const MemRegion *R = Element.getAsRegion();
  if (!R)
    return state;

  const auto *ER = dyn_cast<ElementRegion>(R);
  if (!ER)
    return state;

  if (ER->getValueType() != C.getASTContext().CharTy)
    return state;

  // Get the size of the array.
  const auto *superReg = cast<SubRegion>(ER->getSuperRegion());
  DefinedOrUnknownSVal Size =
      getDynamicExtent(state, superReg, C.getSValBuilder());

  // Get the index of the accessed element.
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();

  ProgramStateRef StInBound = state->assumeInBound(Idx, Size, true);
  ProgramStateRef StOutBound = state->assumeInBound(Idx, Size, false);
  if (StOutBound && !StInBound) {

    // Emit a bug report.
    ErrorMessage Message =
        createOutOfBoundErrorMsg(CurrentFunctionDescription, Access);
    emitOutOfBoundsBug(C, StOutBound, Buffer.Expression, Message);
    return nullptr;
  }

  // Array bound check succeeded.  From this point forward the array bound
  // should always succeed.
  return StInBound;
}

void CStringBoundMisraChecker::emitNullArgBug(CheckerContext &C,
                                              ProgramStateRef State,
                                              const Stmt *S,
                                              StringRef WarningMsg) const {
  if (ExplodedNode *N = C.generateErrorNode(State)) {
    if (!BT_Null)
      BT_Null.reset(new BuiltinBug(
          this, categories::UnixAPI,
          "Null pointer argument in call to byte string function"));

    BuiltinBug *BT = static_cast<BuiltinBug *>(BT_Null.get());
    std::string misra_msg =
        std::string("[misrac-2012-21.17]: ") + std::string(WarningMsg);
    auto Report = std::make_unique<PathSensitiveBugReport>(*BT, misra_msg, N);
    Report->addRange(S->getSourceRange());
    if (const auto *Ex = dyn_cast<Expr>(S))
      bugreporter::trackExpressionValue(N, Ex, *Report);
    C.emitReport(std::move(Report));
  }
}

void CStringBoundMisraChecker::emitOutOfBoundsBug(CheckerContext &C,
                                                  ProgramStateRef State,
                                                  const Stmt *S,
                                                  StringRef WarningMsg) const {
  if (ExplodedNode *N = C.generateErrorNode(State)) {
    if (!BT_Bounds)
      BT_Bounds.reset(new BuiltinBug(
          this, "Out-of-bound array access",
          "Byte string function accesses out-of-bound array element"));

    BuiltinBug *BT = static_cast<BuiltinBug *>(BT_Bounds.get());

    // FIXME: It would be nice to eventually make this diagnostic more clear,
    // e.g., by referencing the original declaration or by saying *why* this
    // reference is outside the range.
    std::string misra_msg =
        std::string("[misrac-2012-21.17]: ") + std::string(WarningMsg);
    auto Report = std::make_unique<PathSensitiveBugReport>(*BT, misra_msg, N);
    Report->addRange(S->getSourceRange());
    C.emitReport(std::move(Report));
  }
}

void CStringBoundMisraChecker::emitNotCStringBug(CheckerContext &C,
                                                 ProgramStateRef State,
                                                 const Stmt *S,
                                                 StringRef WarningMsg) const {
  if (ExplodedNode *N = C.generateNonFatalErrorNode(State)) {
    if (!BT_NotCString)
      BT_NotCString.reset(
          new BuiltinBug(this, categories::UnixAPI,
                         "Argument is not a null-terminated string."));
    std::string misra_msg =
        std::string("[misrac-2012-21.17]: ") + std::string(WarningMsg);
    auto Report =
        std::make_unique<PathSensitiveBugReport>(*BT_NotCString, misra_msg, N);

    Report->addRange(S->getSourceRange());
    C.emitReport(std::move(Report));
  }
}

ProgramStateRef CStringBoundMisraChecker::setCStringLength(
    ProgramStateRef state, const MemRegion *MR, SVal strLength) {
  assert(!strLength.isUndef() && "Attempt to set an undefined string length");

  MR = MR->StripCasts();

  switch (MR->getKind()) {
  case MemRegion::StringRegionKind:
    // FIXME: This can happen if we strcpy() into a string region. This is
    // undefined [C99 6.4.5p6], but we should still warn about it.
    return state;

  case MemRegion::SymbolicRegionKind:
  case MemRegion::AllocaRegionKind:
  case MemRegion::NonParamVarRegionKind:
  case MemRegion::ParamVarRegionKind:
  case MemRegion::FieldRegionKind:
  case MemRegion::ObjCIvarRegionKind:
    // These are the types we can currently track string lengths for.
    break;

  case MemRegion::ElementRegionKind:
    // FIXME: Handle element regions by upper-bounding the parent region's
    // string length.
    return state;

  default:
    // Other regions (mostly non-data) can't have a reliable C string length.
    // For now, just ignore the change.
    // FIXME: These are rare but not impossible. We should output some kind of
    // warning for things like strcpy((char[]){'a', 0}, "b");
    return state;
  }

  if (strLength.isUnknown())
    return state->remove<CStringLength>(MR);

  return state->set<CStringLength>(MR, strLength);
}

SVal CStringBoundMisraChecker::getCStringLengthForRegion(CheckerContext &C,
                                                         ProgramStateRef &state,
                                                         const Expr *Ex,
                                                         const MemRegion *MR,
                                                         bool hypothetical) {
  if (!hypothetical) {
    // If there's a recorded length, go ahead and return it.
    const SVal *Recorded = state->get<CStringLength>(MR);
    if (Recorded)
      return *Recorded;
  }

  // Otherwise, get a new symbol and update the state.
  SValBuilder &svalBuilder = C.getSValBuilder();
  QualType sizeTy = svalBuilder.getContext().getSizeType();
  SVal strLength = svalBuilder.getMetadataSymbolVal(
      CStringBoundMisraChecker::getTag(), MR, Ex, sizeTy,
      C.getLocationContext(), C.blockCount());

  if (!hypothetical) {
    if (Optional<NonLoc> strLn = strLength.getAs<NonLoc>()) {
      // In case of unbounded calls strlen etc bound the range to SIZE_MAX/4
      BasicValueFactory &BVF = svalBuilder.getBasicValueFactory();
      const llvm::APSInt &maxValInt = BVF.getMaxValue(sizeTy);
      llvm::APSInt fourInt = APSIntType(maxValInt).getValue(4);
      const llvm::APSInt *maxLengthInt =
          BVF.evalAPSInt(BO_Div, maxValInt, fourInt);
      NonLoc maxLength = svalBuilder.makeIntVal(*maxLengthInt);
      SVal evalLength =
          svalBuilder.evalBinOpNN(state, BO_LE, *strLn, maxLength, sizeTy);
      state = state->assume(evalLength.castAs<DefinedOrUnknownSVal>(), true);
    }
    state = state->set<CStringLength>(MR, strLength);
  }

  return strLength;
}

bool CStringBoundMisraChecker::checkNullInBuffer(SVal Buffer,
                                                 CheckerContext &C) {
  const MemRegion *R = Buffer.getAsRegion();
  // buffer cannot be casted into an ElementRegion,
  // so null can't be checked in a loop through the buffer.
  if (!isa_and_nonnull<ElementRegion>(R)) {
    // ignore unknown or undefined buffer value
    if (R &&
        !isa_and_nonnull<SymbolicRegion>(R)
        // string regions always have correct length
        // (see the comments in getCStringLength)
        && !isa_and_nonnull<StringRegion>(R)) {
      return false;
    }
    // still report warning here to avoid false negetive
    return true;
  }
  const ElementRegion *ER = dyn_cast<ElementRegion>(R);
  if (!ER)
    return true;

  const MemRegion *SR = dyn_cast<ElementRegion>(R)->getSuperRegion();
  if (!isa_and_nonnull<SubRegion>(SR)) {
    return true;
  }
  MemRegionManager &MRM = SR->getMemRegionManager();

  ASTContext &ACtx = C.getASTContext();
  SValBuilder &SVB = C.getSValBuilder();

  QualType CharTy = ACtx.CharTy;

  ProgramStateRef state = C.getState();

  // build the loop index
  NonLoc Idx = SVB.makeZeroArrayIndex();
  NonLoc Step = SVB.makeArrayIndex(1);
  QualType IdxTy = Idx.getType(ACtx);

  // Get the size of the array.
  DefinedOrUnknownSVal ElementCount = getDynamicElementCount(
      state, ER->getSuperRegion(), C.getSValBuilder(), ER->getValueType());

  ProgramStateRef StInBound = state->assumeInBound(Idx, ElementCount, true);
  ProgramStateRef StOutBound = state->assumeInBound(Idx, ElementCount, false);
  while (StInBound && !StOutBound) {
    const ElementRegion *ER =
        MRM.getElementRegion(CharTy, Idx, dyn_cast<SubRegion>(SR), ACtx);
    SVal Val = state->getSVal(ER);

    if (!Val.isUnknownOrUndef()) {
      ProgramStateRef stateNull, stateNonNull;
      std::tie(stateNull, stateNonNull) = assumeZero(C, state, Val, CharTy);
      if (stateNull) {
        return true;
      }
    }
    auto newIdx =
        SVB.evalBinOp(state, BO_Add, Idx, Step, IdxTy).getAs<NonLoc>();
    Idx = newIdx.getValue();
    StInBound = state->assumeInBound(Idx, ElementCount, true);
    StOutBound = state->assumeInBound(Idx, ElementCount, false);
  }
  return false;
}

SVal CStringBoundMisraChecker::getCStringLength(CheckerContext &C,
                                                ProgramStateRef &state,
                                                const Expr *Ex, SVal Buf,
                                                bool hypothetical) const {
  const MemRegion *MR = Buf.getAsRegion();
  if (!MR) {
    // If we can't get a region, see if it's something we /know/ isn't a
    // C string. In the context of locations, the only time we can issue such
    // a warning is for labels.
    if (Optional<loc::GotoLabel> Label = Buf.getAs<loc::GotoLabel>()) {
      SmallString<120> buf;
      llvm::raw_svector_ostream os(buf);
      assert(CurrentFunctionDescription);
      os << "Argument to " << CurrentFunctionDescription
         << " is the address of the label '" << Label->getLabel()->getName()
         << "', which is not a null-terminated string";

      emitNotCStringBug(C, state, Ex, os.str());
      return UndefinedVal();
    }

    // If it's not a region and not a label, give up.
    return UnknownVal();
  }
  if (!checkNullInBuffer(Buf, C)) {
    SmallString<120> buf;
    llvm::raw_svector_ostream os(buf);
    assert(CurrentFunctionDescription);
    os << CurrentFunctionDescription << " violates misra_c_2012: rule_21_17";
    emitNotCStringBug(C, state, Ex, os.str());
    return UndefinedVal();
  }

  // If we have a region, strip casts from it and see if we can figure out
  // its length. For anything we can't figure out, just return UnknownVal.
  MR = MR->StripCasts();

  switch (MR->getKind()) {
  case MemRegion::StringRegionKind: {
    // Modifying the contents of string regions is undefined [C99 6.4.5p6],
    // so we can assume that the byte length is the correct C string length.
    SValBuilder &svalBuilder = C.getSValBuilder();
    QualType sizeTy = svalBuilder.getContext().getSizeType();
    const StringLiteral *strLit = cast<StringRegion>(MR)->getStringLiteral();
    return svalBuilder.makeIntVal(strLit->getByteLength(), sizeTy);
  }
  case MemRegion::SymbolicRegionKind:
  case MemRegion::AllocaRegionKind:
  case MemRegion::NonParamVarRegionKind:
  case MemRegion::ParamVarRegionKind:
  case MemRegion::FieldRegionKind:
  case MemRegion::ObjCIvarRegionKind:
    return getCStringLengthForRegion(C, state, Ex, MR, hypothetical);
  case MemRegion::CompoundLiteralRegionKind:
    // FIXME: Can we track this? Is it necessary?
    return UnknownVal();
  case MemRegion::ElementRegionKind:
    // FIXME: How can we handle this? It's not good enough to subtract the
    // offset from the base string length; consider "123\x00567" and &a[5].
    return UnknownVal();
  default:
    // Other regions (mostly non-data) can't have a reliable C string length.
    // In this case, an error is emitted and UndefinedVal is returned.
    // The caller should always be prepared to handle this case.
    SmallString<120> buf;
    llvm::raw_svector_ostream os(buf);

    assert(CurrentFunctionDescription);
    os << "Argument to " << CurrentFunctionDescription << " is ";

    if (SummarizeRegion(os, C.getASTContext(), MR))
      os << ", which is not a null-terminated string";
    else
      os << "not a null-terminated string";

    emitNotCStringBug(C, state, Ex, os.str());
    return UndefinedVal();
  }
}

const StringLiteral *
CStringBoundMisraChecker::getCStringLiteral(CheckerContext &C,
                                            ProgramStateRef &state,
                                            const Expr *expr, SVal val) const {

  // Get the memory region pointed to by the val.
  const MemRegion *bufRegion = val.getAsRegion();
  if (!bufRegion)
    return nullptr;

  // Strip casts off the memory region.
  bufRegion = bufRegion->StripCasts();

  // Cast the memory region to a string region.
  const StringRegion *strRegion = dyn_cast<StringRegion>(bufRegion);
  if (!strRegion)
    return nullptr;

  // Return the actual string in the string region.
  return strRegion->getStringLiteral();
}

bool CStringBoundMisraChecker::IsFirstBufInBound(CheckerContext &C,
                                                 ProgramStateRef state,
                                                 const Expr *FirstBuf,
                                                 const Expr *Size) {
  // If we do not know that the buffer is long enough we return 'true'.
  // Otherwise the parent region of this field region would also get
  // invalidated, which would lead to warnings based on an unknown state.

  // Originally copied from CheckBufferAccess and CheckLocation.
  SValBuilder &svalBuilder = C.getSValBuilder();
  ASTContext &Ctx = svalBuilder.getContext();
  const LocationContext *LCtx = C.getLocationContext();

  QualType sizeTy = Size->getType();
  QualType PtrTy = Ctx.getPointerType(Ctx.CharTy);
  SVal BufVal = state->getSVal(FirstBuf, LCtx);

  SVal LengthVal = state->getSVal(Size, LCtx);
  Optional<NonLoc> Length = LengthVal.getAs<NonLoc>();
  if (!Length)
    return true; // cf top comment.

  // Compute the offset of the last element to be accessed: size-1.
  NonLoc One = svalBuilder.makeIntVal(1, sizeTy).castAs<NonLoc>();
  SVal Offset = svalBuilder.evalBinOpNN(state, BO_Sub, *Length, One, sizeTy);
  if (Offset.isUnknown())
    return true; // cf top comment
  NonLoc LastOffset = Offset.castAs<NonLoc>();

  // Check that the first buffer is sufficiently long.
  SVal BufStart = svalBuilder.evalCast(BufVal, PtrTy, FirstBuf->getType());
  Optional<Loc> BufLoc = BufStart.getAs<Loc>();
  if (!BufLoc)
    return true; // cf top comment.

  SVal BufEnd =
      svalBuilder.evalBinOpLN(state, BO_Add, *BufLoc, LastOffset, PtrTy);

  // Check for out of bound array element access.
  const MemRegion *R = BufEnd.getAsRegion();
  if (!R)
    return true; // cf top comment.

  const ElementRegion *ER = dyn_cast<ElementRegion>(R);
  if (!ER)
    return true; // cf top comment.

  // FIXME: Does this crash when a non-standard definition
  // of a library function is encountered?
  assert(ER->getValueType() == C.getASTContext().CharTy &&
         "IsFirstBufInBound should only be called with char* ElementRegions");

  // Get the size of the array.
  const SubRegion *superReg = cast<SubRegion>(ER->getSuperRegion());
  DefinedOrUnknownSVal SizeDV = getDynamicExtent(state, superReg, svalBuilder);

  // Get the index of the accessed element.
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();

  ProgramStateRef StInBound = state->assumeInBound(Idx, SizeDV, true);

  return static_cast<bool>(StInBound);
}

ProgramStateRef CStringBoundMisraChecker::InvalidateBuffer(
    CheckerContext &C, ProgramStateRef state, const Expr *E, SVal V,
    bool IsSourceBuffer, const Expr *Size) {
  Optional<Loc> L = V.getAs<Loc>();
  if (!L)
    return state;

  // FIXME: This is a simplified version of what's in CFRefCount.cpp -- it makes
  // some assumptions about the value that CFRefCount can't. Even so, it should
  // probably be refactored.
  if (Optional<loc::MemRegionVal> MR = L->getAs<loc::MemRegionVal>()) {
    const MemRegion *R = MR->getRegion()->StripCasts();

    // Are we dealing with an ElementRegion?  If so, we should be invalidating
    // the super-region.
    if (const ElementRegion *ER = dyn_cast<ElementRegion>(R)) {
      R = ER->getSuperRegion();
      // FIXME: What about layers of ElementRegions?
    }

    // Invalidate this region.
    const LocationContext *LCtx = C.getPredecessor()->getLocationContext();

    bool CausesPointerEscape = false;
    RegionAndSymbolInvalidationTraits ITraits;
    // Invalidate and escape only indirect regions accessible through the source
    // buffer.
    if (IsSourceBuffer) {
      ITraits.setTrait(R->getBaseRegion(),
                       RegionAndSymbolInvalidationTraits::TK_PreserveContents);
      ITraits.setTrait(R, RegionAndSymbolInvalidationTraits::TK_SuppressEscape);
      CausesPointerEscape = true;
    } else {
      const MemRegion::Kind &K = R->getKind();
      if (K == MemRegion::FieldRegionKind)
        if (Size && IsFirstBufInBound(C, state, E, Size)) {
          // If destination buffer is a field region and access is in bound,
          // do not invalidate its super region.
          ITraits.setTrait(
              R,
              RegionAndSymbolInvalidationTraits::TK_DoNotInvalidateSuperRegion);
        }
    }

    return state->invalidateRegions(R, E, C.blockCount(), LCtx,
                                    CausesPointerEscape, nullptr, nullptr,
                                    &ITraits);
  }

  // If we have a non-region value by chance, just remove the binding.
  // FIXME: is this necessary or correct? This handles the non-Region
  //  cases.  Is it ever valid to store to these?
  return state->killBinding(*L);
}

bool CStringBoundMisraChecker::SummarizeRegion(raw_ostream &os, ASTContext &Ctx,
                                               const MemRegion *MR) {
  switch (MR->getKind()) {
  case MemRegion::FunctionCodeRegionKind: {
    if (const auto *FD = cast<FunctionCodeRegion>(MR)->getDecl())
      os << "the address of the function '" << *FD << '\'';
    else
      os << "the address of a function";
    return true;
  }
  case MemRegion::BlockCodeRegionKind:
    os << "block text";
    return true;
  case MemRegion::BlockDataRegionKind:
    os << "a block";
    return true;
  case MemRegion::CXXThisRegionKind:
  case MemRegion::CXXTempObjectRegionKind:
    os << "a C++ temp object of type "
       << cast<TypedValueRegion>(MR)->getValueType().getAsString();
    return true;
  case MemRegion::NonParamVarRegionKind:
    os << "a variable of type"
       << cast<TypedValueRegion>(MR)->getValueType().getAsString();
    return true;
  case MemRegion::ParamVarRegionKind:
    os << "a parameter of type"
       << cast<TypedValueRegion>(MR)->getValueType().getAsString();
    return true;
  case MemRegion::FieldRegionKind:
    os << "a field of type "
       << cast<TypedValueRegion>(MR)->getValueType().getAsString();
    return true;
  case MemRegion::ObjCIvarRegionKind:
    os << "an instance variable of type "
       << cast<TypedValueRegion>(MR)->getValueType().getAsString();
    return true;
  default:
    return false;
  }
}

//===----------------------------------------------------------------------===//
// evaluation of individual function calls.
//===----------------------------------------------------------------------===//

void CStringBoundMisraChecker::evalstrLength(CheckerContext &C,
                                             const CallExpr *CE) const {
  // size_t strlen(const char *s);
  evalstrLengthCommon(C, CE, /* IsStrnlen = */ false);
}

void CStringBoundMisraChecker::evalstrnLength(CheckerContext &C,
                                              const CallExpr *CE) const {
  // size_t strnlen(const char *s, size_t maxlen);
  evalstrLengthCommon(C, CE, /* IsStrnlen = */ true);
}

void CStringBoundMisraChecker::evalstrLengthCommon(CheckerContext &C,
                                                   const CallExpr *CE,
                                                   bool IsStrnlen) const {
  CurrentFunctionDescription = "string length function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  if (IsStrnlen) {
    const Expr *maxlenExpr = CE->getArg(1);
    SVal maxlenVal = state->getSVal(maxlenExpr, LCtx);

    ProgramStateRef stateZeroSize, stateNonZeroSize;
    std::tie(stateZeroSize, stateNonZeroSize) =
        assumeZero(C, state, maxlenVal, maxlenExpr->getType());

    // If the size can be zero, the result will be 0 in that case, and we don't
    // have to check the string itself.
    if (stateZeroSize) {
      SVal zero = C.getSValBuilder().makeZeroVal(CE->getType());
      stateZeroSize = stateZeroSize->BindExpr(CE, LCtx, zero);
      C.addTransition(stateZeroSize);
    }

    // If the size is GUARANTEED to be zero, we're done!
    if (!stateNonZeroSize)
      return;

    // Otherwise, record the assumption that the size is nonzero.
    state = stateNonZeroSize;
  }

  // Check that the string argument is non-null.
  AnyArgExpr Arg = {CE->getArg(0), 0};
  SVal ArgVal = state->getSVal(Arg.Expression, LCtx);
  state = checkNonNull(C, state, Arg, ArgVal);

  if (!state)
    return;

  SVal strLength = getCStringLength(C, state, Arg.Expression, ArgVal);

  // If the argument isn't a valid C string, there's no valid state to
  // transition to.
  if (strLength.isUndef())
    return;

  DefinedOrUnknownSVal result = UnknownVal();

  // If the check is for strnlen() then bind the return value to no more than
  // the maxlen value.
  if (IsStrnlen) {
    QualType cmpTy = C.getSValBuilder().getConditionType();

    // It's a little unfortunate to be getting this again,
    // but it's not that expensive...
    const Expr *maxlenExpr = CE->getArg(1);
    SVal maxlenVal = state->getSVal(maxlenExpr, LCtx);

    Optional<NonLoc> strLengthNL = strLength.getAs<NonLoc>();
    Optional<NonLoc> maxlenValNL = maxlenVal.getAs<NonLoc>();

    if (strLengthNL && maxlenValNL) {
      ProgramStateRef stateStringTooLong, stateStringNotTooLong;

      // Check if the strLength is greater than the maxlen.
      std::tie(stateStringTooLong, stateStringNotTooLong) = state->assume(
          C.getSValBuilder()
              .evalBinOpNN(state, BO_GT, *strLengthNL, *maxlenValNL, cmpTy)
              .castAs<DefinedOrUnknownSVal>());

      if (stateStringTooLong && !stateStringNotTooLong) {
        // If the string is longer than maxlen, return maxlen.
        result = *maxlenValNL;
      } else if (stateStringNotTooLong && !stateStringTooLong) {
        // If the string is shorter than maxlen, return its length.
        result = *strLengthNL;
      }
    }

    if (result.isUnknown()) {
      // If we don't have enough information for a comparison, there's
      // no guarantee the full string length will actually be returned.
      // All we know is the return value is the min of the string length
      // and the limit. This is better than nothing.
      result = C.getSValBuilder().conjureSymbolVal(nullptr, CE, LCtx,
                                                   C.blockCount());
      NonLoc resultNL = result.castAs<NonLoc>();

      if (strLengthNL) {
        state = state->assume(
            C.getSValBuilder()
                .evalBinOpNN(state, BO_LE, resultNL, *strLengthNL, cmpTy)
                .castAs<DefinedOrUnknownSVal>(),
            true);
      }

      if (maxlenValNL) {
        state = state->assume(
            C.getSValBuilder()
                .evalBinOpNN(state, BO_LE, resultNL, *maxlenValNL, cmpTy)
                .castAs<DefinedOrUnknownSVal>(),
            true);
      }
    }

  } else {
    // This is a plain strlen(), not strnlen().
    result = strLength.castAs<DefinedOrUnknownSVal>();

    // If we don't know the length of the string, conjure a return
    // value, so it can be used in constraints, at least.
    if (result.isUnknown()) {
      result = C.getSValBuilder().conjureSymbolVal(nullptr, CE, LCtx,
                                                   C.blockCount());
    }
  }

  // Bind the return value.
  assert(!result.isUnknown() && "Should have conjured a value by now");
  state = state->BindExpr(CE, LCtx, result);
  C.addTransition(state);
}

void CStringBoundMisraChecker::evalStrcpy(CheckerContext &C,
                                          const CallExpr *CE) const {
  // char *strcpy(char *restrict dst, const char *restrict src);
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ false,
                   /* IsBounded = */ false,
                   /* appendK = */ ConcatFnKind::none);
}

void CStringBoundMisraChecker::evalStrncpy(CheckerContext &C,
                                           const CallExpr *CE) const {
  // char *strncpy(char *restrict dst, const char *restrict src, size_t n);
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ false,
                   /* IsBounded = */ true,
                   /* appendK = */ ConcatFnKind::none);
}

void CStringBoundMisraChecker::evalStpcpy(CheckerContext &C,
                                          const CallExpr *CE) const {
  // char *stpcpy(char *restrict dst, const char *restrict src);
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ true,
                   /* IsBounded = */ false,
                   /* appendK = */ ConcatFnKind::none);
}

void CStringBoundMisraChecker::evalStrlcpy(CheckerContext &C,
                                           const CallExpr *CE) const {
  // size_t strlcpy(char *dest, const char *src, size_t size);
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ true,
                   /* IsBounded = */ true,
                   /* appendK = */ ConcatFnKind::none,
                   /* returnPtr = */ false);
}

void CStringBoundMisraChecker::evalStrcat(CheckerContext &C,
                                          const CallExpr *CE) const {
  // char *strcat(char *restrict s1, const char *restrict s2);
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ false,
                   /* IsBounded = */ false,
                   /* appendK = */ ConcatFnKind::strcat);
}

void CStringBoundMisraChecker::evalStrncat(CheckerContext &C,
                                           const CallExpr *CE) const {
  // char *strncat(char *restrict s1, const char *restrict s2, size_t n);
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ false,
                   /* IsBounded = */ true,
                   /* appendK = */ ConcatFnKind::strcat);
}

void CStringBoundMisraChecker::evalStrlcat(CheckerContext &C,
                                           const CallExpr *CE) const {
  // size_t strlcat(char *dst, const char *src, size_t size);
  // It will append at most size - strlen(dst) - 1 bytes,
  // NULL-terminating the result.
  evalStrcpyCommon(C, CE,
                   /* ReturnEnd = */ false,
                   /* IsBounded = */ true,
                   /* appendK = */ ConcatFnKind::strlcat,
                   /* returnPtr = */ false);
}

void CStringBoundMisraChecker::evalStrcpyCommon(CheckerContext &C,
                                                const CallExpr *CE,
                                                bool ReturnEnd, bool IsBounded,
                                                ConcatFnKind appendK,
                                                bool returnPtr) const {
  if (appendK == ConcatFnKind::none)
    CurrentFunctionDescription = "string copy function";
  else
    CurrentFunctionDescription = "string concatenation function";

  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the destination is non-null.
  DestinationArgExpr Dst = {CE->getArg(0), 0};
  SVal DstVal = state->getSVal(Dst.Expression, LCtx);
  state = checkNonNull(C, state, Dst, DstVal);
  if (!state)
    return;

  // Check that the source is non-null.
  SourceArgExpr srcExpr = {CE->getArg(1), 1};
  SVal srcVal = state->getSVal(srcExpr.Expression, LCtx);
  state = checkNonNull(C, state, srcExpr, srcVal);
  if (!state)
    return;

  // Get the string length of the source.
  SVal strLength = getCStringLength(C, state, srcExpr.Expression, srcVal);
  Optional<NonLoc> strLengthNL = strLength.getAs<NonLoc>();

  // Get the string length of the destination buffer.
  SVal dstStrLength = getCStringLength(C, state, Dst.Expression, DstVal);
  Optional<NonLoc> dstStrLengthNL = dstStrLength.getAs<NonLoc>();

  // If the source isn't a valid C string, give up.
  if (strLength.isUndef())
    return;

  SValBuilder &svalBuilder = C.getSValBuilder();
  QualType cmpTy = svalBuilder.getConditionType();
  QualType sizeTy = svalBuilder.getContext().getSizeType();

  // These two values allow checking two kinds of errors:
  // - actual overflows caused by a source that doesn't fit in the destination
  // - potential overflows caused by a bound that could exceed the destination
  SVal amountCopied = UnknownVal();
  SVal maxLastElementIndex = UnknownVal();
  const char *boundWarning = nullptr;

  // FIXME: Why do we choose the srcExpr if the access has no size?
  //  Note that the 3rd argument of the call would be the size parameter.

  // If the function is strncpy, strncat, etc... it is bounded.
  if (IsBounded) {
    // Get the max number of characters to copy.
    SizeArgExpr lenExpr = {CE->getArg(2), 2};
    SVal lenVal = state->getSVal(lenExpr.Expression, LCtx);

    // Protect against misdeclared strncpy().
    lenVal =
        svalBuilder.evalCast(lenVal, sizeTy, lenExpr.Expression->getType());

    Optional<NonLoc> lenValNL = lenVal.getAs<NonLoc>();

    // If we know both values, we might be able to figure out how much
    // we're copying.
    if (strLengthNL && lenValNL) {
      switch (appendK) {
      case ConcatFnKind::none:
      case ConcatFnKind::strcat: {
        ProgramStateRef stateSourceTooLong, stateSourceNotTooLong;
        // Check if the max number to copy is less than the length of the src.
        // If the bound is equal to the source length, strncpy won't null-
        // terminate the result!
        std::tie(stateSourceTooLong, stateSourceNotTooLong) = state->assume(
            svalBuilder
                .evalBinOpNN(state, BO_GE, *strLengthNL, *lenValNL, cmpTy)
                .castAs<DefinedOrUnknownSVal>());

        if (stateSourceTooLong && !stateSourceNotTooLong) {
          // Max number to copy is less than the length of the src, so the
          // actual strLength copied is the max number arg.
          state = stateSourceTooLong;
          amountCopied = lenVal;

        } else if (!stateSourceTooLong && stateSourceNotTooLong) {
          // The source buffer entirely fits in the bound.
          state = stateSourceNotTooLong;
          amountCopied = strLength;
        }
        break;
      }
      case ConcatFnKind::strlcat:
        if (!dstStrLengthNL)
          return;

        // amountCopied = min (size - dstLen - 1 , srcLen)
        SVal freeSpace = svalBuilder.evalBinOpNN(state, BO_Sub, *lenValNL,
                                                 *dstStrLengthNL, sizeTy);
        if (!freeSpace.getAs<NonLoc>())
          return;
        freeSpace =
            svalBuilder.evalBinOp(state, BO_Sub, freeSpace,
                                  svalBuilder.makeIntVal(1, sizeTy), sizeTy);
        Optional<NonLoc> freeSpaceNL = freeSpace.getAs<NonLoc>();

        // While unlikely, it is possible that the subtraction is
        // too complex to compute, let's check whether it succeeded.
        if (!freeSpaceNL)
          return;
        SVal hasEnoughSpace = svalBuilder.evalBinOpNN(
            state, BO_LE, *strLengthNL, *freeSpaceNL, cmpTy);

        ProgramStateRef TrueState, FalseState;
        std::tie(TrueState, FalseState) =
            state->assume(hasEnoughSpace.castAs<DefinedOrUnknownSVal>());

        // srcStrLength <= size - dstStrLength -1
        if (TrueState && !FalseState) {
          amountCopied = strLength;
        }

        // srcStrLength > size - dstStrLength -1
        if (!TrueState && FalseState) {
          amountCopied = freeSpace;
        }

        if (TrueState && FalseState)
          amountCopied = UnknownVal();
        break;
      }
    }
    // We still want to know if the bound is known to be too large.
    if (lenValNL) {
      switch (appendK) {
      case ConcatFnKind::strcat:
        // For strncat, the check is strlen(dst) + lenVal < sizeof(dst)

        // Get the string length of the destination. If the destination is
        // memory that can't have a string length, we shouldn't be copying
        // into it anyway.
        if (dstStrLength.isUndef())
          return;

        if (dstStrLengthNL) {
          maxLastElementIndex = svalBuilder.evalBinOpNN(
              state, BO_Add, *lenValNL, *dstStrLengthNL, sizeTy);

          boundWarning = "Size argument is greater than the free space in the "
                         "destination buffer";
        }
        break;
      case ConcatFnKind::none:
      case ConcatFnKind::strlcat:
        // For strncpy and strlcat, this is just checking
        //  that lenVal <= sizeof(dst).
        // (Yes, strncpy and strncat differ in how they treat termination.
        // strncat ALWAYS terminates, but strncpy doesn't.)

        // We need a special case for when the copy size is zero, in which
        // case strncpy will do no work at all. Our bounds check uses n-1
        // as the last element accessed, so n == 0 is problematic.
        ProgramStateRef StateZeroSize, StateNonZeroSize;
        std::tie(StateZeroSize, StateNonZeroSize) =
            assumeZero(C, state, *lenValNL, sizeTy);

        // If the size is known to be zero, we're done.
        if (StateZeroSize && !StateNonZeroSize) {
          if (returnPtr) {
            StateZeroSize = StateZeroSize->BindExpr(CE, LCtx, DstVal);
          } else {
            if (appendK == ConcatFnKind::none) {
              // strlcpy returns strlen(src)
              StateZeroSize = StateZeroSize->BindExpr(CE, LCtx, strLength);
            } else {
              // strlcat returns strlen(src) + strlen(dst)
              SVal retSize = svalBuilder.evalBinOp(state, BO_Add, strLength,
                                                   dstStrLength, sizeTy);
              StateZeroSize = StateZeroSize->BindExpr(CE, LCtx, retSize);
            }
          }
          C.addTransition(StateZeroSize);
          return;
        }

        // Otherwise, go ahead and figure out the last element we'll touch.
        // We don't record the non-zero assumption here because we can't
        // be sure. We won't warn on a possible zero.
        NonLoc one = svalBuilder.makeIntVal(1, sizeTy).castAs<NonLoc>();
        maxLastElementIndex =
            svalBuilder.evalBinOpNN(state, BO_Sub, *lenValNL, one, sizeTy);
        boundWarning = "Size argument is greater than the length of the "
                       "destination buffer";
        break;
      }
    }
  } else {
    // The function isn't bounded. The amount copied should match the length
    // of the source buffer.
    amountCopied = strLength;
  }

  assert(state);

  // This represents the number of characters copied into the destination
  // buffer. (It may not actually be the strlen if the destination buffer
  // is not terminated.)
  SVal finalStrLength = UnknownVal();
  SVal strlRetVal = UnknownVal();

  if (appendK == ConcatFnKind::none && !returnPtr) {
    // strlcpy returns the sizeof(src)
    strlRetVal = strLength;
  }

  // If this is an appending function (strcat, strncat...) then set the
  // string length to strlen(src) + strlen(dst) since the buffer will
  // ultimately contain both.
  if (appendK != ConcatFnKind::none) {
    // Get the string length of the destination. If the destination is memory
    // that can't have a string length, we shouldn't be copying into it anyway.
    if (dstStrLength.isUndef())
      return;

    if (appendK == ConcatFnKind::strlcat && dstStrLengthNL && strLengthNL) {
      strlRetVal = svalBuilder.evalBinOpNN(state, BO_Add, *strLengthNL,
                                           *dstStrLengthNL, sizeTy);
    }

    Optional<NonLoc> amountCopiedNL = amountCopied.getAs<NonLoc>();

    // If we know both string lengths, we might know the final string length.
    if (amountCopiedNL && dstStrLengthNL) {
      // Make sure the two lengths together don't overflow a size_t.

      finalStrLength = svalBuilder.evalBinOpNN(state, BO_Add, *amountCopiedNL,
                                               *dstStrLengthNL, sizeTy);
    }

    // If we couldn't get a single value for the final string length,
    // we can at least bound it by the individual lengths.
    if (finalStrLength.isUnknown()) {
      // Try to get a "hypothetical" string length symbol, which we can later
      // set as a real value if that turns out to be the case.
      finalStrLength = getCStringLength(C, state, CE, DstVal, true);
      assert(!finalStrLength.isUndef());

      if (Optional<NonLoc> finalStrLengthNL = finalStrLength.getAs<NonLoc>()) {
        if (amountCopiedNL && appendK == ConcatFnKind::none) {
          // we overwrite dst string with the src
          // finalStrLength >= srcStrLength
          SVal sourceInResult = svalBuilder.evalBinOpNN(
              state, BO_GE, *finalStrLengthNL, *amountCopiedNL, cmpTy);
          state = state->assume(sourceInResult.castAs<DefinedOrUnknownSVal>(),
                                true);
          if (!state)
            return;
        }

        if (dstStrLengthNL && appendK != ConcatFnKind::none) {
          // we extend the dst string with the src
          // finalStrLength >= dstStrLength
          SVal destInResult = svalBuilder.evalBinOpNN(
              state, BO_GE, *finalStrLengthNL, *dstStrLengthNL, cmpTy);
          state =
              state->assume(destInResult.castAs<DefinedOrUnknownSVal>(), true);
          if (!state)
            return;
        }
      }
    }

  } else {
    // Otherwise, this is a copy-over function (strcpy, strncpy, ...), and
    // the final string length will match the input string length.
    finalStrLength = amountCopied;
  }

  SVal Result;

  if (returnPtr) {
    // The final result of the function will either be a pointer past the last
    // copied element, or a pointer to the start of the destination buffer.
    Result = (ReturnEnd ? UnknownVal() : DstVal);
  } else {
    if (appendK == ConcatFnKind::strlcat || appendK == ConcatFnKind::none)
      // strlcpy, strlcat
      Result = strlRetVal;
    else
      Result = finalStrLength;
  }

  assert(state);

  // If the destination is a MemRegion, try to check for a buffer overflow and
  // record the new string length.
  if (Optional<loc::MemRegionVal> dstRegVal =
          DstVal.getAs<loc::MemRegionVal>()) {
    QualType ptrTy = Dst.Expression->getType();

    // If we have an exact value on a bounded copy, use that to check for
    // overflows, rather than our estimate about how much is actually copied.
    if (Optional<NonLoc> maxLastNL = maxLastElementIndex.getAs<NonLoc>()) {
      SVal maxLastElement =
          svalBuilder.evalBinOpLN(state, BO_Add, *dstRegVal, *maxLastNL, ptrTy);

      state = CheckLocation(C, state, Dst, maxLastElement, AccessKind::write);
      if (!state)
        return;
    }

    // Then, if the final length is known...
    if (Optional<NonLoc> knownStrLength = finalStrLength.getAs<NonLoc>()) {
      SVal lastElement = svalBuilder.evalBinOpLN(state, BO_Add, *dstRegVal,
                                                 *knownStrLength, ptrTy);

      // ...and we haven't checked the bound, we'll check the actual copy.
      if (!boundWarning) {
        state = CheckLocation(C, state, Dst, lastElement, AccessKind::write);
        if (!state)
          return;
      }

      // If this is a stpcpy-style copy, the last element is the return value.
      if (returnPtr && ReturnEnd)
        Result = lastElement;
    }

    // Invalidate the destination (regular invalidation without pointer-escaping
    // the address of the top-level region). This must happen before we set the
    // C string length because invalidation will clear the length.
    // FIXME: Even if we can't perfectly model the copy, we should see if we
    // can use LazyCompoundVals to copy the source values into the destination.
    // This would probably remove any existing bindings past the end of the
    // string, but that's still an improvement over blank invalidation.
    state = InvalidateBuffer(C, state, Dst.Expression, *dstRegVal,
                             /*IsSourceBuffer*/ false, nullptr);

    // Invalidate the source (const-invalidation without const-pointer-escaping
    // the address of the top-level region).
    state = InvalidateBuffer(C, state, srcExpr.Expression, srcVal,
                             /*IsSourceBuffer*/ true, nullptr);

    // Set the C string length of the destination, if we know it.
    if (IsBounded && (appendK == ConcatFnKind::none)) {
      // strncpy is annoying in that it doesn't guarantee to null-terminate
      // the result string. If the original string didn't fit entirely inside
      // the bound (including the null-terminator), we don't know how long the
      // result is.
      if (amountCopied != strLength)
        finalStrLength = UnknownVal();
    }
    state = setCStringLength(state, dstRegVal->getRegion(), finalStrLength);
  }

  assert(state);

  if (returnPtr) {
    // If this is a stpcpy-style copy, but we were unable to check for a buffer
    // overflow, we still need a result. Conjure a return value.
    if (ReturnEnd && Result.isUnknown()) {
      Result = svalBuilder.conjureSymbolVal(nullptr, CE, LCtx, C.blockCount());
    }
  }
  // Set the return value.
  state = state->BindExpr(CE, LCtx, Result);
  C.addTransition(state);
}

void CStringBoundMisraChecker::evalStrcmp(CheckerContext &C,
                                          const CallExpr *CE) const {
  // int strcmp(const char *s1, const char *s2);
  evalStrcmpCommon(C, CE, /* IsBounded = */ false, /* IgnoreCase = */ false);
}

void CStringBoundMisraChecker::evalStrncmp(CheckerContext &C,
                                           const CallExpr *CE) const {
  // int strncmp(const char *s1, const char *s2, size_t n);
  evalStrcmpCommon(C, CE, /* IsBounded = */ true, /* IgnoreCase = */ false);
}

void CStringBoundMisraChecker::evalStrcasecmp(CheckerContext &C,
                                              const CallExpr *CE) const {
  // int strcasecmp(const char *s1, const char *s2);
  evalStrcmpCommon(C, CE, /* IsBounded = */ false, /* IgnoreCase = */ true);
}

void CStringBoundMisraChecker::evalStrncasecmp(CheckerContext &C,
                                               const CallExpr *CE) const {
  // int strncasecmp(const char *s1, const char *s2, size_t n);
  evalStrcmpCommon(C, CE, /* IsBounded = */ true, /* IgnoreCase = */ true);
}

void CStringBoundMisraChecker::evalStrcmpCommon(CheckerContext &C,
                                                const CallExpr *CE,
                                                bool IsBounded,
                                                bool IgnoreCase) const {
  CurrentFunctionDescription = "string comparison function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the first string is non-null
  AnyArgExpr Left = {CE->getArg(0), 0};
  SVal LeftVal = state->getSVal(Left.Expression, LCtx);
  state = checkNonNull(C, state, Left, LeftVal);
  if (!state)
    return;

  // Check that the second string is non-null.
  AnyArgExpr Right = {CE->getArg(1), 1};
  SVal RightVal = state->getSVal(Right.Expression, LCtx);
  state = checkNonNull(C, state, Right, RightVal);
  if (!state)
    return;

  // Get the string length of the first string or give up.
  SVal LeftLength = getCStringLength(C, state, Left.Expression, LeftVal);
  if (LeftLength.isUndef())
    return;

  // Get the string length of the second string or give up.
  SVal RightLength = getCStringLength(C, state, Right.Expression, RightVal);
  if (RightLength.isUndef())
    return;

  // If we know the two buffers are the same, we know the result is 0.
  // First, get the two buffers' addresses. Another checker will have already
  // made sure they're not undefined.
  DefinedOrUnknownSVal LV = LeftVal.castAs<DefinedOrUnknownSVal>();
  DefinedOrUnknownSVal RV = RightVal.castAs<DefinedOrUnknownSVal>();

  // See if they are the same.
  SValBuilder &svalBuilder = C.getSValBuilder();
  DefinedOrUnknownSVal SameBuf = svalBuilder.evalEQ(state, LV, RV);
  ProgramStateRef StSameBuf, StNotSameBuf;
  std::tie(StSameBuf, StNotSameBuf) = state->assume(SameBuf);

  // If the two arguments might be the same buffer, we know the result is 0,
  // and we only need to check one size.
  if (StSameBuf) {
    StSameBuf =
        StSameBuf->BindExpr(CE, LCtx, svalBuilder.makeZeroVal(CE->getType()));
    C.addTransition(StSameBuf);

    // If the two arguments are GUARANTEED to be the same, we're done!
    if (!StNotSameBuf)
      return;
  }

  assert(StNotSameBuf);
  state = StNotSameBuf;

  // At this point we can go about comparing the two buffers.
  // For now, we only do this if they're both known string literals.

  // Attempt to extract string literals from both expressions.
  const StringLiteral *LeftStrLiteral =
      getCStringLiteral(C, state, Left.Expression, LeftVal);
  const StringLiteral *RightStrLiteral =
      getCStringLiteral(C, state, Right.Expression, RightVal);
  bool canComputeResult = false;
  SVal resultVal =
      svalBuilder.conjureSymbolVal(nullptr, CE, LCtx, C.blockCount());

  if (LeftStrLiteral && RightStrLiteral) {
    StringRef LeftStrRef = LeftStrLiteral->getString();
    StringRef RightStrRef = RightStrLiteral->getString();

    if (IsBounded) {
      // Get the max number of characters to compare.
      const Expr *lenExpr = CE->getArg(2);
      SVal lenVal = state->getSVal(lenExpr, LCtx);

      // If the length is known, we can get the right substrings.
      if (const llvm::APSInt *len = svalBuilder.getKnownValue(state, lenVal)) {
        // Create substrings of each to compare the prefix.
        LeftStrRef = LeftStrRef.substr(0, (size_t)len->getZExtValue());
        RightStrRef = RightStrRef.substr(0, (size_t)len->getZExtValue());
        canComputeResult = true;
      }
    } else {
      // This is a normal, unbounded strcmp.
      canComputeResult = true;
    }

    if (canComputeResult) {
      // Real strcmp stops at null characters.
      size_t s1Term = LeftStrRef.find('\0');
      if (s1Term != StringRef::npos)
        LeftStrRef = LeftStrRef.substr(0, s1Term);

      size_t s2Term = RightStrRef.find('\0');
      if (s2Term != StringRef::npos)
        RightStrRef = RightStrRef.substr(0, s2Term);

      // Use StringRef's comparison methods to compute the actual result.
      int compareRes = IgnoreCase ? LeftStrRef.compare_insensitive(RightStrRef)
                                  : LeftStrRef.compare(RightStrRef);

      // The strcmp function returns an integer greater than, equal to, or less
      // than zero, [c11, p7.24.4.2].
      if (compareRes == 0) {
        resultVal = svalBuilder.makeIntVal(compareRes, CE->getType());
      } else {
        DefinedSVal zeroVal = svalBuilder.makeIntVal(0, CE->getType());
        // Constrain strcmp's result range based on the result of StringRef's
        // comparison methods.
        BinaryOperatorKind op = (compareRes == 1) ? BO_GT : BO_LT;
        SVal compareWithZero = svalBuilder.evalBinOp(
            state, op, resultVal, zeroVal, svalBuilder.getConditionType());
        DefinedSVal compareWithZeroVal = compareWithZero.castAs<DefinedSVal>();
        state = state->assume(compareWithZeroVal, true);
      }
    }
  }

  state = state->BindExpr(CE, LCtx, resultVal);

  // Record this as a possible path.
  C.addTransition(state);
}

void CStringBoundMisraChecker::evalStrchr(CheckerContext &C,
                                          const CallExpr *CE) const {
  CurrentFunctionDescription = "strchr or strrchr function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the first string is non-null
  AnyArgExpr Left = {CE->getArg(0), 0};
  SVal LeftVal = state->getSVal(Left.Expression, LCtx);
  state = checkNonNull(C, state, Left, LeftVal);
  if (!state)
    return;

  // Check the string length of the first string or give up.
  getCStringLength(C, state, Left.Expression, LeftVal);
  // TODO: modeling the function behavior
}

void CStringBoundMisraChecker::evalStrspn(CheckerContext &C,
                                          const CallExpr *CE) const {
  CurrentFunctionDescription = "strspn or strcspn function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the first string is non-null
  AnyArgExpr Left = {CE->getArg(0), 0};
  SVal LeftVal = state->getSVal(Left.Expression, LCtx);
  state = checkNonNull(C, state, Left, LeftVal);
  if (!state)
    return;

  // Check that the second string is non-null.
  AnyArgExpr Right = {CE->getArg(1), 1};
  SVal RightVal = state->getSVal(Right.Expression, LCtx);
  state = checkNonNull(C, state, Right, RightVal);
  if (!state)
    return;

  // Get the string length of the first string or give up.
  SVal LeftLength = getCStringLength(C, state, Left.Expression, LeftVal);
  if (LeftLength.isUndef())
    return;

  // Get the string length of the second string or give up.
  getCStringLength(C, state, Right.Expression, RightVal);
  // TODO: modeling the function behavior
}

void CStringBoundMisraChecker::evalStrstr(CheckerContext &C,
                                          const CallExpr *CE) const {
  CurrentFunctionDescription = "strstr() function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the first string is non-null
  AnyArgExpr Left = {CE->getArg(0), 0};
  SVal LeftVal = state->getSVal(Left.Expression, LCtx);
  state = checkNonNull(C, state, Left, LeftVal);
  if (!state)
    return;

  // Check that the second string is non-null.
  AnyArgExpr Right = {CE->getArg(1), 1};
  SVal RightVal = state->getSVal(Right.Expression, LCtx);
  state = checkNonNull(C, state, Right, RightVal);
  if (!state)
    return;

  // Get the string length of the first string or give up.
  SVal LeftLength = getCStringLength(C, state, Left.Expression, LeftVal);
  if (LeftLength.isUndef())
    return;

  // Get the string length of the second string or give up.
  getCStringLength(C, state, Right.Expression, RightVal);
  // TODO: modeling the function behavior
}

void CStringBoundMisraChecker::evalStrpbrk(CheckerContext &C,
                                           const CallExpr *CE) const {
  CurrentFunctionDescription = "strpbrk() function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the first string is non-null
  AnyArgExpr Left = {CE->getArg(0), 0};
  SVal LeftVal = state->getSVal(Left.Expression, LCtx);
  state = checkNonNull(C, state, Left, LeftVal);
  if (!state)
    return;

  // Check that the second string is non-null.
  AnyArgExpr Right = {CE->getArg(1), 1};
  SVal RightVal = state->getSVal(Right.Expression, LCtx);
  state = checkNonNull(C, state, Right, RightVal);
  if (!state)
    return;

  // Get the string length of the first string or give up.
  SVal LeftLength = getCStringLength(C, state, Left.Expression, LeftVal);
  if (LeftLength.isUndef())
    return;

  // Get the string length of the second string or give up.
  getCStringLength(C, state, Right.Expression, RightVal);
  // TODO: modeling the function behavior
}

void CStringBoundMisraChecker::evalStrtok(CheckerContext &C,
                                          const CallExpr *CE) const {
  CurrentFunctionDescription = "strtok() function";
  ProgramStateRef state = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // the first string can be null
  AnyArgExpr Left = {CE->getArg(0), 0};
  SVal LeftVal = state->getSVal(Left.Expression, LCtx);

  // Check that the second string is non-null.
  AnyArgExpr Right = {CE->getArg(1), 1};
  SVal RightVal = state->getSVal(Right.Expression, LCtx);
  state = checkNonNull(C, state, Right, RightVal);
  if (!state)
    return;

  // Get the string length of the first string or give up.
  SVal LeftLength = getCStringLength(C, state, Left.Expression, LeftVal);
  if (LeftLength.isUndef())
    return;

  // Get the string length of the second string or give up.
  getCStringLength(C, state, Right.Expression, RightVal);
  // TODO: modeling the function behavior
}

void CStringBoundMisraChecker::evalStrsep(CheckerContext &C,
                                          const CallExpr *CE) const {
  // char *strsep(char **stringp, const char *delim);
  //  Sanity: does the search string parameter match the return type?
  SourceArgExpr SearchStrPtr = {CE->getArg(0), 0};

  QualType CharPtrTy = SearchStrPtr.Expression->getType()->getPointeeType();
  if (CharPtrTy.isNull() ||
      CE->getType().getUnqualifiedType() != CharPtrTy.getUnqualifiedType())
    return;

  CurrentFunctionDescription = "strsep()";
  ProgramStateRef State = C.getState();
  const LocationContext *LCtx = C.getLocationContext();

  // Check that the search string pointer is non-null (though it may point to
  // a null string).
  SVal SearchStrVal = State->getSVal(SearchStrPtr.Expression, LCtx);
  State = checkNonNull(C, State, SearchStrPtr, SearchStrVal);
  if (!State)
    return;

  // Check that the delimiter string is non-null.
  AnyArgExpr DelimStr = {CE->getArg(1), 1};
  SVal DelimStrVal = State->getSVal(DelimStr.Expression, LCtx);
  State = checkNonNull(C, State, DelimStr, DelimStrVal);
  if (!State)
    return;

  SValBuilder &SVB = C.getSValBuilder();
  SVal Result;
  if (Optional<Loc> SearchStrLoc = SearchStrVal.getAs<Loc>()) {
    // Get the current value of the search string pointer, as a char*.
    Result = State->getSVal(*SearchStrLoc, CharPtrTy);

    // Invalidate the search string, representing the change of one delimiter
    // character to NUL.
    State = InvalidateBuffer(C, State, SearchStrPtr.Expression, Result,
                             /*IsSourceBuffer*/ false, nullptr);

    // Overwrite the search string pointer. The new value is either an address
    // further along in the same string, or NULL if there are no more tokens.
    State = State->bindLoc(
        *SearchStrLoc,
        SVB.conjureSymbolVal(getTag(), CE, LCtx, CharPtrTy, C.blockCount()),
        LCtx);
  } else {
    assert(SearchStrVal.isUnknown());
    // Conjure a symbolic value. It's the best we can do.
    Result = SVB.conjureSymbolVal(nullptr, CE, LCtx, C.blockCount());
  }

  // Set the return value, and finish.
  State = State->BindExpr(CE, LCtx, Result);
  C.addTransition(State);
}

//===----------------------------------------------------------------------===//
// The driver method, and other Checker callbacks.
//===----------------------------------------------------------------------===//

CStringBoundMisraChecker::FnCheck
CStringBoundMisraChecker::identifyCall(const CallEvent &Call,
                                       CheckerContext &C) const {
  const auto *CE = dyn_cast_or_null<CallExpr>(Call.getOriginExpr());
  if (!CE)
    return nullptr;

  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());
  if (!FD)
    return nullptr;

  // Pro-actively check that argument types are safe to do arithmetic upon.
  // We do not want to crash if someone accidentally passes a structure
  // into, say, a C++ overload of any of these functions. We could not check
  // that for std::copy because they may have arguments of other types.
  for (auto I : CE->arguments()) {
    QualType T = I->getType();
    if (!T->isIntegralOrEnumerationType() && !T->isPointerType())
      return nullptr;
  }

  const FnCheck *Callback = Callbacks.lookup(Call);
  if (Callback)
    return *Callback;

  return nullptr;
}

bool CStringBoundMisraChecker::evalCall(const CallEvent &Call,
                                        CheckerContext &C) const {
  FnCheck Callback = identifyCall(Call, C);

  // If the callee isn't a string function, let another checker handle it.
  if (!Callback)
    return false;

  // Check and evaluate the call.
  const auto *CE = cast<CallExpr>(Call.getOriginExpr());
  (this->*Callback)(C, CE);

  // If the evaluate call resulted in no change, chain to the next eval call
  // handler.
  // Note, the custom CString evaluation calls assume that basic safety
  // properties are held. However, if the user chooses to turn off some of these
  // checks, we ignore the issues and leave the call evaluation to a generic
  // handler.
  return C.isDifferent();
}

void CStringBoundMisraChecker::checkPreStmt(const DeclStmt *DS,
                                            CheckerContext &C) const {
  // Record string length for char a[] = "abc";
  ProgramStateRef state = C.getState();

  for (const auto *I : DS->decls()) {
    const VarDecl *D = dyn_cast<VarDecl>(I);
    if (!D)
      continue;

    // FIXME: Handle array fields of structs.
    if (!D->getType()->isArrayType())
      continue;

    const Expr *Init = D->getInit();
    if (!Init)
      continue;
    if (!isa<StringLiteral>(Init))
      continue;

    Loc VarLoc = state->getLValue(D, C.getLocationContext());
    const MemRegion *MR = VarLoc.getAsRegion();
    if (!MR)
      continue;

    SVal StrVal = C.getSVal(Init);
    assert(StrVal.isValid() && "Initializer string is unknown or undefined");
    DefinedOrUnknownSVal strLength =
        getCStringLength(C, state, Init, StrVal).castAs<DefinedOrUnknownSVal>();

    state = state->set<CStringLength>(MR, strLength);
  }

  C.addTransition(state);
}

ProgramStateRef CStringBoundMisraChecker::checkRegionChanges(
    ProgramStateRef state, const InvalidatedSymbols *,
    ArrayRef<const MemRegion *> ExplicitRegions,
    ArrayRef<const MemRegion *> Regions, const LocationContext *LCtx,
    const CallEvent *Call) const {
  CStringLengthTy Entries = state->get<CStringLength>();
  if (Entries.isEmpty())
    return state;

  llvm::SmallPtrSet<const MemRegion *, 8> Invalidated;
  llvm::SmallPtrSet<const MemRegion *, 32> SuperRegions;

  // First build sets for the changed regions and their super-regions.
  for (ArrayRef<const MemRegion *>::iterator I = Regions.begin(),
                                             E = Regions.end();
       I != E; ++I) {
    const MemRegion *MR = *I;
    Invalidated.insert(MR);

    SuperRegions.insert(MR);
    while (const SubRegion *SR = dyn_cast<SubRegion>(MR)) {
      MR = SR->getSuperRegion();
      SuperRegions.insert(MR);
    }
  }

  CStringLengthTy::Factory &F = state->get_context<CStringLength>();

  // Then loop over the entries in the current state.
  for (CStringLengthTy::iterator I = Entries.begin(), E = Entries.end(); I != E;
       ++I) {
    const MemRegion *MR = I.getKey();

    // Is this entry for a super-region of a changed region?
    if (SuperRegions.count(MR)) {
      Entries = F.remove(Entries, MR);
      continue;
    }

    // Is this entry for a sub-region of a changed region?
    const MemRegion *Super = MR;
    while (const SubRegion *SR = dyn_cast<SubRegion>(Super)) {
      Super = SR->getSuperRegion();
      if (Invalidated.count(Super)) {
        Entries = F.remove(Entries, MR);
        break;
      }
    }
  }

  return state->set<CStringLength>(Entries);
}

void CStringBoundMisraChecker::checkLiveSymbols(ProgramStateRef state,
                                                SymbolReaper &SR) const {
  // Mark all symbols in our string length map as valid.
  CStringLengthTy Entries = state->get<CStringLength>();

  for (CStringLengthTy::iterator I = Entries.begin(), E = Entries.end(); I != E;
       ++I) {
    SVal Len = I.getData();

    for (SymExpr::symbol_iterator si = Len.symbol_begin(),
                                  se = Len.symbol_end();
         si != se; ++si)
      SR.markInUse(*si);
  }
}

void CStringBoundMisraChecker::checkDeadSymbols(SymbolReaper &SR,
                                                CheckerContext &C) const {
  ProgramStateRef state = C.getState();
  CStringLengthTy Entries = state->get<CStringLength>();
  if (Entries.isEmpty())
    return;

  CStringLengthTy::Factory &F = state->get_context<CStringLength>();
  for (CStringLengthTy::iterator I = Entries.begin(), E = Entries.end(); I != E;
       ++I) {
    SVal Len = I.getData();
    if (SymbolRef Sym = Len.getAsSymbol()) {
      if (SR.isDead(Sym))
        Entries = F.remove(Entries, I.getKey());
    }
  }

  state = state->set<CStringLength>(Entries);
  C.addTransition(state);
}

void ento::registerCStringBoundMisraChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<CStringBoundMisraChecker>();
}

bool ento::shouldRegisterCStringBoundMisraChecker(const CheckerManager &mgr) {
  return true;
}
