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

#ifndef ANALYZER_MISRA_RULE_11_8_CHECKER_H_
#define ANALYZER_MISRA_RULE_11_8_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_11_8 {

/*
 * From [misra-c2012-11.8]
 * A cast shall not remove any const or volatile qualification from the type
 * pointed to by a pointer
 *
 * this rule should only applies to:
 * (1) both the source and the destination type in the conversion are pointers
 *
 * Note:
 * the qualification is for the type pointed to, not the pointer itself.
 *
 * Procedure:
 * the matcher should match all the cast from one pointer type to another
 * pointer type, then we check the PointeeType:
 * - if source PointeeType has const qualification, destination has not, report
 * error
 * - if source PointeeType has volatile qualification, destination has not,
 * report error
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

}  // namespace rule_11_8
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_11_8_CHECKER_H_
