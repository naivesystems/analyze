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

#include "googlecpp/g1150/libtooling/checker.h"

#include <glog/logging.h>

#include <iostream>
#include <regex>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {
void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "In general, every .cc file should have an associated .h file. There are some common exceptions, such as unit tests and small .cc files containing just a main() function.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

bool IsHeaderFile(std::string file_name) {
  return absl::EndsWith(file_name, ".h") || absl::EndsWith(file_name, ".hpp");
}

}  // namespace

namespace googlecpp {
namespace g1150 {
namespace libtooling {

Optional<std::string> filename_prefix{};

void PPCheck::Init(analyzer::proto::ResultsList* results_list,
                   SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

// FileChanged is a callback that is invoked whenever a source file is entered
// or exited. The first file entered is the source file. We use it to get the
// name of the source file.
void PPCheck::FileChanged(SourceLocation Loc, FileChangeReason Reason,
                          SrcMgr::CharacteristicKind FileType, FileID PrevId) {
  if (filename_prefix.has_value()) return;

  const SourceManager& SM = *source_manager_;
  if (source_manager_->isInSystemHeader(Loc)) return;
  if (source_manager_->isInSystemMacro(Loc)) return;

  const FileEntry* File = SM.getFileEntryForID(SM.getFileID(Loc));
  if (File) {
    filename_prefix = llvm::sys::path::stem(File->getName()).str();
  }
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<PPCheck> callback(new PPCheck());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

AST_MATCHER(TranslationUnitDecl, hasJustOneDecl) {
  int decl_count = 0;
  for (auto it : Node.decls()) {
    // There are some implicit typedef declarations,
    // such as __int128 and unsigned __int128
    if (it->isImplicit()) continue;
    if (decl_count > 0) {
      return false;
    }
    ++decl_count;
  }
  return true;
}

// If a source file contains headers, its TranslationUnitDecl will match twice.
// This may be due to Clang's traversal matching strategy.
// These two matching declarations have the same ID, but have different
// pointers.
bool hasTranslationUnitVisit{false};

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    auto matcher =
        translationUnitDecl(unless(translationUnitDecl(
                                has(functionDecl(isMain())), hasJustOneDecl())))
            .bind("translationunit");
    finder->addMatcher(matcher, this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const auto* TU =
            result.Nodes.getNodeAs<TranslationUnitDecl>("translationunit")) {
      if (hasTranslationUnitVisit) return;
      if ("main" == filename_prefix.value_or("")) return;

      // report if each declaration in source file is the first declaration or
      // the first declaration is not in a header file
      // TODO: exclude the exception about static global variable in a class
      SourceManager* SM = result.SourceManager;
      DeclContext::decl_range decls = TU->decls();
      std::unordered_map<std::string, bool> all_first_declaration;
      for (DeclContext::decl_iterator it = decls.begin(); it != decls.end();
           ++it) {
        if (it->isImplicit()) continue;
        SourceLocation loc = it->getLocation();
        if (SM->isInSystemHeader(loc)) continue;
        if (SM->isInSystemMacro(loc)) continue;
        std::string file_name =
            misra::libtooling_utils::GetRealFilename(loc, SM);
        if (IsHeaderFile(file_name)) continue;
        Decl* first_decl = *it;
        if (all_first_declaration.find(file_name) ==
            all_first_declaration.end()) {
          all_first_declaration.emplace(file_name, true);
        } else if (!all_first_declaration.at(file_name))
          continue;
        while (!first_decl->isFirstDecl()) {
          first_decl = first_decl->getPreviousDecl();
        }
        if (IsHeaderFile(misra::libtooling_utils::GetRealFilename(
                first_decl->getLocation(), SM))) {
          all_first_declaration[file_name] = false;
        }
      }
      for (auto it = all_first_declaration.begin();
           it != all_first_declaration.end(); ++it) {
        if (it->second) {
          ReportError(it->first, 1, results_list_);
        }
      }
      hasTranslationUnitVisit = true;
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

}  // namespace libtooling
}  // namespace g1150
}  // namespace googlecpp
