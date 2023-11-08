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

#ifndef ANALYZER_GOOGLECPP_G1177_LIBTOOLING_LIB_H_
#define ANALYZER_GOOGLECPP_G1177_LIBTOOLING_LIB_H_

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using namespace clang;

namespace googlecpp {
namespace g1177 {
namespace libtooling {
class CastCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  CastCallback* callback_;
  ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace g1177
}  // namespace googlecpp

#endif  // ANALYZER_GOOGLECPP_G1177_LIBTOOLING_LIB_H_
