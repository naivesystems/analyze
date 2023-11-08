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

#ifndef ANALYZER_AUTOSAR_A7_2_3_LIBTOOLING_CHECKER_H_
#define ANALYZER_AUTOSAR_A7_2_3_LIBTOOLING_CHECKER_H_

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using namespace clang;
using MatchFinder = clang::ast_matchers::MatchFinder;

using analyzer::proto::ResultsList;

namespace autosar {
namespace rule_A7_2_3 {
namespace libtooling {
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder);
  void run(const MatchFinder::MatchResult& result);

 private:
  ResultsList* results_list_;
};

class Checker {
 public:
  void Init(ResultsList* results_list);
  MatchFinder* GetMatchFinder();

 private:
  Callback* callback_;
  MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace rule_A7_2_3
}  // namespace autosar

#endif
