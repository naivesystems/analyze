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

#ifndef ANALYZER_MISRA_CPP_2008_RULE_0_1_4_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_0_1_4_LIBTOOLING_CHECKER_H_

#include <string.h>

#include <unordered_map>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using namespace clang;

namespace misra_cpp_2008 {
namespace rule_0_1_4 {
namespace libtooling {

using MatchFinder = clang::ast_matchers::MatchFinder;

class Checker : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  MatchFinder* GetMatchFinder() { return &finder_; }

  void run(const MatchFinder::MatchResult& result) override;

  void checkVarDecl(VarDecl* varDecl, FullSourceLoc location,
                    SourceManager* sourceManager);

  void checkDeclRef(DeclRefExpr* declRef);

  void reportInValidVarDecl();

 private:
  MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<VarDecl*, int> varDecl_reference_count_;
  std::unordered_map<VarDecl*, std::pair<std::string, int>> varDecl_record_;
};

}  // namespace libtooling
}  // namespace rule_0_1_4
}  // namespace misra_cpp_2008

#endif  // ANALYZER_MISRA_CPP_2008_RULE_0_1_4_LIBTOOLING_CHECKER_H_
