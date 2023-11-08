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

#ifndef ANALYZER_MISRA_RULE_5_6_CHECKER_H_
#define ANALYZER_MISRA_RULE_5_6_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <unordered_map>

#include "misra/proto_util.h"

namespace misra {
namespace rule_5_6 {

class TypedefNameUniqueCallback;

/*
 * From [misra-c2012-5.6]
 * A typedef name shall be a unique identifier
 *
 * But there is an exception:
 * The typedef name may be the same as the structure, union or enumeration tag
 * name associated with the typedef
 *
 * With this exception, there are three kinds of declaration:
 * (1) typedef
 * (2) declarator(including variable and function)
 * (3) tag (including struct, union and enum)
 *
 * 1. For typedef, we should check wheteher its name is unique accross all the
 *    three kinds of declaration. And we also need to record its associated
 *    tag's first declaration location to check such bad case:
 *
 *    bad1.c:
 *    typedef strcut a{int x;} a;
 *    bad2.c:
 *    struct a {int y;};
 *
 * 2. For declarator, we only need to check whether it reuses typedef name
 *
 * 3. For tag, we also need to check whether it reuses typedef name. If yes but
 *    the current tag and typedef associated tag have the same first declaration
 *    location, this is the exception.
 */

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  TypedefNameUniqueCallback* typedef_name_unique_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_5_6
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_5_6_CHECKER_H_
