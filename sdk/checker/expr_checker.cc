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

#include "sdk/checker/expr_checker.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <glog/logging.h>

#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"

using analyzer::proto::ResultsList;
using clang::ast_matchers::MatchFinder;
using clang::ast_matchers::StatementMatcher;
using std::string;

namespace {

class Callback : public sdk::checker::ASTCheckerCallback {
 public:
  Callback(string message, StatementMatcher matcher, string bind_id)
      : message_(message), matcher_(matcher), bind_id_(bind_id) {}

  void Init(ResultsList* results_list, MatchFinder* finder) override {
    results_list_ = results_list;
    finder->addMatcher(matcher_, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const clang::Expr* expr = result.Nodes.getNodeAs<clang::Expr>(bind_id_);
    string path =
        misra::libtooling_utils::GetFilename(expr, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(expr, result.SourceManager);
    misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                              message_);
  }

 private:
  string message_;
  StatementMatcher matcher_;
  string bind_id_;
  ResultsList* results_list_;
};

}  // namespace

namespace sdk {
namespace checker {

void ExprChecker::Init(string name, string message, StatementMatcher matcher,
                       string bind_id) {
  callback_ = new Callback(message, matcher, bind_id);
  ASTChecker::Init(name, callback_);
}

}  // namespace checker
}  // namespace sdk
