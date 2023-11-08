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

//=== misrac-2012-22_3-OpenFileChecker.cpp - Check whether opening the same file
// ---*- C++ -*-===//
//
// The checker that is responsible for rule 22.3.
//
// The non-compliant case for the memcmp() arguments:
//  Open the same file for read and write with different streams (open the same
//  file for read-only is compliant)
// All other cases are complicant.
//
// There are two maps:
//  1. StreamMap: map Symbol of file stream to StreamState. There are two
//     states in StreamState (open and read-only, and open but not read-only).
//     Closed stream will be directly removed from StreamMap
//  2. FileMap: map Filename to SymbolSet (a set which contains symbols of file
//     stream opening the same file)
//
// The general process is:
//  In Function checkPostCall():
//   1. Match fopen() and get its arguments (filename and mode)
//   2. Check whether filename in FileMap. If true, continue to step 3.
//   Otherwise, skip to step 5
//   3. Use checkOpenSameFile() to check whether rule 22.3 is violated. There
//   are two cases:
//    (1) If current file stream mode is read-only, all file streams in
//        Filemap[filename] should not be open but not read-only
//    (2) If current file stream mode is not read-only, all file streams in
//        Filemap[filename] must be closed
//   4. Once the checkOpenSameFile() returns true, the checker will report a
//      bug.
//   5. Update two maps
//  In Function checkPreCall():
//   Remove the file stream from StreamMap if it is closed by fclose()
//
// Problems:
//  - The checker only supports streams using fopen. Similar features provided
//  by the execution environment is not included
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

struct SymbolSet {
 private:
  SRSet SS;
  SymbolSet(SRSet InSS) : SS(InSS) {}

 public:
  SRSet getSymbolSet() const { return SS; }

  static SymbolSet getSymbolSetStruct(SRSet InSS) { return SymbolSet(InSS); }

  bool operator==(const SymbolSet &X) const { return SS == X.SS; }
  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddPointer(&SS); }
};

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

struct Filename {
 private:
  std::string FN;
  Filename(std::string InFN) : FN(InFN) {}

 public:
  std::string getFilename() const { return FN; }

  static Filename getFilenameStruct(std::string InFN) { return Filename(InFN); }

  bool operator==(const Filename &X) const { return FN == X.FN; }
  bool operator<(const Filename &X) const { return FN < X.FN; }
  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddString(FN); }
};

class OpenSameFileChecker : public Checker<check::PostCall, check::PreCall> {
  CallDescription OpenFn, CloseFn;

  mutable std::unique_ptr<BugType> BT;

  bool checkOpenSameFile(ProgramStateRef State, SRSet SS,
                         std::string Mode) const;

  void reportBug(CheckerContext &C) const {
    if (!BT) {
      BT = std::make_unique<BugType>(this, "Wrong usage of function",
                                     categories::LogicError);
    }
    ExplodedNode *N = C.generateErrorNode();
    auto R = std::make_unique<PathSensitiveBugReport>(
        *BT, "Open the same file for read and write access", N);
    C.emitReport(std::move(R));
  }

 public:
  OpenSameFileChecker();

  /// Process fopen.
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  /// Process fclose.
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

}  // end anonymous namespace

REGISTER_MAP_WITH_PROGRAMSTATE(StreamMap, SymbolRef, StreamState)
REGISTER_MAP_WITH_PROGRAMSTATE(FileMap, Filename, SymbolSet)

OpenSameFileChecker::OpenSameFileChecker()
    : OpenFn({CDF_MaybeBuiltin, "fopen", 2}), CloseFn({CDF_MaybeBuiltin, "fclose", 1}) {}

void OpenSameFileChecker::checkPostCall(const CallEvent &Call,
                                        CheckerContext &C) const {
  if (!Call.isGlobalCFunction()) return;

  if (!OpenFn.matches(Call)) return;

  // Get the symbolic value corresponding to the file handle.
  SymbolRef FileDesc = Call.getReturnValue().getAsSymbol();
  if (!FileDesc) return;

  // Generate the next transition (an edge in the exploded graph).
  ProgramStateRef State = C.getState();
  const SVal Filename = Call.getArgSVal(0);
  const SVal Mode = Call.getArgSVal(1);
  if (Filename.isUnknownOrUndef() || Mode.isUnknownOrUndef() ||
      !Filename.getType(C.getASTContext())->isPointerType()) {
    return;
  }

  std::string FilenameStr =
      Filename.getAsRegion()->getBaseRegion()->getString();
  FilenameStr = FilenameStr.substr(1, FilenameStr.size() - 2);
  std::string ModeStr = Mode.getAsRegion()->getBaseRegion()->getString();
  ModeStr = ModeStr.substr(1, ModeStr.size() - 2);

  const SymbolSet *SymbolS =
      State->get<FileMap>(Filename::getFilenameStruct(FilenameStr));
  SRSet SRS;
  if (SymbolS) {
    SRS = SymbolS->getSymbolSet();
  }
  if (checkOpenSameFile(State, SRS, ModeStr)) {
    reportBug(C);
    return;
  }
  if (ModeStr == "r") {
    State = State->set<StreamMap>(FileDesc, StreamState::getOnlyReadOpened());
  } else {
    State =
        State->set<StreamMap>(FileDesc, StreamState::getNotOnlyReadOpened());
  }
  SRS.insert(FileDesc);

  State = State->set<FileMap>(Filename::getFilenameStruct(FilenameStr),
                              SymbolSet::getSymbolSetStruct(SRS));
  C.addTransition(State);
}

void OpenSameFileChecker::checkPreCall(const CallEvent &Call,
                                       CheckerContext &C) const {
  if (!Call.isGlobalCFunction()) return;

  if (!CloseFn.matches(Call)) return;

  // Get the symbolic value corresponding to the file handle.
  SymbolRef FileDesc = Call.getArgSVal(0).getAsSymbol();
  if (!FileDesc) return;

  ProgramStateRef State = C.getState();

  // Generate the next transition, in which the stream is closed.
  State = State->remove<StreamMap>(FileDesc);
  C.addTransition(State);
}

bool OpenSameFileChecker::checkOpenSameFile(ProgramStateRef State,
                                            SRSet SymbolS,
                                            std::string Mode) const {
  for (auto it = SymbolS.begin(); it != SymbolS.end(); it++) {
    if ((Mode == "r" && (State->get<StreamMap>(*it) &&
                         State->get<StreamMap>(*it)->isNotOnlyReadOpened())) ||
        (Mode != "r" && State->get<StreamMap>(*it))) {
      return true;
    }
  }
  return false;
}

void ento::registerOpenSameFileChecker(CheckerManager &mgr) {
  auto *Checker = mgr.registerChecker<OpenSameFileChecker>();
}

bool ento::shouldRegisterOpenSameFileChecker(const CheckerManager &mgr) {
  return true;
}
