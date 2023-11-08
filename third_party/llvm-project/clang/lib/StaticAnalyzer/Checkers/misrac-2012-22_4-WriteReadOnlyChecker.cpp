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

//=== misrac-2012-22_4-WriteReadOnlyChecker.cpp - Check whether writing
//read-only file ---*- C++ -*-===//
//
// The checker that is responsible for rule 22.4.
//
// The non-compliant case:
//  Write to a file which is read-only
//
// There is one maps:
//  1. StreamMap: map Symbol of file stream to StreamState. There are two
//     states in StreamState (open and read-only, open but not read-only and
//     closed).
//
// The general process is:
//  In Function checkPostCall():
//   1. Match fopen() and get its mode arguments
//   2. Update StreamMap according to fopen mode
//
//  In Function checkPreCall():
//   There are four cases:
//    1. If fclose() is matched, remove the corresponding file stream from
//    StreamMap
//    2. if fputc(), putc(), putw() or fputs() is matched, get the second
//    argument (file stream)
//    3. if fprtinf() is matched, get the first argument (file stream)
//    4. if fwrite() is matched, get the fourth argument (file stream)
//   In the last three cases, check whether the file stream is read-only. If
//   true, report a bug
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
typedef std::set<SymbolRef> SRSet;

struct StreamState {
 private:
  enum Kind { OnlyReadOpened, NotOnlyReadOpened } K;
  StreamState(Kind InK) : K(InK) {}

 public:
  bool isOnlyReadOpened() const { return K == OnlyReadOpened; }
  bool isNotOnlyReadOpened() const { return K == NotOnlyReadOpened; }

  static StreamState getOnlyReadOpened() { return StreamState(OnlyReadOpened); }
  static StreamState getNotOnlyReadOpened() {
    return StreamState(NotOnlyReadOpened);
  }

  bool operator==(const StreamState &X) const { return K == X.K; }
  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(K); }
};

class WriteReadOnlyChecker : public Checker<check::PostCall, check::PreCall> {
  CallDescription OpenFn, CloseFn, FprintfFn, FwriteFn;
  CallDescriptionMap<bool> FuncPutList = {
      {{CDF_MaybeBuiltin, "fputc", 2}, true},
      {{CDF_MaybeBuiltin, "putc", 2}, true},
      {{CDF_MaybeBuiltin, "fputs", 2}, true},
      {{CDF_MaybeBuiltin, "putw", 2}, true},
  };

  mutable std::unique_ptr<BugType> BT;

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "Wrong usage of function",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT, "Write to a file stream which is read-only", N);
    C.emitReport(std::move(R));
  }

 public:
  WriteReadOnlyChecker();

  /// Process fopen.
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  /// Process file write functions and fclose.
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

}  // end anonymous namespace

REGISTER_MAP_WITH_PROGRAMSTATE(StreamMap, SymbolRef, StreamState)

WriteReadOnlyChecker::WriteReadOnlyChecker()
    : OpenFn({CDF_MaybeBuiltin, "fopen", 2}),
      CloseFn({CDF_MaybeBuiltin, "fclose", 1}),
      FprintfFn({CDF_MaybeBuiltin, "fprintf"}),
      FwriteFn({CDF_MaybeBuiltin, "fwrite", 4}) {}

void WriteReadOnlyChecker::checkPostCall(const CallEvent &Call,
                                         CheckerContext &C) const {
  if (!Call.isGlobalCFunction()) return;

  if (!OpenFn.matches(Call)) return;

  // Get the symbolic value corresponding to the file handle.
  SymbolRef FileDesc = Call.getReturnValue().getAsSymbol();
  if (!FileDesc) return;

  // Generate the next transition (an edge in the exploded graph).
  ProgramStateRef State = C.getState();
  const SVal Mode = Call.getArgSVal(1);
  if (Mode.isUnknownOrUndef()) {
    return;
  }

  std::string ModeStr = Mode.getAsRegion()->getBaseRegion()->getString();
  ModeStr = ModeStr.substr(1, ModeStr.size() - 2);

  if (ModeStr == "r") {
    State = State->set<StreamMap>(FileDesc, StreamState::getOnlyReadOpened());
  } else {
    State =
        State->set<StreamMap>(FileDesc, StreamState::getNotOnlyReadOpened());
  }

  C.addTransition(State);
}

void WriteReadOnlyChecker::checkPreCall(const CallEvent &Call,
                                        CheckerContext &C) const {
  if (!Call.isGlobalCFunction()) return;

  SymbolRef FileDesc;
  ProgramStateRef State = C.getState();
  int FileDecsPos = -1;

  if (CloseFn.matches(Call)) {
    FileDecsPos = 0;
    FileDesc = Call.getArgSVal(FileDecsPos).getAsSymbol();
    if (!FileDesc) return;
    State = State->remove<StreamMap>(FileDesc);
  }
  if (FuncPutList.lookup(Call)) {
    FileDecsPos = 1;
  }
  if (FprintfFn.matches(Call)) {
    FileDecsPos = 0;
  }
  if (FwriteFn.matches(Call)) {
    FileDecsPos = 3;
  }
  if (FileDecsPos == -1) return;

  FileDesc = Call.getArgSVal(FileDecsPos).getAsSymbol();
  if (!FileDesc) return;
  const StreamState *SS = State->get<StreamMap>(FileDesc);
  if (SS && SS->isOnlyReadOpened()) {
    reportBug(C);
    return;
  }
  C.addTransition(State);
}

void ento::registerWriteReadOnlyChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<WriteReadOnlyChecker>();
}

bool ento::shouldRegisterWriteReadOnlyChecker(const CheckerManager &mgr) {
  return true;
}
