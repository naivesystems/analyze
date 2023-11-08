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

#ifndef ANALYZER_MISRA_CPP_2008_rule_14_7_1_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_rule_14_7_1_LIBTOOLING_CHECKER_H_

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using namespace clang;

namespace misra_cpp_2008 {
namespace rule_14_7_1 {
namespace libtooling {
class ClassTemplateDeclCallback;
class ClassTemplateInstanceCallback;
class FuncTemplateInitCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  ClassTemplateInstanceCallback* class_template_callback_;
  ClassTemplateDeclCallback* class_template_decl_callback_;
  FuncTemplateInitCallback* func_template_callback_;
  ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace rule_14_7_1
}  // namespace misra_cpp_2008

#endif  // ANALYZER_MISRA_CPP_2008_rule_14_7_1_LIBTOOLING_CHECKER_H_
