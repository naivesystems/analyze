/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_7_5_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "不得将自动存储对象的地址赋给在该对象不复存在后仍然可能存在的另一个对象";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_5_2);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_5_2 {
namespace libtooling {

// Note: Some cases are still open, for example:
// 1) Whether the target object is persist after the source object is
// implement-defined, i.e., whether this rule is violated is determined by the
// implementing behavior. 2) More complex reference to the source object. More
// details see cppcheck/test/testautovariables:invalidLifetime and deadPointer.
// 3) The assigning object passes the address of an object with shorter storage
// to other objects that persist after the first object is ceased to exist.

class AssignOpCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperation(
            isAssignmentOperator(),
            hasLHS(declRefExpr(to(varDecl().bind("target")))),
            hasRHS(unaryOperator(
                hasOperatorName("&"),
                hasUnaryOperand(declRefExpr(to(
                    varDecl(hasAutomaticStorageDuration()).bind("source")))))))
            .bind("assign_op"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const BinaryOperator* assign_op_ =
        result.Nodes.getNodeAs<BinaryOperator>("assign_op");
    std::string path_ =
        misra::libtooling_utils::GetFilename(assign_op_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(assign_op_, result.SourceManager);
    const VarDecl* target_ = result.Nodes.getNodeAs<VarDecl>("target");
    const VarDecl* source_ = result.Nodes.getNodeAs<VarDecl>("source");
    if (!misra::libtooling_utils::IsInSystemHeader(assign_op_,
                                                   result.Context)) {
      if (!target_->hasLocalStorage()) {
        ReportError(path_, line_number_, results_list_);
        return;
      } else {
        // get the declStmt of the source and target varDecl
        const auto& target_parents = result.Context->getParents(*target_);
        const auto& source_parents = result.Context->getParents(*source_);
        // get the innermost compoundStmt of the declStmt
        const CompoundStmt* target_compound =
            result.Context->getParents(target_parents[0])[0]
                .get<CompoundStmt>();
        const CompoundStmt* source_compound =
            result.Context->getParents(source_parents[0])[0]
                .get<CompoundStmt>();
        if (target_compound == nullptr || source_compound == nullptr) {
          return;
        }
        if ((target_compound != source_compound) &&
            (target_compound->getSourceRange().fullyContains(
                source_compound->getSourceRange()))) {
          ReportError(path_, line_number_, results_list_);
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

// To handle cases where the address of an object with local storage is returned
// and assigned to another object as a function return value.
class FuncAssignCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperation(
            isAssignmentOperator(), hasLHS(declRefExpr(to(varDecl()))),
            hasRHS(callExpr(
                callee(functionDecl(forEachDescendant(returnStmt(hasDescendant(
                    unaryOperator(hasOperatorName("&"),
                                  hasUnaryOperand(declRefExpr(to(varDecl(
                                      hasAutomaticStorageDuration())))))))))))))
            .bind("assign_op"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const BinaryOperator* assign_op_ =
        result.Nodes.getNodeAs<BinaryOperator>("assign_op");
    std::string path_ =
        misra::libtooling_utils::GetFilename(assign_op_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(assign_op_, result.SourceManager);
    if (!misra::libtooling_utils::IsInSystemHeader(assign_op_,
                                                   result.Context)) {
      ReportError(path_, line_number_, results_list_);
    }
    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

// To handle 'throw' cases mentioned in Rationale of Rule 7-5-2
class ThrowCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxThrowExpr(has(unaryOperator(hasOperatorName("&"),
                                       hasUnaryOperand(declRefExpr(to(varDecl(
                                           hasAutomaticStorageDuration())))))))
            .bind("throw"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXThrowExpr* throw_ = result.Nodes.getNodeAs<CXXThrowExpr>("throw");
    std::string path_ =
        misra::libtooling_utils::GetFilename(throw_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(throw_, result.SourceManager);
    if (!misra::libtooling_utils::IsInSystemHeader(throw_, result.Context)) {
      ReportError(path_, line_number_, results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  assignOpCallback_ = new AssignOpCallback;
  assignOpCallback_->Init(results_list_, &finder_);
  funcAssignCallback_ = new FuncAssignCallback;
  funcAssignCallback_->Init(results_list_, &finder_);
  throwCallback_ = new ThrowCallback;
  throwCallback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_7_5_2
}  // namespace misra_cpp_2008
