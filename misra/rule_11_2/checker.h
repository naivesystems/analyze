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

#ifndef ANALYZER_MISRA_RULE_11_2_CHECKER_H_
#define ANALYZER_MISRA_RULE_11_2_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_11_2 {

/*
 * From [misra-c2012-11.3]
 * Conversions shall not be performed between a pointer to an incomplete
 * type and any other type
 *
 * According to the Amplification and the Exception:
 * (1) a pointer to an incomplete type here is not void type
 * (2) a null pointer constant may be converted into a pointer to an incomplete
 * type
 * (3) a pointer to an incomplete type may be converted into void
 * (4) an incomplete type here should be unqualified
 *
 * Procedure:
 * the matcher should match all the cast from or into a pointer type:
 * (this is because we cannot match incomplete type directly.)
 * - if this is a cast into a pointer:
 *   - if this cast is not into a pointer to an incomplete type, return
 *   - if this cast is from null, return
 *   - if this cast is into a pointer to void, return
 * - if this is a cast from a pointer to an incomplete type:
 *   - if this cast is not from a pointer to an incomplete type, return
 *   - if this cast is into void type, return
 *   - if this cast is from a pointer to void, return
 * - if the unqualified pointer object is the same, return
 * - report error
 *
 */
class CastCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  CastCallback* callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_11_2
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_11_2_CHECKER_H_
