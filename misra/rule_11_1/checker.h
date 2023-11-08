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

#ifndef ANALYZER_MISRA_RULE_11_1_CHECKER_H_
#define ANALYZER_MISRA_RULE_11_1_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_11_1 {

/*
 * From [misra-c2012-11.1]
 * Conversions shall not be performed between a pointer to a function
 * and any other type
 *
 * Amplification:
 * a pointer to a function shall only be converted into or from a pointer to a
 * function with a compatible type. In Exception, we can see a compatible type
 * is:
 * - null pointer constant (Exception 1)
 * - void (Exception 2)
 * - that function type (Exception 3)
 *
 * Procedure:
 * - match all casts from or into a function pointer type, as
 *   'pointsTo(parenType(innerType(functionType())))', after that:
 *   - use 'isNullPointerConstant' for Exception 1
 *   - use 'isVoidType' for Exception 2
 *   - use 'isFunctionType' and 'getPointeeType' for Exception 3
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

}  // namespace rule_11_1
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_11_1_CHECKER_H_
