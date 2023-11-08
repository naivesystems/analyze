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

#ifndef ANALYZER_MISRA_RULE_11_4_CHECKER_H_
#define ANALYZER_MISRA_RULE_11_4_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_11_4 {

/*
 * From [misra-c2012-11.4]
 * A conversion should not be performed between a pointer to object and an
 * integer type
 *
 * According to the Amplification and the Examples, this rule applies to:
 * (1) convert a pointer to object to an integer type
 * (2) convert an integer type to a pointer to object
 *
 * Exception:
 * convert null constant pointer with integer type to a pointer to object
 *
 * Procedure:
 * the matcher should match all the cast between a pointer to object and an
 * integer type
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

}  // namespace rule_11_4
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_11_4_CHECKER_H_
