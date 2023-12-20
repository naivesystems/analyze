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

#ifndef ANALYZER_SDK_CHECKER_AST_CHECKER_H_
#define ANALYZER_SDK_CHECKER_AST_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <string>

#include "analyzer/proto/results.pb.h"
#include "sdk/checker/checker.h"

namespace sdk {
namespace checker {

using analyzer::proto::ResultsList;
using clang::ast_matchers::MatchFinder;

class ASTCheckerCallback : public MatchFinder::MatchCallback {
 public:
  virtual ~ASTCheckerCallback() {}

  virtual void Init(ResultsList* results_list, MatchFinder* finder) = 0;

  virtual void run(const MatchFinder::MatchResult& Result) = 0;
};

class ASTChecker : public Checker {
 public:
  virtual ~ASTChecker() {}

  void Init(std::string name, ASTCheckerCallback* callback) {
    name_ = name;
    callback_ = callback;
  }

  int Run(int argc, char** argv) override;

 private:
  ResultsList results_list_;
  MatchFinder finder_;
  std::string name_;
  ASTCheckerCallback* callback_;
};

}  // namespace checker
}  // namespace sdk

#endif  // ANALYZER_SDK_CHECKER_AST_CHECKER_H_
