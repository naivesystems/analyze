/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2022-2023  Naive Systems Ltd.

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

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "sdk/checker/define_expr_checker.h"

using namespace clang::ast_matchers;

namespace {

sdk::checker::DefineExprChecker _(
    "misra_cpp_2008/rule_4_10_1", "NULL不得用作整型值",
    implicitCastExpr(hasSourceExpression(expr(gnuNullExpr())),
                     hasImplicitDestinationType(isInteger()),
                     unless(isExpansionInSystemHeader()))
        .bind("cast"),
    "cast");

}  // namespace
