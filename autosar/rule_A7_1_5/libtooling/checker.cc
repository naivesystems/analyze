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

#include "autosar/rule_A7_1_5/libtooling/checker.h"

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
      "The auto specifier shall not be used apart from following cases: (1) to declare that a variable has the same type as return type of a function call, (2) to declare that a variable has the same type as initializer of non-fundamental type, (3) to declare parameters of a generic lambda expression, (4) to declare a function template using trailing return type syntax.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A7_1_5 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(varDecl(unless(isExpansionInSystemHeader()),
                               hasInitializer(expr().bind("init")),
                               unless(hasInitializer(callExpr())))
                           .bind("vd"),
                       this);
    finder->addMatcher(
        parmVarDecl(hasType(autoType()), unless(isExpansionInSystemHeader()),
                    unless(hasParent(lambdaExpr())))
            .bind("pd"),
        this);  // auto type parameters cannot be compiled by clang, so this is
                // actually not nessecery
    finder->addMatcher(functionDecl(unless(isExpansionInSystemHeader()),
                                    unless(hasTrailingReturn()))
                           .bind("fd"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("vd");
    const FunctionDecl* function_decl =
        result.Nodes.getNodeAs<FunctionDecl>("fd");
    if (var_decl || function_decl) {
      const Type* t;
      if (var_decl) {
        t = var_decl->getType().getTypePtr();
        const Expr* init = result.Nodes.getNodeAs<Expr>("init");
        if (!init || (!init->getType().isNull() &&
                      !init->getType()->isFundamentalType())) {
          return;
        }
      } else {
        t = function_decl->getDeclaredReturnType().getTypePtr();
      }

      if (t->getContainedAutoType()) {
        if (var_decl) {
          ReportError(
              misra::libtooling_utils::GetFilename(var_decl,
                                                   result.SourceManager),
              misra::libtooling_utils::GetLine(var_decl, result.SourceManager),
              results_list_);
        } else {
          ReportError(misra::libtooling_utils::GetFilename(
                          function_decl, result.SourceManager),
                      misra::libtooling_utils::GetLine(function_decl,
                                                       result.SourceManager),
                      results_list_);
        }
        return;
      }
    }

    const ParmVarDecl* parm_decl = result.Nodes.getNodeAs<ParmVarDecl>("pd");
    if (parm_decl) {
      ReportError(
          misra::libtooling_utils::GetFilename(parm_decl, result.SourceManager),
          misra::libtooling_utils::GetLine(parm_decl, result.SourceManager),
          results_list_);
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
}  // namespace rule_A7_1_5
}  // namespace autosar
