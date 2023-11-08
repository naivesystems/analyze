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

#include "googlecpp/g1149/libtooling/checker.h"

#include <glog/logging.h>

#include <iostream>
#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {
void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "If a source or header file refers to a symbol defined elsewhere, the file should directly include a header file which properly intends to provide a declaration or definition of that symbol";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

void CheckInMainFile(const Decl* decl, SourceManager* SM,
                     analyzer::proto::ResultsList* results_list_,
                     const std::string& main_filename,
                     const std::unordered_set<std::string> header_files) {
  std::string filename = misra::libtooling_utils::GetFilename(decl, SM);
  // Report errors in main file only.
  if (filename != main_filename) return;

  int line = misra::libtooling_utils::GetLine(decl, SM);
  ReportError(filename, line, results_list_);
}

void CheckTransitivelyIncluded(const string& filename, int line,
                               const Decl* decl, SourceManager* SM,
                               analyzer::proto::ResultsList* results_list_,
                               const std::string& main_filename,
                               unordered_set<std::string>& header_files) {
  // Get the file path of the declaration to verify that the declaration is
  // properly included.
  std::string header_filename = misra::libtooling_utils::GetFilename(decl, SM);
  // We only find the symbols that are included from header files.
  if (header_filename != filename && 0 == header_files.count(header_filename)) {
    // If the declaration is not properly included, we will report error and add
    // its file to header_files for once-only reporting.
    header_files.insert(header_filename);
    ReportError(filename, line, results_list_);
  }
}

}  // namespace

