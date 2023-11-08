/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_7_1_2/libtooling/checker.h"

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
      "如果对应的对象不被修改，那么函数中的指针或引用形参应被声明为指向const的指针或指向const的引用";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_7_1_2);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_1_2 {
namespace libtooling {

std::unordered_map<const clang::ParmVarDecl*, std::tuple<std::string, int>>
    not_const_parm_map{};
std::unordered_set<const clang::ParmVarDecl*> used_parm_map{};

// The idea of this checker is to find to sets:
// 1. the the parameters that are not const qualified.
// 2. the parameters that are modified in the function body, or escaped out of
// the func. Set 1 - Set 2 is the results we need.
//
// Set 1:
//  match A pointer or reference parameter in a function not be declared as
//  pointer to const or reference to const
//  exceptions:
//    1. not a definition: declaration has no body, so skip it to avoid useless
//      multi report
//    2. cxx virtual function: this is due to the exception of misra rules,
//      override function is ignored. The override function must be virtual:
//      In a member function declaration or definition, override specifier
//      ensures that the function is virtual and is overriding a virtual
//      function from a base class. The program is ill-formed (a compile-time
//      error is generated) if this is not true. (cppreference.com)
//      see good3 for examples.
//    3. implicit parameter: for example, default constructor and destructor
//      For example:
//      class A {
//        int a;
//      }
//      If we don't ignore implicit functions, the matcher will report on `class
//      A` for default constructor and destructor
//
// Set 2:
//  match the parameters that are modified in the function body, or escaped out
//  of the function. Set 1 - Set 2 is what we need.
//  This set has four cases:
//   1. match `*parm = 1` when parm is a pointer, or `parm = 1` when parm is a
//   reference (parm pointer as a left value)
//   2. match `*parm++` or `parm--`
//   3. match `func(parm)`, the pointer is escaped when calling another function
//   4. match `return parm;`, pass pointer as return value means the pointer is
//   escaped. See good2 for examples like this, p2 cannot be const qualified:
//      int32_t* f (int32_t * p2)
//      {
//        return p2;
//      }
//   5. match `p3->a = 1;` or `p4.b = 2;` when p3 and p4 are pointers to struct
//  We use these cases to find modified or escaped parameters, and then we can
//  find the parameters should be const qualified(not modified and not escaped).
class ReturnCallback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // match A pointer or reference parameter in a function not be declared as
    // pointer to const or reference to const
    auto param_bind = parmVarDecl(
        unless(isExpansionInSystemHeader()),
        unless(hasType(rValueReferenceType())),
        // only match definition
        hasAncestor(functionDecl(isDefinition())),
        unless(anyOf(hasAncestor(cxxMethodDecl((isVirtual()))),
                     // ignore implicit parameter in cxx: for example, implicit
                     // constructor and destructor
                     hasAncestor(cxxMethodDecl(isImplicit())))),
        anyOf(hasType(referenceType(pointee(unless(isConstQualified())))),
              hasType(pointerType(pointee(unless(isConstQualified()))))));

    // match declRefExpr to reference
    auto reference_param_bind = ignoringImpCasts(declRefExpr(to(
        parmVarDecl(hasType(referenceType(pointee(unless(isConstQualified())))))
            .bind("used_param"))));
    // match declRefExpr to pointer
    auto ref_to_param = ignoringImpCasts(declRefExpr(to(
        parmVarDecl(hasType(pointerType(pointee(unless(isConstQualified())))))
            .bind("used_param"))));
    // match the dereference of the pointer
    auto deref_param_pointer = ignoringImpCasts(
        unaryOperator(hasOperatorName("*"), hasUnaryOperand(ref_to_param)));
    // match `p3->a = 1;` or `p4.b = 2;`
    auto member_param_pointer =
        memberExpr(
            hasDescendant(declRefExpr(to(param_bind.bind("used_param")))))
            .bind("member_expr");
    // match `*parm` when parm is a pointer, or `parm` when parm is a reference
    auto lvalue_parm_decl =
        anyOf(deref_param_pointer, reference_param_bind, member_param_pointer);
    // match `*parm = 1` when parm is a pointer, or `parm = 1` when parm is a
    // reference. Also covers `+=` and other assignment operators.
    auto modified_equal = forEachDescendant(
        binaryOperation(isAssignmentOperator(), hasLHS(lvalue_parm_decl)));
    // match `*parm++` or `*parm--`
    auto modified_increase_decrease = forEachDescendant(
        unaryOperator(anyOf(hasOperatorName("++"), hasOperatorName("--")),
                      hasUnaryOperand(lvalue_parm_decl)));

    // the declRefExpr to pointer or reference parameter
    auto raw_parm_expr =
        ignoringImpCasts(declRefExpr(to(param_bind.bind("used_param"))));
    // match `return parm;`, pass pointer as return value means the pointer is
    // escaped and cannot be declared as pointer to const
    auto as_return_value =
        forEachDescendant(returnStmt(hasReturnValue(raw_parm_expr)));
    // match `func(parm)`, the pointer is
    // escaped and cannot be declared as pointer to const
    auto as_func_argument = forEachDescendant(
        callExpr(forEachArgumentWithParam(raw_parm_expr, anything())));
    // match parm which is modified by function
    finder->addMatcher(
        functionDecl(isDefinition(),
                     anyOf(modified_equal, modified_increase_decrease)),
        this);
    // match parm which is escaped in the function
    finder->addMatcher(
        functionDecl(isDefinition(), anyOf(as_return_value, as_func_argument)),
        this);
    // ignore cxx record decl
    finder->addMatcher(cxxRecordDecl().bind("used_param"), this);
    // match not const parm
    finder->addMatcher(param_bind.bind("not_const_func_parm"), this);
    finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource, member_param_pointer), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const ParmVarDecl* parm =
        result.Nodes.getNodeAs<ParmVarDecl>("not_const_func_parm");
    const ParmVarDecl* used_param =
        result.Nodes.getNodeAs<ParmVarDecl>("used_param");
    auto* memberExpr = result.Nodes.getNodeAs<MemberExpr>("member_expr");
    if (memberExpr && !misra::libtooling_utils::IsInSystemHeader(
                          memberExpr, result.Context)) {
      if (auto* method =
              llvm::dyn_cast<CXXMethodDecl>(memberExpr->getMemberDecl())) {
        // if a member method is const member function, but the param is not
        // const then the param should not in used_parm_map
        if (method->isConst()) {
          return;
        }
      }
    }
    if (parm) {
      if (misra::libtooling_utils::IsInSystemHeader(parm, result.Context)) {
        return;
      }
      std::string path_ =
          misra::libtooling_utils::GetFilename(parm, result.SourceManager);
      int line_number_ =
          misra::libtooling_utils::GetLine(parm, result.SourceManager);
      not_const_parm_map.insert({parm, std::make_tuple(path_, line_number_)});
    }
    if (used_param) {
      used_parm_map.insert(used_param);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::checkFuncParmMapAndReport(void) {
  for (auto parm : used_parm_map) {
    not_const_parm_map.erase(parm);
  }
  for (auto value : not_const_parm_map) {
    ReportError(std::get<0>(value.second), std::get<1>(value.second),
                results_list_);
  }
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  returnCallback_ = new ReturnCallback;
  returnCallback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_7_1_2
}  // namespace misra_cpp_2008
