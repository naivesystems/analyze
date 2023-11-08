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

#ifndef ANALYZER_MISRA_RULE_11_7_CHECKER_H_
#define ANALYZER_MISRA_RULE_11_7_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_11_7 {

/*
 * From [misra-c2012-11.7]
 * A cast shall not be performed between pointer to object and a non-interger
 * arithmetic type
 *
 * Amplification:
 * a non-interger arithmetic type is:
 * - boolean
 * - character
 * - enum
 * - floating
 *
 * Procedure:
 * This is similar to 11.6. In 11.6, 'isInteger()' can match all the boolean,
 * character and enum type, but here we need to separate them from basic
 * interger type.
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

}  // namespace rule_11_7
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_11_7_CHECKER_H_
