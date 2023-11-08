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

#include "autosar/rule_A1_1_1/libtooling/checker.h"

#include <clang/Basic/SourceManager.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"

using analyzer::proto::ResultsList;
using namespace clang;
using namespace misra::proto_util;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "All code shall conform to ISO/IEC 14882:2014 - Programming Language C++ and shall not use deprecated features.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A1_1_1 {
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
      level == DiagnosticsEngine::Level::Warning ||
      level == DiagnosticsEngine::Level::Fatal) {
    std::string path = d.getSourceManager().getFilename(d.getLocation()).str();
    int line = d.getSourceManager().getPresumedLineNumber(d.getLocation());
    ReportError(path, line, results_list_);
  }
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
}

}  // namespace libtooling
}  // namespace rule_A1_1_1
}  // namespace autosar
