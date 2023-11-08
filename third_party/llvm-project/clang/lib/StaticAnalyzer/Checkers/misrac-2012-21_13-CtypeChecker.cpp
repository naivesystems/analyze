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

//=== misrac-2012-21_13-CtypeChecker.cpp - Check standard ctype functions ---*- C++ -*-===//
//
// This checker is modified on top of the original clang static checker
// StdCLibraryFunctionsChecker.cpp.
//
// The original checker not only checks the validity of arguments, it also
// models functions behaviors because the source code of library functions may
// not be available for analysis. It is achieved by checking CaseConstraints in
// Summary and inserting proper states, which is implemented in function
// checkPostCall and evalCall.
//
// Since for rule 21.13 we only need to check the arguments in checkPreCall,
// CaseConstraints, checkPostCall and evalCall are removed from the checker.
//
// Detailed modifications are:
// 1. Removed the function modelling part of the original checker, including
// CaseConstraints and Invalidation, which results in:
//  - Callbacks to evalCall and postCall are removed (modelling is not needed).
//  - Constructor of Summary now is set to default (due to the removal of
//    Invalidation).
//  - Summary now only hold the ArgConstraints, and the checkValidity will be
//    only carried on argumments (Original checker also checks CaseConstraints).
// 2. Removed functions that are not defined in header ctype.h from Summary map.
// 3. Removed all CaseConstraints when adding functions to Summary map.
// 4. Changed the name of the checker to "CtypeFunctionArgs"
// 5. Added new package "misra_c_2012" for holding this checker.
//
// The following standard C functions are currently supported:
//
//   isdigit   isupper  toascii
//   isalnum   isgraph  isxdigit
//   isalpha   islower  toupper
//   isascii   isprint  tolower
//   isblank   ispunct
//   iscntrl   isspace
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
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"

#include <string>

using namespace clang;
using namespace clang::ento;

namespace {
class CtypeFunctionsChecker : public Checker<check::PreCall> {

  class Summary;

  // The universal integral type to use in value range descriptions.
  // Unsigned to make sure overflows are well-defined.
  typedef uint64_t RangeInt;

  /// Normally, describes a single range constraint, eg. {{0, 1}, {3, 4}} is
  /// a non-negative integer, which less than 5 and not equal to 2. For
  /// `ComparesToArgument', holds information about how exactly to compare to
  /// the argument.
  typedef std::vector<std::pair<RangeInt, RangeInt>> IntRangeVector;

  /// A reference to an argument or return value by its number.
  /// ArgNo in CallExpr and CallEvent is defined as Unsigned, but
  /// obviously uint32_t should be enough for all practical purposes.
  typedef uint32_t ArgNo;
  static const ArgNo Ret;

  /// Returns the string representation of an argument index.
  /// E.g.: (1) -> '1st arg', (2) - > '2nd arg'
  static SmallString<8> getArgDesc(ArgNo);

  class ValueConstraint;

  // Pointer to the ValueConstraint. We need a copyable, polymorphic and
  // default initialize able type (vector needs that). A raw pointer was good,
  // however, we cannot default initialize that. unique_ptr makes the Summary
  // class non-copyable, therefore not an option. Releasing the copyability
  // requirement would render the initialization of the Summary map infeasible.
  using ValueConstraintPtr = std::shared_ptr<ValueConstraint>;

  /// Polymorphic base class that represents a constraint on a given argument
  /// (or return value) of a function. Derived classes implement different kind
  /// of constraints, e.g range constraints or correlation between two
  /// arguments.
  class ValueConstraint {
  public:
    ValueConstraint(ArgNo ArgN) : ArgN(ArgN) {}
    virtual ~ValueConstraint() {}
    /// Apply the effects of the constraint on the given program state. If null
    /// is returned then the constraint is not feasible.
    virtual ProgramStateRef apply(ProgramStateRef State, const CallEvent &Call,
                                  const Summary &Summary,
                                  CheckerContext &C) const = 0;
    virtual ValueConstraintPtr negate() const {
      llvm_unreachable("Not implemented");
    };

    // Check whether the constraint is malformed or not. It is malformed if the
    // specified argument has a mismatch with the given FunctionDecl (e.g. the
    // arg number is out-of-range of the function's argument list).
    bool checkValidity(const FunctionDecl *FD) const {
      const bool ValidArg = ArgN == Ret || ArgN < FD->getNumParams();
      assert(ValidArg && "Arg out of range!");
      if (!ValidArg)
        return false;
      // Subclasses may further refine the validation.
      return checkSpecificValidity(FD);
    }
    ArgNo getArgNo() const { return ArgN; }

    // Return those arguments that should be tracked when we report a bug. By
    // default it is the argument that is constrained, however, in some special
    // cases we need to track other arguments as well. E.g. a buffer size might
    // be encoded in another argument.
    virtual std::vector<ArgNo> getArgsToTrack() const { return {ArgN}; }

    virtual StringRef getName() const = 0;

    // Give a description that explains the constraint to the user. Used when
    // the bug is reported.
    virtual std::string describe(ProgramStateRef State,
                                 const Summary &Summary) const {
      // There are some descendant classes that are not used as argument
      // constraints, e.g. ComparisonConstraint. In that case we can safely
      // ignore the implementation of this function.
      llvm_unreachable("Not implemented");
    }

  protected:
    ArgNo ArgN; // Argument to which we apply the constraint.

    /// Do polymorphic sanity check on the constraint.
    virtual bool checkSpecificValidity(const FunctionDecl *FD) const {
      return true;
    }
  };

  /// Given a range, should the argument stay inside or outside this range?
  enum RangeKind { OutOfRange, WithinRange };

  /// Encapsulates a range on a single symbol.
  class RangeConstraint : public ValueConstraint {
    RangeKind Kind;
    // A range is formed as a set of intervals (sub-ranges).
    // E.g. {['A', 'Z'], ['a', 'z']}
    //
    // The default constructed RangeConstraint has an empty range set, applying
    // such constraint does not involve any assumptions, thus the State remains
    // unchanged. This is meaningful, if the range is dependent on a looked up
    // type (e.g. [0, Socklen_tMax]). If the type is not found, then the range
    // is default initialized to be empty.
    IntRangeVector Ranges;