namespace googlecpp {
namespace g1149 {
namespace libtooling {

std::string main_filename{};
unordered_set<std::string> header_files;

void PPCheck::Init(analyzer::proto::ResultsList* results_list,
                   SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

// FileChanged is a callback that is invoked whenever a source file is entered
// or exited. The first file entered is the source file(main_filename). We use
// it to get the name of the source file.
void PPCheck::FileChanged(SourceLocation Loc, FileChangeReason Reason,
                          SrcMgr::CharacteristicKind FileType, FileID PrevId) {
  // Use PrevId's isValid to verify that the file is the main source file.
  if (PrevId.isValid()) return;

  SourceManager* SM = source_manager_;
  if (SM->isInSystemHeader(Loc)) return;
  if (SM->isInSystemMacro(Loc)) return;

  const FileEntry* File = SM->getFileEntryForID(SM->getFileID(Loc));
  if (File) {
    main_filename = misra::libtooling_utils::GetRealFilename(Loc, SM);
  }
}

// This callback is invoked whenever an inclusion directive of any kind
// (#include, #import, etc.) is processed, regardless of whether the inclusion
// actually results in an inclusion. We use it to collect the names of all
// header files.
void PPCheck::InclusionDirective(SourceLocation HashLoc,
                                 const Token& IncludeTok, StringRef FileName,
                                 bool IsAngled, CharSourceRange FilenameRange,
                                 Optional<FileEntryRef> File,
                                 StringRef SearchPath, StringRef RelativePath,
                                 const Module* Imported,
                                 SrcMgr::CharacteristicKind FileType) {
  SourceManager* SM = source_manager_;
  if (SM->isInSystemHeader(HashLoc)) return;
  if (SM->isInSystemMacro(HashLoc)) return;

  std::string file = misra::libtooling_utils::GetRealFilename(HashLoc, SM);
  if (file != main_filename) return;

  if (!File.hasValue()) return;
  StringRef header_path = File.value().getFileEntry().tryGetRealPathName();
  header_files.insert(header_path.str());
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<PPCheck> callback(new PPCheck());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

AST_MATCHER(VarDecl, isExternalFirstDecl) {
  return Node.isFirstDecl() && Node.hasExternalStorage();
}

void Callback::Init(analyzer::proto::ResultsList* results_list,
                    ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;

  // There are two situations that should not occur in .cc files.
  // 1. external variables;
  auto external_first_variable_matcher =
      varDecl(unless(isExpansionInSystemHeader()), isExternalFirstDecl())
          .bind("externalvar");
  finder->addMatcher(external_first_variable_matcher, this);

  // 2. function declaration without definition
  auto function_only_decl_matcher =
      functionDecl(unless(isExpansionInSystemHeader()), unless(isDefinition()))
          .bind("function_only_decl");
  finder->addMatcher(function_only_decl_matcher, this);

  // And do not rely on transitive inclusions of types, variables, and
  // functions.
  // 3. using types that are transitively included;
  auto var_decl_matcher =
      varDecl(unless(isExpansionInSystemHeader()),
              hasType(qualType(hasDeclaration(
                  decl(unless(isExpansionInSystemHeader())).bind("type")))))
          .bind("decl");
  finder->addMatcher(var_decl_matcher, this);
  auto parm_var_decl_matcher =
      parmVarDecl(unless(isExpansionInSystemHeader()),
                  hasType(qualType(hasDeclaration(
                      decl(unless(isExpansionInSystemHeader())).bind("type")))))
          .bind("decl");
  finder->addMatcher(parm_var_decl_matcher, this);
  auto return_type_matcher =
      functionDecl(unless(isExpansionInSystemHeader()),
                   returns(hasDeclaration(
                       decl(unless(isExpansionInSystemHeader())).bind("type"))))
          .bind("decl");
  finder->addMatcher(return_type_matcher, this);

  // 4. using variables that are transitively included.
  auto decl_ref_matcher =
      declRefExpr(
          unless(isExpansionInSystemHeader()),
          to(varDecl(unless(isExpansionInSystemHeader())).bind("vardecl")))
          .bind("declrefexpr");
  finder->addMatcher(decl_ref_matcher, this);

  // 5. using functions that are transitively included.
  auto m1 = callExpr(unless(isExpansionInSystemHeader()),
                     callee(functionDecl(unless(isExpansionInSystemHeader()))
                                .bind("functiondecl")))
                .bind("call");
  finder->addMatcher(m1, this);
}

void Callback::run(const ast_matchers::MatchFinder::MatchResult& result) {
  auto context = result.Context;
  auto SM = result.SourceManager;
  if (const auto* external_first_var =
          result.Nodes.getNodeAs<VarDecl>("externalvar")) {
    if (misra::libtooling_utils::IsInSystemHeader(external_first_var, context))
      return;

    if (!external_first_var->isFirstDecl()) return;

    CheckInMainFile(external_first_var, SM, results_list_, main_filename,
                    header_files);
  } else if (const auto* function_only_decl =
                 result.Nodes.getNodeAs<FunctionDecl>("function_only_decl")) {
    if (misra::libtooling_utils::IsInSystemHeader(function_only_decl, context))
      return;

    if (function_only_decl->hasBody() || !function_only_decl->isFirstDecl())
      return;

    CheckInMainFile(function_only_decl, SM, results_list_, main_filename,
                    header_files);
  } else if (const auto* decl = result.Nodes.getNodeAs<Decl>("decl")) {
    if (misra::libtooling_utils::IsInSystemHeader(decl, context)) return;

    std::string filename = misra::libtooling_utils::GetFilename(decl, SM);
    // Report errors in main file only.
    if (filename != main_filename) return;

    auto line = misra::libtooling_utils::GetLine(decl, SM);
    if (const auto* type_decl = result.Nodes.getNodeAs<Decl>("type")) {
      CheckTransitivelyIncluded(filename, line, type_decl, SM, results_list_,
                                main_filename, header_files);
    }
  } else if (const auto* decl_ref_expr =
                 result.Nodes.getNodeAs<DeclRefExpr>("declrefexpr")) {
    if (misra::libtooling_utils::IsInSystemHeader(decl_ref_expr, context))
      return;

    std::string filename =
        misra::libtooling_utils::GetFilename(decl_ref_expr, SM);
    if (filename != main_filename) return;

    auto line = misra::libtooling_utils::GetLine(decl_ref_expr, SM);
    if (const auto* decl = result.Nodes.getNodeAs<VarDecl>("vardecl")) {
      CheckTransitivelyIncluded(filename, line, decl, SM, results_list_,
                                main_filename, header_files);
    }
  } else if (const auto* call_expr = result.Nodes.getNodeAs<CallExpr>("call")) {
    if (misra::libtooling_utils::IsInSystemHeader(call_expr, context)) return;

    std::string filename = misra::libtooling_utils::GetFilename(call_expr, SM);
    if (filename != main_filename) return;

    auto line = misra::libtooling_utils::GetLine(call_expr, SM);
    if (const auto* decl = result.Nodes.getNodeAs<Decl>("functiondecl")) {
      CheckTransitivelyIncluded(filename, line, decl, SM, results_list_,
                                main_filename, header_files);
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

}  // namespace libtooling
}  // namespace g1149
}  // namespace googlecpp
