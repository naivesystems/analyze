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

//=== misra-cxx-2008-R-0_1_2-Unreachable.cpp - Generalized dead code checker ---*- C++ -*-===//
//
// This file implements a generalized unreachable code checker using a
// path-sensitive analysis. We mark any path visited, and then walk the CFG as a
// post-analysis to determine what was never visited.
//
// A similar flow-sensitive only check exists in Analysis/ReachableCode.cpp
// New function in addition to the original checker
// relaxStdbool
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/AST/ParentMap.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/SourceManager.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerHelpers.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SVals.h"
#include "llvm/ADT/SmallSet.h"

using namespace clang;
using namespace ento;

namespace {
/*
 * This function is adopted from the original csa checker
 * In the original checker any variable that is defined using macro
 * Is marked as unreachable
 * The condition is relaxed for variable in macro that have value 0/1
 * Since they maybe used as the boolean condition in the loop / if
 * For instance true / false they are defined in the stdbool
*/
bool relaxStdbool(const Stmt *S) {
  // Recursively find any substatements containing macros
  if (S->getBeginLoc().isMacroID()){
    // Relax condition for true/flase
    if (const auto *I = dyn_cast<IntegerLiteral>(S))
        return !(I-> getValue() == 0 || I->getValue() == 1);
    return true;
  }

  if (S->getEndLoc().isMacroID()){
    // Relax condition for true/flase
    if (const auto *I = dyn_cast<IntegerLiteral>(S))
        return !(I-> getValue() == 0 || I->getValue() == 1);
    return true;
  }

  for (const Stmt *Child : S->children())
    if (Child && relaxStdbool(Child))
      return true;

  return false;
}
class UnreachableMisraCodeChecker : public Checker<check::EndAnalysis> {
public:
  void checkEndAnalysis(ExplodedGraph &G, BugReporter &B,
                        ExprEngine &Eng) const;
private:
  typedef llvm::SmallSet<unsigned, 32> CFGBlocksSet;

  static inline const Stmt *getUnreachableStmt(const CFGBlock *CB);
  static void FindUnreachableEntryPoints(const CFGBlock *CB,
                                         CFGBlocksSet &reachable,
                                         CFGBlocksSet &visited);
  static bool isInvalidPath(const CFGBlock *CB, const ParentMap &PM);
  static inline bool isEmptyCFGBlock(const CFGBlock *CB);
};
}

void UnreachableMisraCodeChecker::checkEndAnalysis(ExplodedGraph &G,
                                              BugReporter &B,
                                              ExprEngine &Eng) const {
  CFGBlocksSet reachable, visited;

  if (Eng.hasWorkRemaining())
    return;

  const Decl *D = nullptr;
  CFG *C = nullptr;
  const ParentMap *PM = nullptr;
  const LocationContext *LC = nullptr;
  // Iterate over ExplodedGraph
  for (ExplodedGraph::node_iterator I = G.nodes_begin(), E = G.nodes_end();
      I != E; ++I) {
    const ProgramPoint &P = I->getLocation();
    LC = P.getLocationContext();
    if (!LC->inTopFrame())
      continue;

    if (!D)
      D = LC->getAnalysisDeclContext()->getDecl();

    // Save the CFG if we don't have it already
    if (!C)
      C = LC->getAnalysisDeclContext()->getUnoptimizedCFG();
    if (!PM)
      PM = &LC->getParentMap();

    if (Optional<BlockEntrance> BE = P.getAs<BlockEntrance>()) {
      const CFGBlock *CB = BE->getBlock();
      reachable.insert(CB->getBlockID());
    }
  }

  // Bail out if we didn't get the CFG or the ParentMap.
  if (!D || !C || !PM)
    return;

  // Don't do anything for template instantiations.  Proving that code
  // in a template instantiation is unreachable means proving that it is
  // unreachable in all instantiations.
  if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
    if (FD->isTemplateInstantiation())
      return;

  // Find CFGBlocks that were not covered by any node
  for (CFG::const_iterator I = C->begin(), E = C->end(); I != E; ++I) {
    const CFGBlock *CB = *I;
    // Check if the block is unreachable
    if (reachable.count(CB->getBlockID()))
      continue;

    // Check if the block is empty (an artificial block)
    if (isEmptyCFGBlock(CB))
      continue;

    // Find the entry points for this block
    if (!visited.count(CB->getBlockID()))
      FindUnreachableEntryPoints(CB, reachable, visited);

    // This block may have been pruned; check if we still want to report it
    if (reachable.count(CB->getBlockID()))
      continue;

    // Check for false positives
    if (isInvalidPath(CB, *PM))
      continue;

    // It is good practice to always have a "default" label in a "switch", even
    // if we should never get there. It can be used to detect errors, for
    // instance. Unreachable code directly under a "default" label is therefore
    // likely to be a false positive.
    if (const Stmt *label = CB->getLabel())
      if (label->getStmtClass() == Stmt::DefaultStmtClass)
        continue;

    // Special case for __builtin_unreachable.
    // FIXME: This should be extended to include other unreachable markers,
    // such as llvm_unreachable.
    if (!CB->empty()) {
      bool foundUnreachable = false;
      for (CFGBlock::const_iterator ci = CB->begin(), ce = CB->end();
           ci != ce; ++ci) {
        if (Optional<CFGStmt> S = (*ci).getAs<CFGStmt>())
          if (const CallExpr *CE = dyn_cast<CallExpr>(S->getStmt())) {
            if (CE->getBuiltinCallee() == Builtin::BI__builtin_unreachable ||
                CE->isBuiltinAssumeFalse(Eng.getContext())) {
              foundUnreachable = true;
              break;
            }
          }
      }
      if (foundUnreachable)
        continue;
    }

    // We found a block that wasn't covered - find the statement to report
    SourceRange SR;
    PathDiagnosticLocation DL;
    SourceLocation SL;
    if (const Stmt *S = getUnreachableStmt(CB)) {
      // In macros, 'do {...} while (0)' is often used. Don't warn about the
      // condition 0 when it is unreachable.
      if (S->getBeginLoc().isMacroID())
        if (const auto *I = dyn_cast<IntegerLiteral>(S))
          if (I->getValue() == 0ULL)
            if (const Stmt *Parent = PM->getParent(S))
              if (isa<DoStmt>(Parent))
                continue;
      SR = S->getSourceRange();
      DL = PathDiagnosticLocation::createBegin(S, B.getSourceManager(), LC);
      SL = DL.asLocation();
      if (SR.isInvalid() || !SL.isValid())
        continue;
    }
    else
      continue;

    // Check if the SourceLocation is in a system header
    const SourceManager &SM = B.getSourceManager();
    if (SM.isInSystemHeader(SL) || SM.isInExternCSystemHeader(SL))
      continue;

    B.EmitBasicReport(D, this, "Unreachable code", "Dead code",
                      "This statement is never executed", DL, SR);
  }
}

