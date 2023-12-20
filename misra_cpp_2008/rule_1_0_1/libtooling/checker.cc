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

#include "misra_cpp_2008/rule_1_0_1/libtooling/checker.hh"

#include <clang/Basic/SourceManager.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"

using namespace clang;
using namespace misra::proto_util;

namespace misra_cpp_2008 {
namespace rule_1_0_1 {
namespace libtooling {

void Checker::HandleDiagnostic(DiagnosticsEngine::Level level,
                               const Diagnostic& d) {
  if (!d.getLocation().isValid()) {
    return;
  }
  if (d.getSourceManager().isInSystemHeader(d.getLocation())) {
    return;
  }
  if (level == DiagnosticsEngine::Level::Error ||
      level == DiagnosticsEngine::Level::Fatal) {
    std::string error_message = "所有代码必须遵循 C++2003 标准";
    std::string path = d.getSourceManager().getFilename(d.getLocation()).str();
    int line = d.getSourceManager().getPresumedLineNumber(d.getLocation());
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_1_0_1);
  }
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
}

}  // namespace libtooling
}  // namespace rule_1_0_1
}  // namespace misra_cpp_2008
