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

#ifndef ANALYZER_MISRA_RULE_5_1_CHECKER_H_
#define ANALYZER_MISRA_RULE_5_1_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <unordered_map>

#include "misra/proto_util.h"

namespace misra {
namespace rule_5_1 {

class ExternalVDCallback;
class ExternalFDCallback;

struct identifier {
  std::string name;
  std::string loc;
  bool is_implicit;
};

/**
 * From [misra-c2012-5.1]:
 *
 * The definition of distinct depends on the implementation and on the version
 * of the C language that is being used:
 *
 * In C90 the minimum requirement is that the first 6 characters of external
 * identifiers are significant but their case is not required to be
 * significant;
 *
 * In C99 the minimum requirement is that the first 31 characters of external
 * identifiers are significant, with each universal character or corresponding
 * extended source character occupying between 6 and 10 characters.
 *
 * In practice, many implementations provide greater limits. For example it is
 * common for external identifiers in C90 to be case-sensitive and for at least
 * the first 31 characters to be significant.
 *
 * In our implementation, we forbid the universal character and offer the
 * options of the length and whether to be case-sensitive for the definition of
 * distinct.
 */

class Checker {
 public:
  void Init(int prefix_length, bool case_sensitive, bool implicit_decl,
            analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  // Variables and functions share the same map.
  std::unordered_map<std::string, identifier> name_locations_;
  ExternalVDCallback* external_vd_callback_;
  ExternalFDCallback* external_fd_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_5_1
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_5_1_CHECKER_H_
