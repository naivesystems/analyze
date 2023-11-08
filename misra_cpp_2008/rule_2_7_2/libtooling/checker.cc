/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_2_7_2/libtooling/checker.hh"

#include <glog/logging.h>

#include <regex>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;

namespace misra_cpp_2008 {
namespace rule_2_7_2 {
namespace libtooling {

void CheckCommentConsumer::HandleTranslationUnit(ASTContext& context) {
  std::set<FileID> fileIDs;
  for (auto decl : context.getTranslationUnitDecl()->decls()) {
    // It walks through all top delcs and records their source files. The source
    // files will be stored at a set and be checked later.
    SourceLocation loc =
        context.getSourceManager().getSpellingLoc(decl->getBeginLoc());
    FileID fileID = context.getSourceManager().getFileID(loc);
    if (fileID.isValid()) {
      fileIDs.insert(fileID);
    }
  }
  for (auto fileID : fileIDs) {
    // It read comments from source files stored above, and check them one by
    // one.
    auto comments = context.Comments.getCommentsInFile(fileID);
    if (comments) {
      for (auto it = comments->begin(); it != comments->end(); it++) {
        RawComment* comment = it->second;
        std::string source = comment->getFormattedText(
            context.getSourceManager(), context.getDiagnostics());
        std::vector<std::string> source_lines = absl::StrSplit(source, "\n");
        for (auto line = source_lines.begin(); line != source_lines.end();
             line++) {
          if (LooksLikeCode(*line)) {
            ReportError(context, comment);
          }
        }
      }
    }
  }
}

bool CheckCommentConsumer::LooksLikeCode(std::string& line) {
  if (line.empty()) return false;
  if (line.front() == '#' || line.back() == ';' || line.back() == '{' ||
      line.front() == '}')
    return true;
  const std::regex pattern1(".*;\\s*//.*");    // some code; // some comment
  const std::regex pattern2(".*;\\s*/\\*.*");  // some code /* some comment
  return std::regex_match(line, pattern1) || std::regex_match(line, pattern2);
}

void CheckCommentConsumer::ReportError(ASTContext& context,
                                       RawComment* comment) {
  // Note: we can report the exact line of comment similar to code,
  //       but now we report error at the line the comments block begin.
  SourceLocation loc =
      context.getSourceManager().getSpellingLoc(comment->getBeginLoc());
  std::string path = misra::libtooling_utils::GetLocationFilename(
      loc, &context.getSourceManager());
  int line = misra::libtooling_utils::GetLocationLine(
      loc, &context.getSourceManager());
  std::string error_message = "不得使用C语言风格的注释将代码段“注释掉”";
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_2_7_2);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line);
}

}  // namespace libtooling
}  // namespace rule_2_7_2
}  // namespace misra_cpp_2008
