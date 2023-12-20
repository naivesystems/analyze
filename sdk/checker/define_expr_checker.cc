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

#include "sdk/checker/define_expr_checker.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <string>

#include "sdk/checker/expr_checker.h"

namespace sdk {
namespace checker {

DefineExprChecker::DefineExprChecker(
    std::string suffix, std::string message,
    clang::ast_matchers::StatementMatcher matcher, std::string bind_id)
    : suffix_rule_(suffix,
                   [suffix, message, matcher, bind_id](int argc, char** argv) {
                     sdk::checker::ExprChecker checker;
                     checker.Init(suffix, message, matcher, bind_id);
                     return checker.Run(argc, argv);
                   }) {}

}  // namespace checker
}  // namespace sdk