  public:
    StringRef getName() const override { return "Range"; }
    RangeConstraint(ArgNo ArgN, RangeKind Kind, const IntRangeVector &Ranges)
        : ValueConstraint(ArgN), Kind(Kind), Ranges(Ranges) {}

    std::string describe(ProgramStateRef State,
                         const Summary &Summary) const override;

    const IntRangeVector &getRanges() const { return Ranges; }

  private:
    ProgramStateRef applyAsOutOfRange(ProgramStateRef State,
                                      const CallEvent &Call,
                                      const Summary &Summary) const;
    ProgramStateRef applyAsWithinRange(ProgramStateRef State,
                                       const CallEvent &Call,
                                       const Summary &Summary) const;

  public:
    ProgramStateRef apply(ProgramStateRef State, const CallEvent &Call,
                          const Summary &Summary,
                          CheckerContext &C) const override {
      switch (Kind) {
      case OutOfRange:
        return applyAsOutOfRange(State, Call, Summary);
      case WithinRange:
        return applyAsWithinRange(State, Call, Summary);
      }
      llvm_unreachable("Unknown range kind!");
    }

    ValueConstraintPtr negate() const override {
      RangeConstraint Tmp(*this);
      switch (Kind) {
      case OutOfRange:
        Tmp.Kind = WithinRange;
        break;
      case WithinRange:
        Tmp.Kind = OutOfRange;
        break;
      }
      return std::make_shared<RangeConstraint>(Tmp);
    }

    bool checkSpecificValidity(const FunctionDecl *FD) const override {
      const bool ValidArg =
          getArgType(FD, ArgN)->isIntegralType(FD->getASTContext());
      assert(ValidArg &&
             "This constraint should be applied on an integral type");
      return ValidArg;
    }
  };

  class ComparisonConstraint : public ValueConstraint {
    BinaryOperator::Opcode Opcode;
    ArgNo OtherArgN;

  public:
    virtual StringRef getName() const override { return "Comparison"; };
    ComparisonConstraint(ArgNo ArgN, BinaryOperator::Opcode Opcode,
                         ArgNo OtherArgN)
        : ValueConstraint(ArgN), Opcode(Opcode), OtherArgN(OtherArgN) {}
    ArgNo getOtherArgNo() const { return OtherArgN; }
    BinaryOperator::Opcode getOpcode() const { return Opcode; }
    ProgramStateRef apply(ProgramStateRef State, const CallEvent &Call,
                          const Summary &Summary,
                          CheckerContext &C) const override;
  };

  class NotNullConstraint : public ValueConstraint {
    using ValueConstraint::ValueConstraint;
    // This variable has a role when we negate the constraint.
    bool CannotBeNull = true;

  public:
    std::string describe(ProgramStateRef State,
                         const Summary &Summary) const override;
    StringRef getName() const override { return "NonNull"; }
    ProgramStateRef apply(ProgramStateRef State, const CallEvent &Call,
                          const Summary &Summary,
                          CheckerContext &C) const override {
      SVal V = getArgSVal(Call, getArgNo());
      if (V.isUndef())
        return State;

      DefinedOrUnknownSVal L = V.castAs<DefinedOrUnknownSVal>();
      if (!L.getAs<Loc>())
        return State;

      return State->assume(L, CannotBeNull);
    }

    ValueConstraintPtr negate() const override {
      NotNullConstraint Tmp(*this);
      Tmp.CannotBeNull = !this->CannotBeNull;
      return std::make_shared<NotNullConstraint>(Tmp);
    }

    bool checkSpecificValidity(const FunctionDecl *FD) const override {
      const bool ValidArg = getArgType(FD, ArgN)->isPointerType();
      assert(ValidArg &&
             "This constraint should be applied only on a pointer type");
      return ValidArg;
    }
  };

  // Represents a buffer argument with an additional size constraint. The
  // constraint may be a concrete value, or a symbolic value in an argument.
  // Example 1. Concrete value as the minimum buffer size.
  //   char *asctime_r(const struct tm *restrict tm, char *restrict buf);
  //   // `buf` size must be at least 26 bytes according the POSIX standard.
  // Example 2. Argument as a buffer size.
  //   ctime_s(char *buffer, rsize_t bufsz, const time_t *time);
  // Example 3. The size is computed as a multiplication of other args.
  //   size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
  //   // Here, ptr is the buffer, and its minimum size is `size * nmemb`.
  class BufferSizeConstraint : public ValueConstraint {
    // The concrete value which is the minimum size for the buffer.
    llvm::Optional<llvm::APSInt> ConcreteSize;
    // The argument which holds the size of the buffer.
    llvm::Optional<ArgNo> SizeArgN;
    // The argument which is a multiplier to size. This is set in case of
    // `fread` like functions where the size is computed as a multiplication of
    // two arguments.
    llvm::Optional<ArgNo> SizeMultiplierArgN;
    // The operator we use in apply. This is negated in negate().
    BinaryOperator::Opcode Op = BO_LE;

  public:
    StringRef getName() const override { return "BufferSize"; }
    BufferSizeConstraint(ArgNo Buffer, llvm::APSInt BufMinSize)
        : ValueConstraint(Buffer), ConcreteSize(BufMinSize) {}
    BufferSizeConstraint(ArgNo Buffer, ArgNo BufSize)
        : ValueConstraint(Buffer), SizeArgN(BufSize) {}
    BufferSizeConstraint(ArgNo Buffer, ArgNo BufSize, ArgNo BufSizeMultiplier)
        : ValueConstraint(Buffer), SizeArgN(BufSize),
          SizeMultiplierArgN(BufSizeMultiplier) {}

    std::vector<ArgNo> getArgsToTrack() const override {
      std::vector<ArgNo> Result{ArgN};
      if (SizeArgN)
        Result.push_back(*SizeArgN);
      if (SizeMultiplierArgN)
        Result.push_back(*SizeMultiplierArgN);
      return Result;
    }

    std::string describe(ProgramStateRef State,
                         const Summary &Summary) const override;

