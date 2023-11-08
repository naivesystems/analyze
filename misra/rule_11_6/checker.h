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

#ifndef ANALYZER_MISRA_RULE_11_6_CHECKER_H_
#define ANALYZER_MISRA_RULE_11_6_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_11_6 {

/*
 * From [misra-c2012-11.6]
 * A cast shall not be performed between pointer to void and a arithmetic type
 *
 * Exception:
 * An integer constant expression with value 0 may be cast into pointer to void.
 *
 * Procedure:
 * - use 'anyOf(realFloatingPointType(), isInteger())' to match all arithmetic
 *        types
 * - use 'integerLiteral(integerZero())' to match zero integer constant
 * - then, we use the above to match:
 *   - Source is pointer to void, and destination is arithmetic type
 *   - Source is arithmetic type but not zero integer constant, and destination
 *     is pointer to void
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

}  // namespace rule_11_6
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_11_6_CHECKER_H_
