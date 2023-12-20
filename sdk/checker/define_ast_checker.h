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

#ifndef ANALYZER_SDK_CHECKER_DEFINE_AST_CHECKER_H_
#define ANALYZER_SDK_CHECKER_DEFINE_AST_CHECKER_H_

#include <string>

#include "podman_image/bigmain/suffix_rule.h"
#include "sdk/checker/ast_checker.h"

namespace sdk {
namespace checker {

template <class CallbackT>
class DefineASTChecker {
 public:
  // TODO(xjia): separate ruleset name and rule name
  explicit DefineASTChecker(std::string suffix)
      : suffix_rule_(suffix, [suffix](int argc, char** argv) {
          CallbackT callback;
          sdk::checker::ASTChecker checker;
          checker.Init(suffix, &callback);
          return checker.Run(argc, argv);
        }) {}

 private:
  podman_image::bigmain::SuffixRule suffix_rule_;
};

}  // namespace checker
}  // namespace sdk

#endif  // ANALYZER_SDK_CHECKER_DEFINE_AST_CHECKER_H_
