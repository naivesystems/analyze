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

#ifndef ANALYZER_SDK_CHECKER_EXPR_CHECKER_H_
#define ANALYZER_SDK_CHECKER_EXPR_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <string>

#include "sdk/checker/ast_checker.h"

namespace sdk {
namespace checker {

class ExprChecker : public ASTChecker {
 public:
  void Init(std::string name, std::string message,
            clang::ast_matchers::StatementMatcher matcher, std::string bind_id);

 private:
  ASTCheckerCallback* callback_;
};

}  // namespace checker
}  // namespace sdk

#endif  // ANALYZER_SDK_CHECKER_EXPR_CHECKER_H_