// Recursively finds the entry point(s) for this dead CFGBlock.
void UnreachableMisraCodeChecker::FindUnreachableEntryPoints(const CFGBlock *CB,
                                                        CFGBlocksSet &reachable,
                                                        CFGBlocksSet &visited) {
  visited.insert(CB->getBlockID());

  for (CFGBlock::const_pred_iterator I = CB->pred_begin(), E = CB->pred_end();
      I != E; ++I) {
    if (!*I)
      continue;

    if (!reachable.count((*I)->getBlockID())) {
      // If we find an unreachable predecessor, mark this block as reachable so
      // we don't report this block
      reachable.insert(CB->getBlockID());
      if (!visited.count((*I)->getBlockID()))
        // If we haven't previously visited the unreachable predecessor, recurse
        FindUnreachableEntryPoints(*I, reachable, visited);
    }
  }
}

// Find the Stmt* in a CFGBlock for reporting a warning
const Stmt *UnreachableMisraCodeChecker::getUnreachableStmt(const CFGBlock *CB) {
  for (CFGBlock::const_iterator I = CB->begin(), E = CB->end(); I != E; ++I) {
    if (Optional<CFGStmt> S = I->getAs<CFGStmt>()) {
      if (!isa<DeclStmt>(S->getStmt()))
        return S->getStmt();
    }
  }
  if (const Stmt *S = CB->getTerminatorStmt())
    return S;
  else
    return nullptr;
}

// Determines if the path to this CFGBlock contained an element that infers this
// block is a false positive. We assume that FindUnreachableEntryPoints has
// already marked only the entry points to any dead code, so we need only to
// find the condition that led to this block (the predecessor of this block.)
// There will never be more than one predecessor.
bool UnreachableMisraCodeChecker::isInvalidPath(const CFGBlock *CB,
                                           const ParentMap &PM) {
  // We only expect a predecessor size of 0 or 1. If it is >1, then an external
  // condition has broken our assumption (for example, a sink being placed by
  // another check). In these cases, we choose not to report.
  if (CB->pred_size() > 1)
    return true;

  // If there are no predecessors, then this block is trivially unreachable
  if (CB->pred_size() == 0)
    return false;

  const CFGBlock *pred = *CB->pred_begin();
  if (!pred)
    return false;

  // Get the predecessor block's terminator condition
  const Stmt *cond = pred->getTerminatorCondition();

  //assert(cond && "CFGBlock's predecessor has a terminator condition");
  // The previous assertion is invalid in some cases (eg do/while). Leaving
  // reporting of these situations on at the moment to help triage these cases.
  if (!cond)
    return false;

  // Run each of the checks on the conditions
  return relaxStdbool(cond) || containsEnum(cond) ||
         containsStaticLocal(cond) || containsBuiltinOffsetOf(cond) ||
         containsStmt<UnaryExprOrTypeTraitExpr>(cond);
}

// Returns true if the given CFGBlock is empty
bool UnreachableMisraCodeChecker::isEmptyCFGBlock(const CFGBlock *CB) {
  return CB->getLabel() == nullptr // No labels
      && CB->size() == 0           // No statements
      && !CB->getTerminatorStmt(); // No terminator
}

void ento::registerUnreachableMisraCodeChecker(CheckerManager &mgr) {
  mgr.registerChecker<UnreachableMisraCodeChecker>();
}

bool ento::shouldRegisterUnreachableMisraCodeChecker(const CheckerManager &mgr) {
  return true;
}
