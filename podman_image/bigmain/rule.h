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

#ifndef ANALYZER_PODMAN_IMAGE_BIGMAIN_RULE_H_
#define ANALYZER_PODMAN_IMAGE_BIGMAIN_RULE_H_

#include <vector>

namespace podman_image {
namespace bigmain {

class Rule {
 public:
  static std::vector<Rule*>& GetAllRules() {
    static std::vector<Rule*>* all_rules = new std::vector<Rule*>();
    return *all_rules;
  }

  Rule() { GetAllRules().push_back(this); }

  // Returns true if this rule is handled
  virtual bool Entrypoint(int argc, char** argv, int* return_value) = 0;
};

}  // namespace bigmain
}  // namespace podman_image

#endif
