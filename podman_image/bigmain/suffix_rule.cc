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

#include "podman_image/bigmain/suffix_rule.h"

#include "absl/strings/match.h"

namespace podman_image {
namespace bigmain {

bool SuffixRule::Entrypoint(int argc, char** argv, int* return_value) {
  if (!absl::EndsWith(argv[0], suffix_)) return false;
  *return_value = fn_(argc, argv);
  return true;
}

}  // namespace bigmain
}  // namespace podman_image
