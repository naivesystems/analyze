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

#ifndef ANALYZER_GOOGLECPP_G1181_LIBTOOLING_CHECKER_H_
#define ANALYZER_GOOGLECPP_G1181_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

namespace googlecpp {
namespace g1181 {
namespace libtooling {

class Callback : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::ast_matchers::MatchFinder* finder);

  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override;

 private:
  analyzer::proto::ResultsList* results_list_;
};

class ASTChecker {
 public:
  void Init(analyzer::proto::ResultsList* results_list) {
    results_list_ = results_list;
    callback_ = new Callback;
    callback_->Init(results_list_, &finder_);
  }
  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  Callback* callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace g1181
}  // namespace googlecpp

#endif
