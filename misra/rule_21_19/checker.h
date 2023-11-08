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

#ifndef ANALYZER_MISRA_RULE_21_19_CHECKER_H_
#define ANALYZER_MISRA_RULE_21_19_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_21_19 {

/*
 * From [misra-c2012-21.19]
 * The pointers returned by the Standard Library functions localeconv, getenv,
 * setlocale or strerror shall be const qualified and not editable.
 *
 * Different from  misrac-2012-21_19-ConstPointerReturnChecker.cpp, this matcher
 * provides a simple type check fot part of the rule 21.19:
 * The pointers returned by the Standard Library functions localeconv, getenv,
 * setlocale or strerror shall be assigned to const qualified variables.
 *
 * Exception:
 * The return value can be casted into void.(ignored)
 *
 * Procedure:
 * (1) the matcher should match all the cast from a call to these functions:
 *  - if the Destination type is void, ignore;
 *  - if the Destination type is a pointer type, and the pointee type is not
 *    const qualified, report error.
 *  - integer type conversion for const is not supported by AST matcher, ignored
 * (2) if there is a call to these functions without cast in it's parent expr,
 *    then it's assigned to a non-const type. report error.
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

}  // namespace rule_21_19
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_21_19_CHECKER_H_
