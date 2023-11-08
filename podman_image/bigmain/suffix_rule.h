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

#ifndef ANALYZER_PODMAN_IMAGE_BIGMAIN_SUFFIX_RULE_H_
#define ANALYZER_PODMAN_IMAGE_BIGMAIN_SUFFIX_RULE_H_

#include <string>

#include "podman_image/bigmain/rule.h"

namespace podman_image {
namespace bigmain {

class SuffixRule : Rule {
 public:
  typedef int (*Fn)(int, char**);

  SuffixRule(std::string suffix, Fn fn) : suffix_(suffix), fn_(fn) {}

  bool Entrypoint(int argc, char** argv, int* return_value) override;

 private:
  std::string suffix_;
  Fn fn_;
};

}  // namespace bigmain
}  // namespace podman_image

#endif
