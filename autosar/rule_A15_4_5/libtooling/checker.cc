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

#include "autosar/rule_A15_4_5/libtooling/checker.h"

#include <glog/logging.h>

#include <regex>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Checked exceptions that could be thrown from a function shall be specified together with the function declaration and they shall be identical in all function declarations and for all its overriders.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A15_4_5 {
namespace libtooling {

struct Loc {
  const string path;
  const int line_number;
};

struct Info {
  const Loc* location;
  bool is_thrown = false;
  // either specified in @throw or is a base class overrided
  bool is_specified = false;
};

unordered_map<string, Info> exception_map;

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(
            forEachDescendant(cxxThrowExpr(hasDescendant(
                cxxConstructExpr(hasDeclaration(cxxConstructorDecl(hasParent(
                    cxxRecordDecl(anyOf(isDerivedFrom(cxxRecordDecl().bind(
                                            "override_cls")),
                                        unless(isDerivedFrom(cxxRecordDecl()))))
                        .bind("exception_cls")))))))),
            unless(hasAncestor(functionTemplateDecl())),
            unless(isExpansionInSystemHeader()))
            .bind("fd"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    string path =
        misra::libtooling_utils::GetFilename(fd, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(fd, result.SourceManager);

    const CXXRecordDecl* exception_cls =
        result.Nodes.getNodeAs<CXXRecordDecl>("exception_cls");
    string name = exception_cls->getNameAsString();

    const CXXRecordDecl* override_cls =
        result.Nodes.getNodeAs<CXXRecordDecl>("override_cls");
    if (override_cls != nullptr) {
      string override_cls_name = override_cls->getNameAsString();
      if (exception_map.find(name) != exception_map.end() &&
          !misra::libtooling_utils::IsInSystemHeader(override_cls,
                                                     result.Context)) {
        exception_map[override_cls_name].is_specified = true;
        exception_map[override_cls_name].location = new Loc{path, line};
      }
    }
    exception_map[name].is_thrown = true;
    exception_map[name].location = new Loc{path, line};
  }

  void Report() {
    for (auto it = exception_map.begin(); it != exception_map.end(); ++it) {
      const Info info = it->second;
      if ((info.is_specified && !info.is_thrown) ||
          (!info.is_specified && info.is_thrown)) {
        ReportError(info.location->path, info.location->line_number,
                    results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

void Checker::Report() { callback_->Report(); }

void CheckCommentConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  auto comments = context.Comments.getCommentsInFile(
      context.getSourceManager().getMainFileID());
  if (!comments) return;
  for (auto it = comments->begin(); it != comments->end(); it++) {
    clang::RawComment* comment = it->second;
    string source = comment->getFormattedText(context.getSourceManager(),
                                              context.getDiagnostics());
    vector<string> source_lines = absl::StrSplit(source, "\n");
    for (auto line = source_lines.begin(); line != source_lines.end(); line++) {
      if (line->empty()) continue;
      string throw_str = "@throw ";
      size_t throw_pos = line->find(throw_str);
      if (throw_pos != std::string::npos) {
        size_t name_start = throw_pos + throw_str.length();
        size_t name_end = line->find_first_of(" \t", name_start + 1);
        if (name_end == std::string::npos) {
          name_end = line->size();
        }
        std::string name = line->substr(name_start, name_end - name_start);
        clang::SourceLocation loc =
            context.getSourceManager().getSpellingLoc(comment->getBeginLoc());
        string path = context.getSourceManager().getFilename(loc).str();
        int line_number =
            context.getSourceManager().getPresumedLineNumber(loc) +
            (line - source_lines.begin());
        // Current implementation cannot relate the throw specification with
        // its function declaration, so we only record the location of the
        // specification (may be overwritten) for reporting further.
        exception_map[name].is_specified = true;
        exception_map[name].location = new Loc{path, line_number};
      }
    }
  }
}

}  // namespace libtooling
}  // namespace rule_A15_4_5
}  // namespace autosar
