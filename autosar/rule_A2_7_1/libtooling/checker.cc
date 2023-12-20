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

#include "autosar/rule_A2_7_1/libtooling/checker.h"

#include <glog/logging.h>

#include <regex>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

using namespace clang;
using namespace misra::proto_util;

namespace autosar {
namespace rule_A2_7_1 {
namespace libtooling {

std::set<FileID> checkedFileIDs;
void CheckCommentConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  std::set<FileID> fileIDs;
  for (auto decl : context.getTranslationUnitDecl()->decls()) {
    SourceLocation loc =
        context.getSourceManager().getSpellingLoc(decl->getBeginLoc());
    FileID fileID = context.getSourceManager().getFileID(loc);
    if (fileID.isValid() && checkedFileIDs.count(fileID) == 0) {
      fileIDs.insert(fileID);
      checkedFileIDs.insert(fileID);
    }
  }
  for (auto fileID : fileIDs) {
    auto comments = context.Comments.getCommentsInFile(fileID);
    if (!comments) continue;
    for (auto it = comments->begin(); it != comments->end(); it++) {
      clang::RawComment* comment = it->second;
      std::string source = comment->getFormattedText(context.getSourceManager(),
                                                     context.getDiagnostics());
      std::vector<std::string> source_lines = absl::StrSplit(source, "\n");
      for (auto line = source_lines.begin(); line != source_lines.end();
           line++) {
        if (line->empty()) continue;
        // The last character of comments cannot be '\'.
        if (line->back() == '\\' &&
            line->substr(line->length() - 2, 2) != "\\\\") {
          clang::SourceLocation loc =
              context.getSourceManager().getSpellingLoc(comment->getBeginLoc());
          std::string path = context.getSourceManager().getFilename(loc).str();
          int line_number =
              context.getSourceManager().getPresumedLineNumber(loc) +
              (line - source_lines.begin());
          ReportError(path, line_number, results_list_);
        }
      }
    }
  }
}

void CheckCommentConsumer::ReportError(std::string path, int line_number,
                                       ResultsList* results_list) {
  std::string error_message =
      "The character \\ shall not occur as a last character of a C++ comment.";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace libtooling
}  // namespace rule_A2_7_1
}  // namespace autosar
