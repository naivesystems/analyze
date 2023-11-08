/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_16_0_3/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;

namespace misra_cpp_2008 {
namespace rule_16_0_3 {
namespace libtooling {

void FindMacroUndefCallback::Init(analyzer::proto::ResultsList* results_list,
                                  SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

void FindMacroUndefCallback::MacroUndefined(const Token& macro_name_tok,
                                            const MacroDefinition& md,
                                            const MacroDirective* undef) {
  if (!undef) {
    return;
  }
  string path = misra::libtooling_utils::GetRealFilename(undef->getLocation(),
                                                         source_manager_);
  int line = misra::libtooling_utils::GetRealLine(undef->getLocation(),
                                                  source_manager_);
  string error_message = "#undef 不得使用";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_16_0_3);
  return;
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<FindMacroUndefCallback> callback(
      new FindMacroUndefCallback());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
}

}  // namespace libtooling
}  // namespace rule_16_0_3
}  // namespace misra_cpp_2008
