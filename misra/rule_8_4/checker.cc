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

#include "misra/rule_8_4/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

string GetDeclName(const NamedDecl* decl) { return decl->getNameAsString(); }

void ReportNoFunctionDeclError(string name, string loc, std::string path,
                               int line_number, ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0511][misra-c2012-8.4]: violation of misra-c2012-8.4\n"
      "Missing function declaration\n"
      "function name: %s\n"
      "location: %s",
      name, loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_4_NO_FUNC_DECL_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

void ReportNoVariableDeclError(string name, string loc, std::string path,
                               int line_number, ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0511][misra-c2012-8.4]: violation of misra-c2012-8.4\n"
      "Missing variable declaration\n"
      "variable name: %s\n"
      "location: %s",
      name, loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_4_NO_VAR_DECL_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

void ReportFunctionParamNotMatchError(string name, string loc, std::string path,
                                      int line_number,
                                      ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0511][misra-c2012-8.4]: violation of misra-c2012-8.4\n"
      "Function declaration and definition type do not match\n"
      "function name: %s\n"
      "definition location: %s",
      name, loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_4_FUNC_PARM_NOT_MATCH_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_8_4 {

class ExternalVDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(varDecl().bind("vd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    ASTContext* context = result.Context;
    if (libtooling_utils::IsInSystemHeader(vd, context)) {
      return;
    }

    std::string path = libtooling_utils::GetFilename(vd, result.SourceManager);
    int line_number = libtooling_utils::GetLine(vd, result.SourceManager);

    if (!vd->hasExternalFormalLinkage()) {
      return;
    }
    if (vd->isThisDeclarationADefinition() ==
        clang::VarDecl::DefinitionKind::DeclarationOnly) {
      return;
    }
    if (vd->hasInit() && vd->getPreviousDecl() == NULL) {
      ReportNoVariableDeclError(
          GetDeclName(vd),
          libtooling_utils::GetLocation(vd, result.SourceManager), path,
          line_number, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

class ExternalFDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    ASTContext* context = result.Context;
    // skip system header
    if (libtooling_utils::IsInSystemHeader(fd, context)) {
      return;
    }

    std::string path = libtooling_utils::GetFilename(fd, result.SourceManager);
    int line_number = libtooling_utils::GetLine(fd, result.SourceManager);

    if (fd->isMain()) {
      return;
    }
    if (!fd->hasExternalFormalLinkage()) {
      return;
    }
    if (fd->isInvalidDecl()) {
      ReportFunctionParamNotMatchError(
          GetDeclName(fd),
          libtooling_utils::GetLocation(fd, result.SourceManager), path,
          line_number, results_list_);
      return;
    }
    if (fd->isThisDeclarationADefinition() && fd->getPreviousDecl() == NULL) {
      ReportNoFunctionDeclError(
          GetDeclName(fd),
          libtooling_utils::GetLocation(fd, result.SourceManager), path,
          line_number, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  vd_callback_ = new ExternalVDCallback;
  vd_callback_->Init(results_list_, &finder_);
  fd_callback_ = new ExternalFDCallback;
  fd_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_8_4
}  // namespace misra
