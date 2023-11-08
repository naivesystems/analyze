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

#include "autosar/rule_A15_1_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <queue>

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
      "Only instances of types derived from std::exception should be thrown.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A15_1_1 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(cxxThrowExpr(has(cxxConstructExpr().bind("construct")),
                                    unless(isExpansionInSystemHeader()))
                           .bind("expr"),
                       this);

    finder->addMatcher(cxxThrowExpr(unless(has(cxxConstructExpr())),
                                    unless(isExpansionInSystemHeader()))
                           .bind("expr"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXThrowExpr* expr = result.Nodes.getNodeAs<CXXThrowExpr>("expr");
    const CXXConstructExpr* construct =
        result.Nodes.getNodeAs<CXXConstructExpr>("construct");
    // if CXXThrowExpr does not throw an object (e.g., throw a integer literal),
    // report error and return
    if (!construct) {
      std::string path =
          misra::libtooling_utils::GetFilename(expr, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(expr, result.SourceManager);
      ReportError(path, line_number, results_list_);
      return;
    }
    // check all direct and indirect base classes to see if the object thrown
    // by CXXThrowExpr inherits from std::exception
    const CXXRecordDecl* constructorParent =
        construct->getConstructor()->getParent();
    queue<const CXXRecordDecl*> baseQueue;
    baseQueue.push(constructorParent);
    while (!baseQueue.empty()) {
      const CXXRecordDecl* RD = baseQueue.front();
      baseQueue.pop();
      for (auto base : RD->bases()) {
        const CXXRecordDecl* baseRD = base.getType()->getAsCXXRecordDecl();
        baseQueue.push(baseRD);
        if (misra::libtooling_utils::IsInSystemHeader(baseRD, result.Context) &&
            baseRD->getName().equals("exception")) {
          return;
        }
      }
    }
    std::string path =
        misra::libtooling_utils::GetFilename(expr, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(expr, result.SourceManager);
    ReportError(path, line_number, results_list_);
    return;
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
}  // namespace rule_A15_1_1
}  // namespace autosar
