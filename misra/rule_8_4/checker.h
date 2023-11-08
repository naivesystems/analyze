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

#ifndef ANALYZER_MISRA_RULE_8_4_CHECKER_H_
#define ANALYZER_MISRA_RULE_8_4_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_8_4 {

class ExternalVDCallback;
class ExternalFDCallback;

/*
 * From [misra-c2012-5.7]:
 * A compatible declaration shall be visible when an object or function with
 * external linkage is defined
 *
 * There are three situation that this rule will report error
 * 1. variable has no declaration
 * 2. function has no declaration
 * 3. parameter type in both function definition and declaration are
 * incompatible
 *
 * For situation 1:
 * When we catch a var decl, we will validate if this decl has external linkage
 * and skip declaration. Then we will check if the decl is a c-style
 * initialization. If it is a c-style initialization then we will check if it
 * has previous declaration. If not, we need to report error. Because it means
 * that this decl is in int i = 1; this format. The procedure is like:
 *
 * skip non external linkage decl
 * skip declaration
 * if decl->getInitStyle() == CInit and decl->getPreviousDecl() == NULL:
 *    reportError
 *
 *
 * For situation 2:
 * When we catch a function decl, if it has no previous declaration and it is a
 * definition we will report error.
 *
 * For situation 3:
 * If the function parameter type is incompatible, the function definition will
 * be an invalid decl. We will report error when we catch an invalid decl.
 *
 */
class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  ExternalVDCallback* vd_callback_;
  ExternalFDCallback* fd_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_8_4
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_8_4_CHECKER_H_