    ProgramStateRef apply(ProgramStateRef State, const CallEvent &Call,
                          const Summary &Summary,
                          CheckerContext &C) const override {
      SValBuilder &SvalBuilder = C.getSValBuilder();
      // The buffer argument.
      SVal BufV = getArgSVal(Call, getArgNo());

      // Get the size constraint.
      const SVal SizeV = [this, &State, &Call, &Summary, &SvalBuilder]() {
        if (ConcreteSize) {
          return SVal(SvalBuilder.makeIntVal(*ConcreteSize));
        }
        assert(SizeArgN && "The constraint must be either a concrete value or "
                           "encoded in an argument.");
        // The size argument.
        SVal SizeV = getArgSVal(Call, *SizeArgN);
        // Multiply with another argument if given.
        if (SizeMultiplierArgN) {
          SVal SizeMulV = getArgSVal(Call, *SizeMultiplierArgN);
          SizeV = SvalBuilder.evalBinOp(State, BO_Mul, SizeV, SizeMulV,
                                        Summary.getArgType(*SizeArgN));
        }
        return SizeV;
      }();

      // The dynamic size of the buffer argument, got from the analyzer engine.
      SVal BufDynSize = getDynamicExtentWithOffset(State, BufV);

      SVal Feasible = SvalBuilder.evalBinOp(State, Op, SizeV, BufDynSize,
                                            SvalBuilder.getContext().BoolTy);
      if (auto F = Feasible.getAs<DefinedOrUnknownSVal>())
        return State->assume(*F, true);

      // We can get here only if the size argument or the dynamic size is
      // undefined. But the dynamic size should never be undefined, only
      // unknown. So, here, the size of the argument is undefined, i.e. we
      // cannot apply the constraint. Actually, other checkers like
      // CallAndMessage should catch this situation earlier, because we call a
      // function with an uninitialized argument.
      llvm_unreachable("Size argument or the dynamic size is Undefined");
    }

    ValueConstraintPtr negate() const override {
      BufferSizeConstraint Tmp(*this);
      Tmp.Op = BinaryOperator::negateComparisonOp(Op);
      return std::make_shared<BufferSizeConstraint>(Tmp);
    }

    bool checkSpecificValidity(const FunctionDecl *FD) const override {
      const bool ValidArg = getArgType(FD, ArgN)->isPointerType();
      assert(ValidArg &&
             "This constraint should be applied only on a pointer type");
      return ValidArg;
    }
  };

  /// The complete list of constraints that defines a single branch.
  typedef std::vector<ValueConstraintPtr> ConstraintSet;

  using ArgTypes = std::vector<Optional<QualType>>;
  using RetType = Optional<QualType>;

  // A placeholder type, we use it whenever we do not care about the concrete
  // type in a Signature.
  const QualType Irrelevant{};
  bool static isIrrelevant(QualType T) { return T.isNull(); }

  // The signature of a function we want to describe with a summary. This is a
  // concessive signature, meaning there may be irrelevant types in the
  // signature which we do not check against a function with concrete types.
  // All types in the spec need to be canonical.
  class Signature {
    using ArgQualTypes = std::vector<QualType>;
    ArgQualTypes ArgTys;
    QualType RetTy;
    // True if any component type is not found by lookup.
    bool Invalid = false;

  public:
    // Construct a signature from optional types. If any of the optional types
    // are not set then the signature will be invalid.
    Signature(ArgTypes ArgTys, RetType RetTy) {
      for (Optional<QualType> Arg : ArgTys) {
        if (!Arg) {
          Invalid = true;
          return;
        } else {
          assertArgTypeSuitableForSignature(*Arg);
          this->ArgTys.push_back(*Arg);
        }
      }
      if (!RetTy) {
        Invalid = true;
        return;
      } else {
        assertRetTypeSuitableForSignature(*RetTy);
        this->RetTy = *RetTy;
      }
    }

    bool isInvalid() const { return Invalid; }
    bool matches(const FunctionDecl *FD) const;

  private:
    static void assertArgTypeSuitableForSignature(QualType T) {
      assert((T.isNull() || !T->isVoidType()) &&
             "We should have no void types in the spec");
      assert((T.isNull() || T.isCanonical()) &&
             "We should only have canonical types in the spec");
    }
    static void assertRetTypeSuitableForSignature(QualType T) {
      assert((T.isNull() || T.isCanonical()) &&
             "We should only have canonical types in the spec");
    }
  };

  static QualType getArgType(const FunctionDecl *FD, ArgNo ArgN) {
    assert(FD && "Function must be set");
    QualType T = (ArgN == Ret)
                     ? FD->getReturnType().getCanonicalType()
                     : FD->getParamDecl(ArgN)->getType().getCanonicalType();
    return T;
  }

  using Cases = std::vector<ConstraintSet>;

  /// A summary includes information about
  ///   * function prototype (signature)
  ///   * a list of argument constraints, that must be true on every branch.
  ///     If these constraints are not satisfied that means a fatal error
  ///     usually resulting in undefined behaviour.
  ///
  /// Application of a summary:
  ///   The signature and argument constraints together contain information
  ///   about which functions are handled by the summary. The signature can use
  ///   "wildcards", i.e. Irrelevant types. Irrelevant type of a parameter in
  ///   a signature means that type is not compared to the type of the parameter
  ///   in the found FunctionDecl. Argument constraints may specify additional
  ///   rules for the given parameter's type, those rules are checked once the
  ///   signature is matched.
  class Summary {
    ConstraintSet ArgConstraints;

    // The function to which the summary applies. This is set after lookup and
    // match to the signature.
    const FunctionDecl *FD = nullptr;

  public:
    Summary() = default;

    Summary &ArgConstraint(ValueConstraintPtr VC) {
      assert(VC->getArgNo() != Ret &&
             "Arg constraint should not refer to the return value");
      ArgConstraints.push_back(VC);
      return *this;
    }

    const ConstraintSet &getArgConstraints() const { return ArgConstraints; }

    QualType getArgType(ArgNo ArgN) const {
      return CtypeFunctionsChecker::getArgType(FD, ArgN);
    }

