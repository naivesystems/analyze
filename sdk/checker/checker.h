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

#ifndef ANALYZER_SDK_CHECKER_CHECKER_H_
#define ANALYZER_SDK_CHECKER_CHECKER_H_

namespace sdk {
namespace checker {

class Checker {
 public:
  virtual ~Checker() {}

  virtual int Run(int argc, char** argv) = 0;
};

}  // namespace checker
}  // namespace sdk

#endif  // ANALYZER_SDK_CHECKER_CHECKER_H_
