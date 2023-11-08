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

#include "misra_cpp_2008/rule_12_1_1/libtooling/checker.h"

#include <glog/logging.h>

#include <queue>

#include "absl/strings/str_format.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Lex/Lexer.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_12_1_1 {
namespace libtooling {

/**
 * An object's dynamic type shall not be used from the body of it's contructor
 * and destructor This includes the 3 cases:
 *
 * 1. typeid on a class with a virtual function or a virtual function in the
 * base class. We first match the typeid and check for virtual functions in
 * 'UseTypeIdOnClassWithVirtualFunction'
 *
 * 2. dynamic cast. Dynamic cast in contructor/destructor is matched directly in
 * the AST and checked if its parameter is 'this'.
 *
 * 3. a virtual call to a virtual function. All the call expression in the
 * constructor/destructor are matched and then checked they are purely virtual
 * in 'VirtualCallToVirtualFunction'.
 */
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        decl(anyOf(cxxConstructorDecl(
                       hasBody(compoundStmt()),
                       anyOf(hasDescendant(cxxDynamicCastExpr().bind("dcast")),
                             hasDescendant(callExpr().bind("callexpr")),
                             hasDescendant(
                                 naivesystemsCxxTypeidExpr().bind("typeid")))),
                   cxxDestructorDecl(
                       hasBody(compoundStmt()),
                       anyOf(hasDescendant(cxxDynamicCastExpr().bind("dcast")),
                             hasDescendant(callExpr().bind("callexpr")),
                             hasDescendant(naivesystemsCxxTypeidExpr().bind(
                                 "typeid")))))),
        this);
  }

  // check if there is typeid on a class with a virtual function
  // or a virtual function in the base class
  bool hasVirtualFunction(const CXXRecordDecl* decl) {
    for (auto it = decl->method_begin(); it != decl->method_end(); ++it) {
      if (it->isVirtual()) {
        return true;
      }
    }
    return false;
  }

  // check for typeid on a class with a virtual function or a virtual function
  // in the base class return true if such typeid exist, otherwise false
  bool UseTypeIdOnClassWithVirtualFunction(const CXXTypeidExpr* typeid_expr) {
    bool has_virtual = false;

    TypeSourceInfo* type_info = typeid_expr->getTypeOperandSourceInfo();
    // convert it to class declaration
    auto type = type_info->getType().getTypePtr();
    const CXXRecordDecl* record_decl = type->getAsCXXRecordDecl();

    // check if the class has virtual function
    if (hasVirtualFunction(record_decl)) {
      has_virtual = true;
    }

    // check if the base class of the class has virtual function
    auto base = record_decl->bases_begin();
    for (; base != record_decl->bases_end(); ++base) {
      auto base_decl = base->getType()->getAsCXXRecordDecl();
      if (hasVirtualFunction(base_decl)) {
        has_virtual = true;
        break;
      }
    }

    // report error when virtual function is detected
    return has_virtual;
  }

  // check if the contructor/destructor contains a virtual call to a virtual
  // function, return false if such call does not exist
  bool VirtualCallToVirtualFunction(
      const ast_matchers::MatchFinder::MatchResult& result,
      const CallExpr* callexpr) {
    // get the function declaration
    const FunctionDecl* func_decl = callexpr->getDirectCallee();
    if (func_decl == nullptr) {
      return false;
    }

    // check if the function is virtual
    if (!(func_decl->isVirtualAsWritten())) {
      return false;
    }

    // check if the function is a virtual or nonvirtual call to a virtual
    // function obj.some_virtual_function(); (virtual call)
    // robj.SomeObject::some_virtual_function(); (direct call)
    // pobj->some_virtual_function(); (virtual call)
    // pobj->SomeObject::some_virtual_function(); (direct call)
    // robj.some_virtual_function(); (virtual call)
    // robj.SomeObject::some_virtual_function(); (direct call)
    SourceLocation start_loc = callexpr->getBeginLoc();
    SourceLocation end_loc = callexpr->getEndLoc();
    SourceManager& source_manager = result.Context->getSourceManager();
    auto func_name =
        Lexer::getSourceText(CharSourceRange::getTokenRange(start_loc, end_loc),
                             source_manager, result.Context->getLangOpts());
    if (func_name.str().find("::") != std::string::npos) {
      return false;
    }
    return true;
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXDynamicCastExpr* dcast =
        result.Nodes.getNodeAs<CXXDynamicCastExpr>("dcast");
    const CallExpr* callexpr = result.Nodes.getNodeAs<CallExpr>("callexpr");
    const CXXTypeidExpr* tId = result.Nodes.getNodeAs<CXXTypeidExpr>("typeid");

    string path = " ";
    int line = -1;

    // check for typeid
    if (tId != nullptr) {
      if (misra::libtooling_utils::IsInSystemHeader(tId, result.Context)) {
        return;
      }
      if (!UseTypeIdOnClassWithVirtualFunction(tId)) {
        return;
      }

      path = misra::libtooling_utils::GetFilename(tId, result.SourceManager);
      line = misra::libtooling_utils::GetLine(tId, result.SourceManager);

    } else if (dcast != nullptr) {
      // check for dynamic cast
      if (misra::libtooling_utils::IsInSystemHeader(dcast, result.Context)) {
        return;
      }
      // get the parameter of the dynamic_cast and check if it is this
      // report error only when the parameter is 'this'
      auto param = dcast->getSubExpr();
      if (param->getStmtClass() != Stmt::StmtClass::CXXThisExprClass) {
        return;
      }
      path = misra::libtooling_utils::GetFilename(dcast, result.SourceManager);
      line = misra::libtooling_utils::GetLine(dcast, result.SourceManager);

    } else if (callexpr != nullptr) {
      // check for call expression
      if (misra::libtooling_utils::IsInSystemHeader(callexpr, result.Context)) {
        return;
      }

      if (!VirtualCallToVirtualFunction(result, callexpr)) {
        return;
      }
      path =
          misra::libtooling_utils::GetFilename(callexpr, result.SourceManager);
      line = misra::libtooling_utils::GetLine(callexpr, result.SourceManager);
    }

    string error_message =
        "对象的动态类型不得从其构造函数或析构函数的主体中使用";
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_12_1_1);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_12_1_1
}  // namespace misra_cpp_2008