    // Returns true if the summary should be applied to the given function.
    // And if yes then store the function declaration.
    bool matchesAndSet(const Signature &Sign, const FunctionDecl *FD) {
      bool Result = Sign.matches(FD) && validateByConstraints(FD);
      if (Result) {
        assert(!this->FD && "FD must not be set more than once");
        this->FD = FD;
      }
      return Result;
    }

  private:
    // Once we know the exact type of the function then do sanity check on all
    // the given constraints.
    bool validateByConstraints(const FunctionDecl *FD) const {
      for (const ValueConstraintPtr &Constraint : ArgConstraints)
        if (!Constraint->checkValidity(FD))
          return false;
      return true;
    }
  };

  // The map of all functions supported by the checker. It is initialized
  // lazily, and it doesn't change after initialization.
  using FunctionSummaryMapType = llvm::DenseMap<const FunctionDecl *, Summary>;
  mutable FunctionSummaryMapType FunctionSummaryMap;

  mutable std::unique_ptr<BugType> BT_InvalidArg;
  mutable bool SummariesInitialized = false;

  static SVal getArgSVal(const CallEvent &Call, ArgNo ArgN) {
    return ArgN == Ret ? Call.getReturnValue() : Call.getArgSVal(ArgN);
  }

public:
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;

  enum CheckKind {
    CK_CtypeFunctionArgsChecker,
    CK_CtypeFunctionsTesterChecker,
    CK_NumCheckKinds
  };
  bool ChecksEnabled[CK_NumCheckKinds] = {};
  CheckerNameRef CheckNames[CK_NumCheckKinds];

  bool DisplayLoadedSummaries = false;
  bool ModelPOSIX = false;

private:
  Optional<Summary> findFunctionSummary(const FunctionDecl *FD,
                                        CheckerContext &C) const;
  Optional<Summary> findFunctionSummary(const CallEvent &Call,
                                        CheckerContext &C) const;

  void initFunctionSummaries(CheckerContext &C) const;

  void reportBug(const CallEvent &Call, ExplodedNode *N,
                 const ValueConstraint *VC, const Summary &Summary,
                 CheckerContext &C) const {
    if (!ChecksEnabled[CK_CtypeFunctionArgsChecker])
      return;
    std::string Msg =
        (Twine("Function argument constraint is not satisfied, constraint: ") +
         VC->getName().data())
            .str();
    if (!BT_InvalidArg)
      BT_InvalidArg = std::make_unique<BugType>(
          CheckNames[CK_CtypeFunctionArgsChecker],
          "Unsatisfied argument constraints", categories::LogicError);
    auto R = std::make_unique<PathSensitiveBugReport>(*BT_InvalidArg, Msg, N);

    for (ArgNo ArgN : VC->getArgsToTrack())
      bugreporter::trackExpressionValue(N, Call.getArgExpr(ArgN), *R);

    // Highlight the range of the argument that was violated.
    R->addRange(Call.getArgSourceRange(VC->getArgNo()));

    // Describe the argument constraint in a note.
    R->addNote(VC->describe(C.getState(), Summary), R->getLocation(),
               Call.getArgSourceRange(VC->getArgNo()));

    C.emitReport(std::move(R));
  }
};

const CtypeFunctionsChecker::ArgNo CtypeFunctionsChecker::Ret =
    std::numeric_limits<ArgNo>::max();

} // end of anonymous namespace

static BasicValueFactory &getBVF(ProgramStateRef State) {
  ProgramStateManager &Mgr = State->getStateManager();
  SValBuilder &SVB = Mgr.getSValBuilder();
  return SVB.getBasicValueFactory();
}

std::string CtypeFunctionsChecker::NotNullConstraint::describe(
    ProgramStateRef State, const Summary &Summary) const {
  SmallString<48> Result;
  Result += "The ";
  Result += getArgDesc(ArgN);
  Result += " should not be NULL";
  return Result.c_str();
}

std::string
CtypeFunctionsChecker::RangeConstraint::describe(ProgramStateRef State,
                                                 const Summary &Summary) const {

  BasicValueFactory &BVF = getBVF(State);

  QualType T = Summary.getArgType(getArgNo());
  SmallString<48> Result;
  Result += "The ";
  Result += getArgDesc(ArgN);
  Result += " should be ";

  // Range kind as a string.
  Kind == OutOfRange ? Result += "out of" : Result += "within";

  // Get the range values as a string.
  Result += " the range ";
  if (Ranges.size() > 1)
    Result += "[";
  unsigned I = Ranges.size();
  for (const std::pair<RangeInt, RangeInt> &R : Ranges) {
    Result += "[";
    const llvm::APSInt &Min = BVF.getValue(R.first, T);
    const llvm::APSInt &Max = BVF.getValue(R.second, T);
    Min.toString(Result);
    Result += ", ";
    Max.toString(Result);
    Result += "]";
    if (--I > 0)
      Result += ", ";
  }
  if (Ranges.size() > 1)
    Result += "]";

  return Result.c_str();
}

SmallString<8>
CtypeFunctionsChecker::getArgDesc(CtypeFunctionsChecker::ArgNo ArgN) {
  SmallString<8> Result;
  Result += std::to_string(ArgN + 1);
  Result += llvm::getOrdinalSuffix(ArgN + 1);
  Result += " arg";
  return Result;
}

std::string CtypeFunctionsChecker::BufferSizeConstraint::describe(
    ProgramStateRef State, const Summary &Summary) const {
  SmallString<96> Result;
  Result += "The size of the ";
  Result += getArgDesc(ArgN);
  Result += " should be equal to or less than the value of ";
  if (ConcreteSize) {
    ConcreteSize->toString(Result);
  } else if (SizeArgN) {
    Result += "the ";
    Result += getArgDesc(*SizeArgN);
    if (SizeMultiplierArgN) {
      Result += " times the ";
      Result += getArgDesc(*SizeMultiplierArgN);
    }
  }
  return Result.c_str();
}

