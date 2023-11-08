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

#include "autosar/rule_A8_4_11/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A smart pointer shall only be used as a parameter type if it expresses lifetime semantics.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A8_4_11 {
namespace libtooling {
// 分别匹配类型为shared_ptr和unique_ptr的参数
// 然后得到它所在的函数定义节点，遍历整个函数里的节点
// 判断shared_ptr是否被拷贝，unique_ptr是否被作为move的参数
class Callback : public MatchFinder::MatchCallback {
 public:
  // 判断此次是否调用move
  bool CheckIsMove(const CallExpr* callExpr) {
    if (auto callee = callExpr->getDirectCallee()) {
      if (callee->getQualifiedNameAsString() == "std::move") {
        return true;
      }
    }
    return false;
  }

  // 递归检查函数的所有节点，判断参数是否作为move的参数
  bool CheckIsMoveParm(const Stmt* stmt, const ParmVarDecl* parm) {
    if (!stmt) {
      return false;
    }
    for (auto child_node : stmt->children()) {
      if (CheckIsMoveParm(child_node, parm)) {
        return true;
        // 匹配 unique_ptr p = std::move(..)
      } else if (auto op_call_expr =
                     dyn_cast<CXXOperatorCallExpr>(child_node)) {
        if (op_call_expr->getOperator() == clang::OO_Equal) {
          auto l_value = op_call_expr->getArg(0);
          auto r_value = op_call_expr->getArg(1);
          if (auto call_expr = dyn_cast<CallExpr>(r_value)) {
            if (CheckIsMove(call_expr)) {
              if (auto decl = dyn_cast<DeclRefExpr>(l_value)) {
                return decl->getDecl() == parm;
              }
            }
          }
        }
        //匹配 std::movd(p)
      } else if (auto call_expr = dyn_cast<CallExpr>(child_node)) {
        if (CheckIsMove(call_expr)) {
          for (int i = 0; i < call_expr->getNumArgs(); ++i) {
            auto arg = call_expr->getArg(i);
            if (auto decl = dyn_cast<DeclRefExpr>(arg->IgnoreImplicit())) {
              return parm == decl->getDecl();
            }
          }
        }
      }
    }
    return false;
  }

  // 递归检查所有节点，判断参数是否被拷贝
  bool CheckIsCopy(const Stmt* stmt, const ParmVarDecl* parm) {
    if (!stmt) {
      return false;
    }
    for (auto child_node : stmt->children()) {
      if (CheckIsCopy(child_node, parm)) {
        return true;
        // 检查是否调用了shared_ptr的构造函数,发生拷贝
      } else if (auto ctor_expr = dyn_cast<CXXConstructExpr>(child_node)) {
        for (int i = 0; i < ctor_expr->getNumArgs(); ++i) {
          auto arg = ctor_expr->getArg(i);
          if (auto decl = dyn_cast<DeclRefExpr>(arg->IgnoreImplicit())) {
            return decl->getDecl() == parm;
          }
        }
      }
    }
    return false;
  }

  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(parmVarDecl(hasType(classTemplateSpecializationDecl(
                                       hasName("std::unique_ptr"))))
                           .bind("unique_ptr"),
                       this);
    finder->addMatcher(parmVarDecl(hasType(classTemplateSpecializationDecl(
                                       hasName("std::shared_ptr"))))
                           .bind("shared_ptr"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const ParmVarDecl* unique_ptr =
        result.Nodes.getNodeAs<ParmVarDecl>("unique_ptr");

    if (unique_ptr) {
      const SourceLocation loc = unique_ptr->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        // 获取参数所在的函数定义节点
        if (auto func_decl =
                dyn_cast<FunctionDecl>(unique_ptr->getLexicalDeclContext())) {
          //判断unique_ptr是否在函数中被用作move的参数
          if (CheckIsMoveParm(func_decl->getBody(), unique_ptr)) {
            return;
          }

          ReportError(
              misra::libtooling_utils::GetFilename(func_decl,
                                                   result.SourceManager),
              misra::libtooling_utils::GetLine(func_decl, result.SourceManager),
              results_list_);
        }
      }
    }

    const ParmVarDecl* shared_ptr =
        result.Nodes.getNodeAs<ParmVarDecl>("shared_ptr");
    if (shared_ptr) {
      const SourceLocation loc = shared_ptr->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        if (auto func_decl =
                dyn_cast<FunctionDecl>(shared_ptr->getLexicalDeclContext())) {
          //获取参数所在的函数定义节点
          if (CheckIsCopy(func_decl->getBody(), shared_ptr)) {
            return;
          }
          //判断shared_ptr是否在函数中被拷贝
          ReportError(
              misra::libtooling_utils::GetFilename(func_decl,
                                                   result.SourceManager),
              misra::libtooling_utils::GetLine(func_decl, result.SourceManager),
              results_list_);
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A8_4_11
}  // namespace autosar
