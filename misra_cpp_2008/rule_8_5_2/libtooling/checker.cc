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

#include "misra_cpp_2008/rule_8_5_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "在数组和结构的非零初始化中，应使用大括号来指示和匹配结构";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_8_5_2 {
namespace libtooling {

class ListInitCallback : public MatchFinder::MatchCallback {
  enum ZeroInitializationType {
    NON_ZERO = 0,
    EMPTY_INIT,
    SINGLE_ZERO_INIT,
  };
  ZeroInitializationType isZeroInitialization(const InitListExpr* init_list) {
    if (0 == init_list->getNumInits()) {
      return ZeroInitializationType::EMPTY_INIT;
    }
    bool zero_appeared = false;
    for (auto expr : *init_list) {
      if (auto cast_expr = llvm::dyn_cast_or_null<ImplicitCastExpr>(expr)) {
        expr = cast_expr->getSubExpr();
      }
      if (auto val = llvm::dyn_cast_or_null<IntegerLiteral>(expr)) {
        if (0 == val->getValue()) {
          if (zero_appeared) {
            return ZeroInitializationType::NON_ZERO;
          }
          zero_appeared = true;
        } else {
          return ZeroInitializationType::NON_ZERO;
        }
      } else if (!llvm::dyn_cast_or_null<ImplicitValueInitExpr>(expr)) {
        // an sub-init causes it's parent-init non-zero if it is not:
        // (1) an integer literal
        // (2) an implicit-value init
        //     'int a[10][10][10]{};' gets AST like this:
        //      VarDecl a 'int[10][10][10]' listinit
        //      `-InitListExpr 'int[10][10][10]'
        //        `-array_filler: ImplicitValueInitExpr  'int[10][10]'
        //     'int a[10][10][10];' gets AST like this:
        //      VarDecl a 'int[10][10][10]' listinit
        //      `-InitListExpr 'int[10][10][10]'
        //        |-array_filler: ImplicitValueInitExpr  'int[10][10]'
        //        `-InitListExpr 'int[10][10]'
        //          |-array_filler: ImplicitValueInitExpr  'int[10]'
        //          `-InitListExpr 'int[10]'
        //            |-array_filler: ImplicitValueInitExpr  'int'
        //            `-IntegerLiteral 'int' 0
        auto sub_init_list = llvm::dyn_cast_or_null<InitListExpr>(expr);
        if (sub_init_list &&
            sub_init_list->getBeginLoc() == sub_init_list->getEndLoc()) {
          // check sub-init-list without braces
          // which means it's generated by semantic analysis
          if (NON_ZERO == isZeroInitialization(sub_init_list)) {
            return ZeroInitializationType::NON_ZERO;
          }
          continue;
        }
        return ZeroInitializationType::NON_ZERO;
      }
    }

    return zero_appeared ? ZeroInitializationType::SINGLE_ZERO_INIT
                         : ZeroInitializationType::EMPTY_INIT;
  }
  bool isMatchableToDeclStructure(const InitListExpr* init_list) {
    if (ZeroInitializationType::NON_ZERO != isZeroInitialization(init_list)) {
      return true;
    }
    if (init_list->hasArrayFiller()) {
      return false;
    }
    for (auto expr : *init_list) {
      auto sub_init_list = llvm::dyn_cast_or_null<InitListExpr>(expr);
      if (!sub_init_list) {
        continue;
      }
      auto sub_init_list_type = isZeroInitialization(sub_init_list);
      switch (sub_init_list_type) {
        case ZeroInitializationType::EMPTY_INIT: {
          return false;
        }
        case ZeroInitializationType::SINGLE_ZERO_INIT: {
          if (sub_init_list->hasArrayFiller()) {
            return false;
          }
        }
        case ZeroInitializationType::NON_ZERO: {
          break;
        }
      }
      auto first_init = sub_init_list->getInit(0);
      if (first_init->getBeginLoc() == sub_init_list->getLBraceLoc()) {
        return false;
      }
      if (!isMatchableToDeclStructure(sub_init_list)) {
        return false;
      }
    }
    return true;
  }

 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(initListExpr(unless(hasAncestor(initListExpr())))
                           .bind("top_level_init_list"),
                       this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    auto top_level_init_list =
        result.Nodes.getNodeAs<InitListExpr>("top_level_init_list");
    if (!isMatchableToDeclStructure(top_level_init_list)) {
      ReportError(misra::libtooling_utils::GetFilename(top_level_init_list,
                                                       result.SourceManager),
                  misra::libtooling_utils::GetLine(top_level_init_list,
                                                   result.SourceManager),
                  results_list_);
    }
    return;
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new ListInitCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_8_5_2
}  // namespace misra_cpp_2008
