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

#include "googlecpp/g1201/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;

namespace googlecpp {
namespace g1201 {
namespace libtooling {

void Check::Init(analyzer::proto::ResultsList* results_list,
                 SourceManager* source_manager, string macro_prefix) {
  results_list_ = results_list;
  source_manager_ = source_manager;
  macro_prefix_ = macro_prefix;
}

void Check::MacroDefined(const Token& macro_name_tok,
                         const MacroDirective* md) {
  if (!md->isDefined()) return;  // #define
  const MacroInfo* macro_info = md->getMacroInfo();

  const auto macro_loc = macro_info->getDefinitionLoc();
  if (source_manager_->isInSystemHeader(macro_loc)) return;
  if (source_manager_->isInSystemMacro(macro_loc)) return;

  string path =
      misra::libtooling_utils::GetRealFilename(macro_loc, source_manager_);
  StringRef Path(path);
  if (!Path.endswith(".h")) return;

  int line = misra::libtooling_utils::GetRealLine(macro_loc, source_manager_);
  StringRef Name = macro_name_tok.getIdentifierInfo()->getName();
  auto isProperPrefix = [&]() {
    if (macro_prefix_ == "_") {  // default check:
                                 // precede with CAPITALs and a underline
      auto pos = Name.find_first_of("_");
      if (pos == StringRef::npos) return false;
      return Name.startswith(Name.substr(0, pos).upper());
    } else {  // strict mode
      return Name.startswith(macro_prefix_);
    }
  }();
  if (!isProperPrefix) {
    string error_message = "Name macros with a project-specific prefix";
    AddResultToResultsList(results_list_, Path.str(), line, error_message);
    LOG(INFO) << absl::StrFormat("%s, name: %s, path: %s, line: %d",
                                 error_message, Name.str(), Path.str(), line);
  }
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<Check> callback(new Check());
  callback->Init(results_list_, &ci.getSourceManager(), macro_preix_);
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list,
                   llvm::StringRef project_name) {
  results_list_ = results_list;
  macro_preix_ = project_name.upper() + "_";
}

}  // namespace libtooling
}  // namespace g1201
}  // namespace googlecpp