ProgramStateRef CtypeFunctionsChecker::RangeConstraint::applyAsOutOfRange(
    ProgramStateRef State, const CallEvent &Call,
    const Summary &Summary) const {
  if (Ranges.empty())
    return State;

  ProgramStateManager &Mgr = State->getStateManager();
  SValBuilder &SVB = Mgr.getSValBuilder();
  BasicValueFactory &BVF = SVB.getBasicValueFactory();
  ConstraintManager &CM = Mgr.getConstraintManager();
  QualType T = Summary.getArgType(getArgNo());
  SVal V = getArgSVal(Call, getArgNo());

  if (auto N = V.getAs<NonLoc>()) {
    const IntRangeVector &R = getRanges();
    size_t E = R.size();
    for (size_t I = 0; I != E; ++I) {
      const llvm::APSInt &Min = BVF.getValue(R[I].first, T);
      const llvm::APSInt &Max = BVF.getValue(R[I].second, T);
      assert(Min <= Max);
      State = CM.assumeInclusiveRange(State, *N, Min, Max, false);
      if (!State)
        break;
    }
  }

  return State;
}

ProgramStateRef CtypeFunctionsChecker::RangeConstraint::applyAsWithinRange(
    ProgramStateRef State, const CallEvent &Call,
    const Summary &Summary) const {
  if (Ranges.empty())
    return State;

  ProgramStateManager &Mgr = State->getStateManager();
  SValBuilder &SVB = Mgr.getSValBuilder();
  BasicValueFactory &BVF = SVB.getBasicValueFactory();
  ConstraintManager &CM = Mgr.getConstraintManager();
  QualType T = Summary.getArgType(getArgNo());
  SVal V = getArgSVal(Call, getArgNo());

  // "WithinRange R" is treated as "outside [T_MIN, T_MAX] \ R".
  // We cut off [T_MIN, min(R) - 1] and [max(R) + 1, T_MAX] if necessary,
  // and then cut away all holes in R one by one.
  //
  // E.g. consider a range list R as [A, B] and [C, D]
  // -------+--------+------------------+------------+----------->
  //        A        B                  C            D
  // Then we assume that the value is not in [-inf, A - 1],
  // then not in [D + 1, +inf], then not in [B + 1, C - 1]
  if (auto N = V.getAs<NonLoc>()) {
    const IntRangeVector &R = getRanges();
    size_t E = R.size();

    const llvm::APSInt &MinusInf = BVF.getMinValue(T);
    const llvm::APSInt &PlusInf = BVF.getMaxValue(T);

    const llvm::APSInt &Left = BVF.getValue(R[0].first - 1ULL, T);
    if (Left != PlusInf) {
      assert(MinusInf <= Left);
      State = CM.assumeInclusiveRange(State, *N, MinusInf, Left, false);
      if (!State)
        return nullptr;
    }

    const llvm::APSInt &Right = BVF.getValue(R[E - 1].second + 1ULL, T);
    if (Right != MinusInf) {
      assert(Right <= PlusInf);
      State = CM.assumeInclusiveRange(State, *N, Right, PlusInf, false);
      if (!State)
        return nullptr;
    }

    for (size_t I = 1; I != E; ++I) {
      const llvm::APSInt &Min = BVF.getValue(R[I - 1].second + 1ULL, T);
      const llvm::APSInt &Max = BVF.getValue(R[I].first - 1ULL, T);
      if (Min <= Max) {
        State = CM.assumeInclusiveRange(State, *N, Min, Max, false);
        if (!State)
          return nullptr;
      }
    }
  }

  return State;
}

ProgramStateRef CtypeFunctionsChecker::ComparisonConstraint::apply(
    ProgramStateRef State, const CallEvent &Call, const Summary &Summary,
    CheckerContext &C) const {

  ProgramStateManager &Mgr = State->getStateManager();
  SValBuilder &SVB = Mgr.getSValBuilder();
  QualType CondT = SVB.getConditionType();
  QualType T = Summary.getArgType(getArgNo());
  SVal V = getArgSVal(Call, getArgNo());

  BinaryOperator::Opcode Op = getOpcode();
  ArgNo OtherArg = getOtherArgNo();
  SVal OtherV = getArgSVal(Call, OtherArg);
  QualType OtherT = Summary.getArgType(OtherArg);
  // Note: we avoid integral promotion for comparison.
  OtherV = SVB.evalCast(OtherV, T, OtherT);
  if (auto CompV = SVB.evalBinOp(State, Op, V, OtherV, CondT)
                       .getAs<DefinedOrUnknownSVal>())
    State = State->assume(*CompV, true);
  return State;
}

void CtypeFunctionsChecker::checkPreCall(const CallEvent &Call,
                                         CheckerContext &C) const {
  Optional<Summary> FoundSummary = findFunctionSummary(Call, C);
  if (!FoundSummary)
    return;

  const Summary &Summary = *FoundSummary;
  ProgramStateRef State = C.getState();

  ProgramStateRef NewState = State;
  for (const ValueConstraintPtr &Constraint : Summary.getArgConstraints()) {
    ProgramStateRef SuccessSt = Constraint->apply(NewState, Call, Summary, C);
    ProgramStateRef FailureSt =
        Constraint->negate()->apply(NewState, Call, Summary, C);
    // The argument constraint is not satisfied.
    if (FailureSt && !SuccessSt) {
      if (ExplodedNode *N = C.generateErrorNode(NewState))
        reportBug(Call, N, Constraint.get(), Summary, C);
      break;
    } else {
      // We will apply the constraint even if we cannot reason about the
      // argument. This means both SuccessSt and FailureSt can be true. If we
      // weren't applying the constraint that would mean that symbolic
      // execution continues on a code whose behaviour is undefined.
      assert(SuccessSt);
      NewState = SuccessSt;
    }
  }
  if (NewState && NewState != State)
    C.addTransition(NewState);
}

