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

#include "googlecpp/g1157/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;

namespace googlecpp {
namespace g1157 {
namespace libtooling {

void Check::Init(analyzer::proto::ResultsList* results_list,
                 SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

void Check::InclusionDirective(SourceLocation HashLoc, const Token& IncludeTok,
                               StringRef FileName, bool IsAngled,
                               CharSourceRange FilenameRange,
                               Optional<FileEntryRef> File,
                               StringRef SearchPath, StringRef RelativePath,
                               const Module* Imported,
                               SrcMgr::CharacteristicKind FileType) {
  if (source_manager_->isInSystemHeader(HashLoc)) return;
  if (source_manager_->isInSystemMacro(HashLoc)) return;
  if (!FileName.contains("./") && !FileName.contains("../")) return;

  string path =
      misra::libtooling_utils::GetRealFilename(HashLoc, source_manager_);
  int line = misra::libtooling_utils::GetRealLine(HashLoc, source_manager_);

  string error_message =
      "All of a project's header files should be listed as descendants of the project's source directory without use of UNIX directory aliases . (the current directory) or .. (the parent directory)";
  AddResultToResultsList(results_list_, path, line, error_message);
  LOG(INFO) << absl::StrFormat("%s, name: %s, path: %s, line: %d",
                               error_message, FileName.str(), path, line);
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<Check> callback(new Check());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
}

}  // namespace libtooling
}  // namespace g1157
}  // namespace googlecpp
