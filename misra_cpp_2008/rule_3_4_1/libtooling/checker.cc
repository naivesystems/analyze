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

#include "misra_cpp_2008/rule_3_4_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace misra_cpp_2008 {
namespace rule_3_4_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    // Match variable declaration, only referenced in compound stmts
    finder->addMatcher(
        compoundStmt(
            has(declStmt(containsDeclaration(0, varDecl().bind("vd")))),
            unless(has(stmt(unless(compoundStmt()),
                            hasDescendant(declRefExpr(
                                hasDeclaration(equalsBoundNode("vd")))))))),
        this);
    // Match variable declaration, only referenced in for stmt body
    finder->addMatcher(
        compoundStmt(
            has(declStmt(containsDeclaration(0, varDecl().bind("vd")))),
            unless(has(forStmt(unless(has(compoundStmt(hasDescendant(
                declRefExpr(hasDeclaration(equalsBoundNode("vd")))))))))),
            unless(has(stmt(unless(forStmt()),
                            hasDescendant(declRefExpr(
                                hasDeclaration(equalsBoundNode("vd")))))))),
        this);
    // Match global variable declaration, only referenced in one function
    finder->addMatcher(
        translationUnitDecl(
            has(varDecl().bind("vd")),
            has(functionDecl(hasBody(hasDescendant(declRefExpr(
                                 hasDeclaration(equalsBoundNode("vd"))))))
                    .bind("func1")),
            unless(
                has(functionDecl(hasBody(hasDescendant(declRefExpr(
                                     hasDeclaration(equalsBoundNode("vd"))))),
                                 unless(equalsBoundNode("func1")))))),
        this);
    // TODO: add more matchers
  }

  void run(const MatchFinder::MatchResult& result) {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    // If variables are only declared, we skip them.
    if (!vd->isUsed()) return;

    if (misra::libtooling_utils::IsInSystemHeader(vd, result.Context)) {
      return;
    }
    string error_message =
        "声明为对象或类型的标识符应在一个使其可见性最低的代码块中定义";
    string path =
        misra::libtooling_utils::GetFilename(vd, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(vd, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_3_4_1);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_3_4_1
}  // namespace misra_cpp_2008
