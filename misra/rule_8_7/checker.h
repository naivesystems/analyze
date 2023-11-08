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

#ifndef ANALYZER_MISRA_RULE_8_7_CHECKER_H_
#define ANALYZER_MISRA_RULE_8_7_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_8_7 {

class ExternalVDCallback;

struct location {
  std::string path;
  int line_number;
  std::string loc;
  std::string first_decl_path;
  std::string first_decl_loc;
};

/*
 */
class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }
  void Run();

 private:
  ExternalVDCallback* vd_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<std::string, std::vector<location>> vd_name_locations_;
};

}  // namespace rule_8_7
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_8_7_CHECKER_H_