bool CtypeFunctionsChecker::Signature::matches(const FunctionDecl *FD) const {
  assert(!isInvalid());
  // Check the number of arguments.
  if (FD->param_size() != ArgTys.size())
    return false;

  // The "restrict" keyword is illegal in C++, however, many libc
  // implementations use the "__restrict" compiler intrinsic in functions
  // prototypes. The "__restrict" keyword qualifies a type as a restricted type
  // even in C++.
  // In case of any non-C99 languages, we don't want to match based on the
  // restrict qualifier because we cannot know if the given libc implementation
  // qualifies the paramter type or not.
  auto RemoveRestrict = [&FD](QualType T) {
    if (!FD->getASTContext().getLangOpts().C99)
      T.removeLocalRestrict();
    return T;
  };

  // Check the return type.
  if (!isIrrelevant(RetTy)) {
    QualType FDRetTy = RemoveRestrict(FD->getReturnType().getCanonicalType());
    if (RetTy != FDRetTy)
      return false;
  }

  // Check the argument types.
  for (size_t I = 0, E = ArgTys.size(); I != E; ++I) {
    QualType ArgTy = ArgTys[I];
    if (isIrrelevant(ArgTy))
      continue;
    QualType FDArgTy =
        RemoveRestrict(FD->getParamDecl(I)->getType().getCanonicalType());
    if (ArgTy != FDArgTy)
      return false;
  }

  return true;
}

Optional<CtypeFunctionsChecker::Summary>
CtypeFunctionsChecker::findFunctionSummary(const FunctionDecl *FD,
                                           CheckerContext &C) const {
  if (!FD)
    return None;

  initFunctionSummaries(C);

  auto FSMI = FunctionSummaryMap.find(FD->getCanonicalDecl());
  if (FSMI == FunctionSummaryMap.end())
    return None;
  return FSMI->second;
}

Optional<CtypeFunctionsChecker::Summary>
CtypeFunctionsChecker::findFunctionSummary(const CallEvent &Call,
                                           CheckerContext &C) const {
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());
  if (!FD)
    return None;
  return findFunctionSummary(FD, C);
}

