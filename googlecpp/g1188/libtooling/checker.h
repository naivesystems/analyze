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

#ifndef ANALYZER_GOOGLECPP_G1188_LIBTOOLING_CHECKER_H_
#define ANALYZER_GOOGLECPP_G1188_LIBTOOLING_CHECKER_H_

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using namespace clang;

// Callback matches functions, assignment operations, and ++, -- operator.
// When a new function is matched, all pointer && reference function params are
// recorded to func_info_2_param_infos, and assuming all the params are not
// output params. When a assignment operation/++/-- is matched, and the lhs is
// one of recorded function params, mark it as output param in
// func_info_2_param_infos. After Callback finished, check the order of
// parameters.

namespace googlecpp {
namespace g1188 {
namespace libtooling {

class Callback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  Callback* callback_;
  ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace g1188
}  // namespace googlecpp

#endif  // ANALYZER_GOOGLECPP_G1188_LIBTOOLING_CHECKER_H_
