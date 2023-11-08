/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
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
