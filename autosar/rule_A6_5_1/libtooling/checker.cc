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

#include "autosar/rule_A6_5_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A for-loop that loops through all elements of the container and does not use its loop-counter shall not be used.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A6_5_1 {
namespace libtooling {

static const char LoopNameArray[] = "forLoopArray";
static const char LoopNameIterator[] = "forLoopIterator";
static const char LoopNameReverseIterator[] = "forLoopReverseIterator";
static const char LoopNamePseudoArray[] = "forLoopPseudoArray";
static const char ConditionBoundName[] = "conditionBound";
static const char InitVarName[] = "initVar";
static const char BeginCallName[] = "beginCall";
static const char EndCallName[] = "endCall";
static const char EndVarName[] = "endVar";

static const StatementMatcher integerComparisonMatcher() {
  return expr(ignoringParenImpCasts(
      declRefExpr(to(varDecl(equalsBoundNode(InitVarName))))));
}

static const DeclarationMatcher initToZeroMatcher() {
  return varDecl(
             hasInitializer(ignoringParenImpCasts(integerLiteral(equals(0)))))
      .bind(InitVarName);
}

static const StatementMatcher incrementVarMatcher() {
  return declRefExpr(to(varDecl(equalsBoundNode(InitVarName))));
}

static StatementMatcher arrayConditionMatcher(
    internal::Matcher<Expr> LimitExpr) {
  return binaryOperator(
      anyOf(allOf(hasOperatorName("<"), hasLHS(integerComparisonMatcher()),
                  hasRHS(LimitExpr)),
            allOf(hasOperatorName(">"), hasLHS(LimitExpr),
                  hasRHS(integerComparisonMatcher())),
            allOf(hasOperatorName("!="),
                  hasOperands(integerComparisonMatcher(), LimitExpr))));
}

/// The matcher for loops over arrays.
/// \code
///   for (int i = 0; i < 3 + 2; ++i) { ... }
/// \endcode
/// The following string identifiers are bound to these parts of the AST:
///   ConditionBoundName: '3 + 2' (as an Expr)
///   InitVarName: 'i' (as a VarDecl)
///   LoopName: The entire for loop (as a ForStmt)
///
/// Client code will need to make sure that:
///   - The index variable is only used as an array index.
///   - All arrays indexed by the loop are the same.
StatementMatcher makeArrayLoopMatcher() {
  StatementMatcher ArrayBoundMatcher =
      expr(hasType(isInteger()), unless(binaryOperator()),
           unless(implicitCastExpr(hasCastKind(CK_LValueToRValue),
                                   has(unaryOperator()))))
          .bind(ConditionBoundName);

  return forStmt(unless(isInTemplateInstantiation()),
                 hasLoopInit(declStmt(hasSingleDecl(initToZeroMatcher()))),
                 hasCondition(arrayConditionMatcher(ArrayBoundMatcher)),
                 hasIncrement(
                     unaryOperator(hasOperatorName("++"),
                                   hasUnaryOperand(incrementVarMatcher()))),
                 unless(hasBody(hasDescendant(incrementVarMatcher()))))
      .bind(LoopNameArray);
}

/// The matcher used for iterator-based for loops.
///
/// This matcher is more flexible than array-based loops. It will match
/// catch loops of the following textual forms (regardless of whether the
/// iterator type is actually a pointer type or a class type):
///
/// \code
///   for (containerType::iterator it = container.begin(),
///        e = createIterator(); it != e; ++it) { ... }
///   for (containerType::iterator it = container.begin();
///        it != anotherContainer.end(); ++it) { ... }
/// \endcode
/// The following string identifiers are bound to the parts of the AST:
///   InitVarName: 'it' (as a VarDecl)
///   LoopName: The entire for loop (as a ForStmt)
///   In the first example only:
///     EndVarName: 'e' (as a VarDecl)
///   In the second example only:
///     EndCallName: 'container.end()' (as a CXXMemberCallExpr)
///
/// Client code will need to make sure that:
///   - The two containers on which 'begin' and 'end' are called are the same.
StatementMatcher makeIteratorLoopMatcher(bool IsReverse) {
  auto BeginNameMatcher = IsReverse ? hasAnyName("rbegin", "crbegin")
                                    : hasAnyName("begin", "cbegin");

  auto EndNameMatcher =
      IsReverse ? hasAnyName("rend", "crend") : hasAnyName("end", "cend");

  StatementMatcher BeginCallMatcher = anyOf(
      cxxMemberCallExpr(argumentCountIs(0),
                        callee(cxxMethodDecl(BeginNameMatcher)))
          .bind(BeginCallName),
      callExpr(callee(functionDecl(
                   IsReverse ? hasAnyName("::std::rbegin", "::std::crbegin")
                             : hasAnyName("::std::begin", "::std::cbegin"))))
          .bind(BeginCallName));

  DeclarationMatcher InitDeclMatcher =
      varDecl(hasInitializer(anyOf(ignoringParenImpCasts(BeginCallMatcher),
                                   materializeTemporaryExpr(
                                       ignoringParenImpCasts(BeginCallMatcher)),
                                   hasDescendant(BeginCallMatcher))))
          .bind(InitVarName);

  DeclarationMatcher EndDeclMatcher =
      varDecl(hasInitializer(anything())).bind(EndVarName);

  StatementMatcher EndCallMatcher = anyOf(
      cxxMemberCallExpr(argumentCountIs(0),
                        callee(cxxMethodDecl(EndNameMatcher))),
      callExpr(callee(
          functionDecl(IsReverse ? hasAnyName("::std::rend", "::std::crend")
                                 : hasAnyName("::std::end", "::std::cend")))));

  StatementMatcher IteratorBoundMatcher =
      expr(anyOf(ignoringParenImpCasts(
                     declRefExpr(to(varDecl(equalsBoundNode(EndVarName))))),
                 ignoringParenImpCasts(expr(EndCallMatcher).bind(EndCallName)),
                 materializeTemporaryExpr(ignoringParenImpCasts(
                     expr(EndCallMatcher).bind(EndCallName)))));

  StatementMatcher IteratorComparisonMatcher = expr(ignoringParenImpCasts(
      declRefExpr(to(varDecl(equalsBoundNode(InitVarName))))));

  return forStmt(
             unless(isInTemplateInstantiation()),
             hasLoopInit(anyOf(declStmt(declCountIs(2),
                                        containsDeclaration(0, InitDeclMatcher),
                                        containsDeclaration(1, EndDeclMatcher)),
                               declStmt(hasSingleDecl(InitDeclMatcher)))),
             hasCondition(ignoringImplicit(binaryOperation(
                 hasOperatorName("!="), hasOperands(IteratorComparisonMatcher,
                                                    IteratorBoundMatcher)))),
             hasIncrement(anyOf(
                 unaryOperator(hasOperatorName("++"),
                               hasUnaryOperand(declRefExpr(
                                   to(varDecl(equalsBoundNode(InitVarName)))))),
                 cxxOperatorCallExpr(
                     hasOverloadedOperatorName("++"),
                     hasArgument(0, declRefExpr(to(varDecl(
                                        equalsBoundNode(InitVarName)))))))),
             unless(hasBody(hasDescendant(incrementVarMatcher()))))
      .bind(IsReverse ? LoopNameReverseIterator : LoopNameIterator);
}

/// The matcher used for array-like containers (pseudoarrays).
///
/// This matcher is more flexible than array-based loops. It will match
/// loops of the following textual forms (regardless of whether the
/// iterator type is actually a pointer type or a class type):
///
/// \code
///   for (int i = 0, j = container.size(); i < j; ++i) { ... }
///   for (int i = 0; i < container.size(); ++i) { ... }
/// \endcode
/// The following string identifiers are bound to the parts of the AST:
///   InitVarName: 'i' (as a VarDecl)
///   LoopName: The entire for loop (as a ForStmt)
///   In the first example only:
///     EndVarName: 'j' (as a VarDecl)
///   In the second example only:
///     EndCallName: 'container.size()' (as a CXXMemberCallExpr)
///
/// Client code will need to make sure that:
///   - The containers on which 'size()' is called is the container indexed.
///   - The index variable is only used in overloaded operator[] or
///     container.at().
///   - The container's iterators would not be invalidated during the loop.
StatementMatcher makePseudoArrayLoopMatcher() {
  // Test that the incoming type has a record declaration that has methods
  // called 'begin' and 'end'. If the incoming type is const, then make sure
  // these methods are also marked const.
  //
  // FIXME: To be completely thorough this matcher should also ensure the
  // return type of begin/end is an iterator that dereferences to the same as
  // what operator[] or at() returns. Such a test isn't likely to fail except
  // for pathological cases.
  //
  // FIXME: Also, a record doesn't necessarily need begin() and end(). Free
  // functions called begin() and end() taking the container as an argument
  // are also allowed.
  TypeMatcher RecordWithBeginEnd = qualType(anyOf(
      qualType(
          isConstQualified(),
          hasUnqualifiedDesugaredType(recordType(hasDeclaration(cxxRecordDecl(
              hasMethod(cxxMethodDecl(hasName("begin"), isConst())),
              hasMethod(cxxMethodDecl(hasName("end"),
                                      isConst()))))    // hasDeclaration
                                                 ))),  // qualType
      qualType(unless(isConstQualified()),
               hasUnqualifiedDesugaredType(recordType(hasDeclaration(
                   cxxRecordDecl(hasMethod(hasName("begin")),
                                 hasMethod(hasName("end")))))))  // qualType
      ));

  StatementMatcher SizeCallMatcher = cxxMemberCallExpr(
      argumentCountIs(0), callee(cxxMethodDecl(hasAnyName("size", "length"))),
      on(anyOf(hasType(pointsTo(RecordWithBeginEnd)),
               hasType(RecordWithBeginEnd))));

  StatementMatcher EndInitMatcher =
      expr(anyOf(ignoringParenImpCasts(expr(SizeCallMatcher).bind(EndCallName)),
                 explicitCastExpr(hasSourceExpression(ignoringParenImpCasts(
                     expr(SizeCallMatcher).bind(EndCallName))))));

  DeclarationMatcher EndDeclMatcher =
      varDecl(hasInitializer(EndInitMatcher)).bind(EndVarName);

  StatementMatcher IndexBoundMatcher =
      expr(anyOf(ignoringParenImpCasts(
                     declRefExpr(to(varDecl(equalsBoundNode(EndVarName))))),
                 EndInitMatcher));

  return forStmt(unless(isInTemplateInstantiation()),
                 hasLoopInit(
                     anyOf(declStmt(declCountIs(2),
                                    containsDeclaration(0, initToZeroMatcher()),
                                    containsDeclaration(1, EndDeclMatcher)),
                           declStmt(hasSingleDecl(initToZeroMatcher())))),
                 hasCondition(arrayConditionMatcher(IndexBoundMatcher)),
                 hasIncrement(
                     unaryOperator(hasOperatorName("++"),
                                   hasUnaryOperand(incrementVarMatcher()))),
                 unless(hasBody(hasDescendant(incrementVarMatcher()))))
      .bind(LoopNamePseudoArray);
}

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(traverse(TK_AsIs, makeArrayLoopMatcher()), this);
  finder->addMatcher(traverse(TK_AsIs, makeIteratorLoopMatcher(false)), this);
  finder->addMatcher(traverse(TK_AsIs, makePseudoArrayLoopMatcher()), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  ASTContext* context = result.Context;
  const BoundNodes& Nodes = result.Nodes;
  const ForStmt* Loop;
  if ((Loop = Nodes.getNodeAs<ForStmt>(LoopNameArray))) {
  } else if ((Loop = Nodes.getNodeAs<ForStmt>(LoopNameIterator))) {
  } else if ((Loop = Nodes.getNodeAs<ForStmt>(LoopNameReverseIterator))) {
  } else {
    Loop = Nodes.getNodeAs<ForStmt>(LoopNamePseudoArray);
    if (!Loop) return;
  }
  std::string path =
      misra::libtooling_utils::GetFilename(Loop, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(Loop, result.SourceManager);
  ReportError(path, line_number, results_list_);
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A6_5_1
}  // namespace autosar