void CtypeFunctionsChecker::initFunctionSummaries(CheckerContext &C) const {
  if (SummariesInitialized)
    return;

  SValBuilder &SVB = C.getSValBuilder();
  BasicValueFactory &BVF = SVB.getBasicValueFactory();
  const ASTContext &ACtx = BVF.getContext();

  // Helper class to lookup a type by its name.
  class LookupType {
    const ASTContext &ACtx;

  public:
    LookupType(const ASTContext &ACtx) : ACtx(ACtx) {}

    // Find the type. If not found then the optional is not set.
    llvm::Optional<QualType> operator()(StringRef Name) {
      IdentifierInfo &II = ACtx.Idents.get(Name);
      auto LookupRes = ACtx.getTranslationUnitDecl()->lookup(&II);
      if (LookupRes.empty())
        return None;

      // Prioritze typedef declarations.
      // This is needed in case of C struct typedefs. E.g.:
      //   typedef struct FILE FILE;
      // In this case, we have a RecordDecl 'struct FILE' with the name 'FILE'
      // and we have a TypedefDecl with the name 'FILE'.
      for (Decl *D : LookupRes)
        if (auto *TD = dyn_cast<TypedefNameDecl>(D))
          return ACtx.getTypeDeclType(TD).getCanonicalType();

      // Find the first TypeDecl.
      // There maybe cases when a function has the same name as a struct.
      // E.g. in POSIX: `struct stat` and the function `stat()`:
      //   int stat(const char *restrict path, struct stat *restrict buf);
      for (Decl *D : LookupRes)
        if (auto *TD = dyn_cast<TypeDecl>(D))
          return ACtx.getTypeDeclType(TD).getCanonicalType();
      return None;
    }
  } lookupTy(ACtx);

  // Below are auxiliary classes to handle optional types that we get as a
  // result of the lookup.
  class GetRestrictTy {
    const ASTContext &ACtx;

  public:
    GetRestrictTy(const ASTContext &ACtx) : ACtx(ACtx) {}
    QualType operator()(QualType Ty) {
      return ACtx.getLangOpts().C99 ? ACtx.getRestrictType(Ty) : Ty;
    }
    Optional<QualType> operator()(Optional<QualType> Ty) {
      if (Ty)
        return operator()(*Ty);
      return None;
    }
  } getRestrictTy(ACtx);
  class GetPointerTy {
    const ASTContext &ACtx;

  public:
    GetPointerTy(const ASTContext &ACtx) : ACtx(ACtx) {}
    QualType operator()(QualType Ty) { return ACtx.getPointerType(Ty); }
    Optional<QualType> operator()(Optional<QualType> Ty) {
      if (Ty)
        return operator()(*Ty);
      return None;
    }
  } getPointerTy(ACtx);
  class {
  public:
    Optional<QualType> operator()(Optional<QualType> Ty) {
      return Ty ? Optional<QualType>(Ty->withConst()) : None;
    }
    QualType operator()(QualType Ty) { return Ty.withConst(); }
  } getConstTy;
  class GetMaxValue {
    BasicValueFactory &BVF;

  public:
    GetMaxValue(BasicValueFactory &BVF) : BVF(BVF) {}
    Optional<RangeInt> operator()(QualType Ty) {
      return BVF.getMaxValue(Ty).getLimitedValue();
    }
    Optional<RangeInt> operator()(Optional<QualType> Ty) {
      if (Ty) {
        return operator()(*Ty);
      }
      return None;
    }
  } getMaxValue(BVF);

  // These types are useful for writing specifications quickly,
  // New specifications should probably introduce more types.
  // Some types are hard to obtain from the AST, eg. "ssize_t".
  // In such cases it should be possible to provide multiple variants
  // of function summary for common cases (eg. ssize_t could be int or long
  // or long long, so three summary variants would be enough).
  // Of course, function variants are also useful for C++ overloads.
  const QualType VoidTy = ACtx.VoidTy;
  const QualType CharTy = ACtx.CharTy;
  const QualType WCharTy = ACtx.WCharTy;
  const QualType IntTy = ACtx.IntTy;
  const QualType UnsignedIntTy = ACtx.UnsignedIntTy;
  const QualType LongTy = ACtx.LongTy;
  const QualType SizeTy = ACtx.getSizeType();

  const QualType VoidPtrTy = getPointerTy(VoidTy); // void *
  const QualType IntPtrTy = getPointerTy(IntTy);   // int *
  const QualType UnsignedIntPtrTy =
      getPointerTy(UnsignedIntTy); // unsigned int *
  const QualType VoidPtrRestrictTy = getRestrictTy(VoidPtrTy);
  const QualType ConstVoidPtrTy =
      getPointerTy(getConstTy(VoidTy));            // const void *
  const QualType CharPtrTy = getPointerTy(CharTy); // char *
  const QualType CharPtrRestrictTy = getRestrictTy(CharPtrTy);
  const QualType ConstCharPtrTy =
      getPointerTy(getConstTy(CharTy)); // const char *
  const QualType ConstCharPtrRestrictTy = getRestrictTy(ConstCharPtrTy);
  const QualType Wchar_tPtrTy = getPointerTy(WCharTy); // wchar_t *
  const QualType ConstWchar_tPtrTy =
      getPointerTy(getConstTy(WCharTy)); // const wchar_t *
  const QualType ConstVoidPtrRestrictTy = getRestrictTy(ConstVoidPtrTy);
  const QualType SizePtrTy = getPointerTy(SizeTy);
  const QualType SizePtrRestrictTy = getRestrictTy(SizePtrTy);

  const RangeInt IntMax = BVF.getMaxValue(IntTy).getLimitedValue();
  const RangeInt UnsignedIntMax =
      BVF.getMaxValue(UnsignedIntTy).getLimitedValue();
  const RangeInt LongMax = BVF.getMaxValue(LongTy).getLimitedValue();
  const RangeInt SizeMax = BVF.getMaxValue(SizeTy).getLimitedValue();

  // Set UCharRangeMax to min of int or uchar maximum value.
  // The C standard states that the arguments of functions like isalpha must
  // be representable as an unsigned char. Their type is 'int', so the max
  // value of the argument should be min(UCharMax, IntMax). This just happen
  // to be true for commonly used and well tested instruction set
  // architectures, but not for others.
  const RangeInt UCharRangeMax =
      std::min(BVF.getMaxValue(ACtx.UnsignedCharTy).getLimitedValue(), IntMax);

  // The platform dependent value of EOF.
  // Try our best to parse this from the Preprocessor, otherwise fallback to -1.
  const auto EOFv = [&C]() -> RangeInt {
    if (const llvm::Optional<int> OptInt =
            tryExpandAsInteger("EOF", C.getPreprocessor()))
      return *OptInt;
    return -1;
  }();

  // Auxiliary class to aid adding summaries to the summary map.
  struct AddToFunctionSummaryMap {
    const ASTContext &ACtx;
    FunctionSummaryMapType &Map;
    bool DisplayLoadedSummaries;
    AddToFunctionSummaryMap(const ASTContext &ACtx, FunctionSummaryMapType &FSM,
                            bool DisplayLoadedSummaries)
        : ACtx(ACtx), Map(FSM), DisplayLoadedSummaries(DisplayLoadedSummaries) {
    }

    // Add a summary to a FunctionDecl found by lookup. The lookup is performed
    // by the given Name, and in the global scope. The summary will be attached
    // to the found FunctionDecl only if the signatures match.
    //
    // Returns true if the summary has been added, false otherwise.
    bool operator()(StringRef Name, Signature Sign, Summary Sum) {
      if (Sign.isInvalid())
        return false;
      IdentifierInfo &II = ACtx.Idents.get(Name);
      auto LookupRes = ACtx.getTranslationUnitDecl()->lookup(&II);
      if (LookupRes.empty())
        return false;
      for (Decl *D : LookupRes) {
        if (auto *FD = dyn_cast<FunctionDecl>(D)) {
          if (Sum.matchesAndSet(Sign, FD)) {
            auto Res = Map.insert({FD->getCanonicalDecl(), Sum});
            assert(Res.second && "Function already has a summary set!");
            (void)Res;
            if (DisplayLoadedSummaries) {
              llvm::errs() << "Loaded summary for: ";
              FD->print(llvm::errs());
              llvm::errs() << "\n";
            }
            return true;
          }
        }
      }
      return false;
    }
    // Add the same summary for different names with the Signature explicitly
    // given.
    void operator()(std::vector<StringRef> Names, Signature Sign, Summary Sum) {
      for (StringRef Name : Names)
        operator()(Name, Sign, Sum);
    }
  } addToFunctionSummaryMap(ACtx, FunctionSummaryMap, DisplayLoadedSummaries);

  // Below are helpers functions to create the summaries.
  auto ArgumentCondition = [](ArgNo ArgN, RangeKind Kind,
                              IntRangeVector Ranges) {
    return std::make_shared<RangeConstraint>(ArgN, Kind, Ranges);
  };
  auto BufferSize = [](auto... Args) {
    return std::make_shared<BufferSizeConstraint>(Args...);
  };
  struct {
    auto operator()(RangeKind Kind, IntRangeVector Ranges) {
      return std::make_shared<RangeConstraint>(Ret, Kind, Ranges);
    }
    auto operator()(BinaryOperator::Opcode Op, ArgNo OtherArgN) {
      return std::make_shared<ComparisonConstraint>(Ret, Op, OtherArgN);
    }
  } ReturnValueCondition;
  struct {
    auto operator()(RangeInt b, RangeInt e) {
      return IntRangeVector{std::pair<RangeInt, RangeInt>{b, e}};
    }
    auto operator()(RangeInt b, Optional<RangeInt> e) {
      if (e)
        return IntRangeVector{std::pair<RangeInt, RangeInt>{b, *e}};
      return IntRangeVector{};
    }
    auto operator()(std::pair<RangeInt, RangeInt> i0,
                    std::pair<RangeInt, Optional<RangeInt>> i1) {
      if (i1.second)
        return IntRangeVector{i0, {i1.first, *(i1.second)}};
      return IntRangeVector{i0};
    }
  } Range;
  auto SingleValue = [](RangeInt v) {
    return IntRangeVector{std::pair<RangeInt, RangeInt>{v, v}};
  };
  auto LessThanOrEq = BO_LE;
  auto NotNull = [&](ArgNo ArgN) {
    return std::make_shared<NotNullConstraint>(ArgN);
  };

  // Removed unused QualType shortcut.

  // We are finally ready to define specifications for all supported functions.
  //
  // Argument ranges should always cover all variants. If return value
  // is completely unknown, omit it from the respective range set.
  //
  // Every item in the list of range sets represents a particular
  // execution path the analyzer would need to explore once
  // the call is modeled - a new program state is constructed
  // for every range set, and each range line in the range set
  // corresponds to a specific constraint within this state.

  // The isascii() family of functions.
  // The argument should be representable as unsigned char or is not equal
  // to EOF. See e.g. Misra C 2012 21.13.
  addToFunctionSummaryMap(
      "isalnum", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isalpha", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isascii", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isblank", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "iscntrl", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isdigit", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isgraph", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "islower", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isprint", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "ispunct", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isspace", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isupper", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "isxdigit", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "toupper", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "tolower", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));
  addToFunctionSummaryMap(
      "toascii", Signature(ArgTypes{IntTy}, RetType{IntTy}),
      Summary().ArgConstraint(ArgumentCondition(
          0U, WithinRange, {{EOFv, EOFv}, {0, UCharRangeMax}})));

  // Removed ModelPOSIX and other unrelated function summaries

  // Functions for testing.
  if (ChecksEnabled[CK_CtypeFunctionsTesterChecker]) {
    addToFunctionSummaryMap("__not_null",
                            Signature(ArgTypes{IntPtrTy}, RetType{IntTy}),
                            Summary().ArgConstraint(NotNull(ArgNo(0))));

    // Test range values.
    addToFunctionSummaryMap("__single_val_1",
                            Signature(ArgTypes{IntTy}, RetType{IntTy}),
                            Summary().ArgConstraint(ArgumentCondition(
                                0U, WithinRange, SingleValue(1))));
    addToFunctionSummaryMap("__range_1_2",
                            Signature(ArgTypes{IntTy}, RetType{IntTy}),
                            Summary().ArgConstraint(ArgumentCondition(
                                0U, WithinRange, Range(1, 2))));
    addToFunctionSummaryMap("__range_1_2__4_5",
                            Signature(ArgTypes{IntTy}, RetType{IntTy}),
                            Summary().ArgConstraint(ArgumentCondition(
                                0U, WithinRange, Range({1, 2}, {4, 5}))));

    // Test range kind.
    addToFunctionSummaryMap("__within",
                            Signature(ArgTypes{IntTy}, RetType{IntTy}),
                            Summary().ArgConstraint(ArgumentCondition(
                                0U, WithinRange, SingleValue(1))));
    addToFunctionSummaryMap("__out_of",
                            Signature(ArgTypes{IntTy}, RetType{IntTy}),
                            Summary().ArgConstraint(ArgumentCondition(
                                0U, OutOfRange, SingleValue(1))));

    addToFunctionSummaryMap(
        "__two_constrained_args",
        Signature(ArgTypes{IntTy, IntTy}, RetType{IntTy}),
        Summary()
            .ArgConstraint(ArgumentCondition(0U, WithinRange, SingleValue(1)))
            .ArgConstraint(ArgumentCondition(1U, WithinRange, SingleValue(1))));
    addToFunctionSummaryMap(
        "__arg_constrained_twice", Signature(ArgTypes{IntTy}, RetType{IntTy}),
        Summary()
            .ArgConstraint(ArgumentCondition(0U, OutOfRange, SingleValue(1)))
            .ArgConstraint(ArgumentCondition(0U, OutOfRange, SingleValue(2))));
    addToFunctionSummaryMap(
        "__defaultparam",
        Signature(ArgTypes{Irrelevant, IntTy}, RetType{IntTy}),
        Summary().ArgConstraint(NotNull(ArgNo(0))));
    addToFunctionSummaryMap(
        "__variadic",
        Signature(ArgTypes{VoidPtrTy, ConstCharPtrTy}, RetType{IntTy}),
        Summary()
            .ArgConstraint(NotNull(ArgNo(0)))
            .ArgConstraint(NotNull(ArgNo(1))));
    addToFunctionSummaryMap(
        "__buf_size_arg_constraint",
        Signature(ArgTypes{ConstVoidPtrTy, SizeTy}, RetType{IntTy}),
        Summary().ArgConstraint(
            BufferSize(/*Buffer=*/ArgNo(0), /*BufSize=*/ArgNo(1))));
    addToFunctionSummaryMap(
        "__buf_size_arg_constraint_mul",
        Signature(ArgTypes{ConstVoidPtrTy, SizeTy, SizeTy}, RetType{IntTy}),
        Summary().ArgConstraint(BufferSize(/*Buffer=*/ArgNo(0),
                                           /*BufSize=*/ArgNo(1),
                                           /*BufSizeMultiplier=*/ArgNo(2))));
    addToFunctionSummaryMap("__buf_size_arg_constraint_concrete",
                            Signature(ArgTypes{ConstVoidPtrTy}, RetType{IntTy}),
                            Summary().ArgConstraint(BufferSize(
                                /*Buffer=*/ArgNo(0),
                                /*BufSize=*/BVF.getValue(10, IntTy))));
    addToFunctionSummaryMap(
        {"__test_restrict_param_0", "__test_restrict_param_1",
         "__test_restrict_param_2"},
        Signature(ArgTypes{VoidPtrRestrictTy}, RetType{VoidTy}), Summary());
  }

  SummariesInitialized = true;
}

void ento::registerCtypeFunctionsChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<CtypeFunctionsChecker>();
  Checker->DisplayLoadedSummaries =
      mgr.getAnalyzerOptions().getCheckerBooleanOption(
          Checker, "DisplayLoadedSummaries");
}

bool ento::shouldRegisterCtypeFunctionsChecker(const CheckerManager &mgr) {
  return true;
}

#define REGISTER_CHECKER(name)                                                 \
  void ento::register##name(CheckerManager &mgr) {                             \
    CtypeFunctionsChecker *checker = mgr.getChecker<CtypeFunctionsChecker>();  \
    checker->ChecksEnabled[CtypeFunctionsChecker::CK_##name] = true;           \
    checker->CheckNames[CtypeFunctionsChecker::CK_##name] =                    \
        mgr.getCurrentCheckerName();                                           \
  }                                                                            \
                                                                               \
  bool ento::shouldRegister##name(const CheckerManager &mgr) { return true; }

REGISTER_CHECKER(CtypeFunctionArgsChecker)
REGISTER_CHECKER(CtypeFunctionsTesterChecker)
