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

#include "autosar/rule_A2_3_1/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {
const std::string basic_chars =
    " \t\v\f\nabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}[]#()<>%:;.?*+-/^&|~!=,\\\"'";

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Only those characters specified in the C++ Language Standard basic source character set shall be used in the source code.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A2_3_1 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        varDecl(unless(isExpansionInSystemHeader())).bind("value_decl"), this);
    finder->addMatcher(
        fieldDecl(unless(isExpansionInSystemHeader())).bind("value_decl"),
        this);
    finder->addMatcher(
        parmVarDecl(unless(isExpansionInSystemHeader())).bind("value_decl"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("func_decl"),
        this);
    finder->addMatcher(stringLiteral(unless(isExpansionInSystemHeader()))
                           .bind("string_literal"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("value_decl");
    if (var_decl &&
        !misra::libtooling_utils::IsInSystemHeader(var_decl, result.Context)) {
      std::string var_name = var_decl->getNameAsString();
      for (const char c : var_name) {
        if (basic_chars.find(c) == std::string::npos) {
          ReportError(
              misra::libtooling_utils::GetFilename(var_decl,
                                                   result.SourceManager),
              misra::libtooling_utils::GetLine(var_decl, result.SourceManager),
              results_list_);
          break;
        }
      }
    }
    const FunctionDecl* func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl");
    if (func_decl &&
        !misra::libtooling_utils::IsInSystemHeader(func_decl, result.Context)) {
      std::string var_name = func_decl->getNameAsString();
      for (const char c : var_name) {
        if (basic_chars.find(c) == std::string::npos) {
          ReportError(
              misra::libtooling_utils::GetFilename(func_decl,
                                                   result.SourceManager),
              misra::libtooling_utils::GetLine(func_decl, result.SourceManager),
              results_list_);
          break;
        }
      }
    }
    const StringLiteral* string_literal =
        result.Nodes.getNodeAs<StringLiteral>("string_literal");
    if (string_literal && !string_literal->isWide() &&
        !string_literal->isUTF8() &&
        !misra::libtooling_utils::IsInSystemHeader(string_literal,
                                                   result.Context)) {
      StringRef string_value = string_literal->getBytes();
      for (const char c : string_value) {
        if (basic_chars.find(c) == std::string::npos) {
          ReportError(misra::libtooling_utils::GetFilename(
                          string_literal, result.SourceManager),
                      misra::libtooling_utils::GetLine(string_literal,
                                                       result.SourceManager),
                      results_list_);
          break;
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

void CheckCommentConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  auto comments = context.Comments.getCommentsInFile(
      context.getSourceManager().getMainFileID());
  if (!comments) return;
  for (auto it = comments->begin(); it != comments->end(); it++) {
    clang::RawComment* comment = it->second;
    std::string source = comment->getFormattedText(context.getSourceManager(),
                                                   context.getDiagnostics());
    std::vector<std::string> source_lines = absl::StrSplit(source, "\n");
    for (auto line = source_lines.begin(); line != source_lines.end(); line++) {
      if (line->empty()) continue;
      for (auto c : *line) {
        if (basic_chars.find(c) == std::string::npos && c != '@') {
          clang::SourceLocation loc =
              context.getSourceManager().getSpellingLoc(comment->getBeginLoc());
          ReportError(context.getSourceManager().getFilename(loc).str(),
                      context.getSourceManager().getPresumedLineNumber(loc) +
                          (line - source_lines.begin()),
                      results_list_);
          break;
        }
      }
    }
  }
}

}  // namespace libtooling
}  // namespace rule_A2_3_1
}  // namespace autosar
