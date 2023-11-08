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

#include "autosar/rule_A7_5_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
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
      "A function shall not return a reference or a pointer to a parameter that is passed by reference to const.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A7_5_1 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()),
                     hasReturnTypeLoc(referenceTypeLoc()),
                     hasDescendant(returnStmt(has(ignoringParens(
                                                  (declRefExpr().bind("ref")))))
                                       .bind("stmt")))
            .bind("decl"),
        this);

    finder->addMatcher(
        functionDecl(
            unless(isExpansionInSystemHeader()),
            hasReturnTypeLoc(pointerTypeLoc()),
            hasDescendant(
                returnStmt(has(ignoringParens(unaryOperator(has(
                               ignoringParens(declRefExpr().bind("ref")))))))
                    .bind("stmt")))
            .bind("decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* decl = result.Nodes.getNodeAs<FunctionDecl>("decl");
    const DeclRefExpr* ref = result.Nodes.getNodeAs<DeclRefExpr>("ref");
    const Stmt* stmt = result.Nodes.getNodeAs<Stmt>("stmt");

    if (isa<ParmVarDecl>(ref->getDecl())) {
      const QualType parmType = cast<ParmVarDecl>(ref->getDecl())->getType();
      if (parmType->isReferenceType() &&
          parmType.getNonReferenceType().isConstQualified()) {
        std::string path =
            misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(stmt, result.SourceManager);
        ReportError(path, line_number, results_list_);
        return;
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
}  // namespace rule_A7_5_1
}  // namespace autosar
