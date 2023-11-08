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

#ifndef ANALYZER_misra_cpp_2008_LIBTOOLING_RULE_8_4_4_CHECKER_H_
#define ANALYZER_misra_cpp_2008_LIBTOOLING_RULE_8_4_4_CHECKER_H_

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using MatchFinder = clang::ast_matchers::MatchFinder;

namespace misra_cpp_2008 {
namespace rule_8_4_4 {
namespace libtooling {

class FuncIdentifierCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  MatchFinder finder_;
  FuncIdentifierCallback* callback_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_8_4_4
}  // namespace misra_cpp_2008

#endif  // misra_cpp_2008_LIBTOOLING_RULE_8_4_4_CHECKER_H_
