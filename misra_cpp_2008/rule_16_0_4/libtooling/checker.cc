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

#include "misra_cpp_2008/rule_16_0_4/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;

namespace misra_cpp_2008 {
namespace rule_16_0_4 {
namespace libtooling {

void FindMacroDefineCallback::Init(analyzer::proto::ResultsList* results_list,
                                   SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

void FindMacroDefineCallback::MacroDefined(const Token& macro_name_tok,
                                           const MacroDirective* md) {
  string name = macro_name_tok.getIdentifierInfo()->getName().str();
  const MacroInfo* macro_info = md->getMacroInfo();
  auto loc = macro_info->getDefinitionLoc();
  if (!macro_info->isFunctionLike()) {
    return;
  }
  string path = misra::libtooling_utils::GetRealFilename(loc, source_manager_);
  int line = misra::libtooling_utils::GetRealLine(loc, source_manager_);
  string error_message = "不应定义类似函数的宏";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_16_0_4);
  return;
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<FindMacroDefineCallback> callback(
      new FindMacroDefineCallback());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
}

}  // namespace libtooling
}  // namespace rule_16_0_4
}  // namespace misra_cpp_2008
