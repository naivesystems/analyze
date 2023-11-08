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

#include "misra/rule_8_5/checker.h"

#include <glog/logging.h>

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

bool IsInCfile(SourceManager* source_manager, const Decl* decl) {
  auto filename = misra::libtooling_utils::GetFilename(decl, source_manager);
  return (filename.back() == 'c');
}

void ReportExternalVDInCfileError(string name, string loc, string path,
                                  int line_number, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C0510][misra-c2012-8.5]: External object shall be declared in one header file.\n"
      "Name: %s\n"
      "Location: %s",
      name, loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_5_EXT_VD_IN_C_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

void ReportExternalFDInCfileError(string name, string loc, string path,
                                  int line_number, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C0510][misra-c2012-8.5]: External function shall be declared in one header file.\n"
      "Name: %s\n"
      "Location: %s",
      name, loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_5_EXT_FD_IN_C_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

void ReportExternalVDDuplicationError(string name, string loc, string other_loc,
                                      string path, int line_number,
                                      ResultsList* results_list) {
  if (loc == other_loc) return;
  string error_message = absl::StrFormat(
      "[C0510][misra-c2012-8.5]: External object shall be declared once in one and only one file.\n"
      "Name: %s\n"
      "Location: %s\n"
      "Other Location: %s",
      name, loc, other_loc);
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_8_5_EXT_VD_DUP_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

void ReportExternalFDDuplicationError(string name, string loc, string other_loc,
                                      string path, int line_number,
                                      ResultsList* results_list) {
  if (loc == other_loc) return;
  string error_message = absl::StrFormat(
      "[C0510][misra-c2012-8.5]: External function shall be declared once in one and only one file.\n"
      "Name: %s\n"
      "Location: %s\n"
      "Other Location: %s",
      name, loc, other_loc);
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_8_5_EXT_FD_DUP_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_8_5 {

class VDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(varDecl(hasExternalFormalLinkage()).bind("vd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    const SourceLocation loc = vd->getLocation();
    if (loc.isInvalid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    if (vd->isThisDeclarationADefinition()) return;
    string path = libtooling_utils::GetFilename(vd, result.SourceManager);
    int line_number = result.Context->getSourceManager().getSpellingLineNumber(
        vd->getLocation());
    if (IsInCfile(result.SourceManager, vd))
      ReportExternalVDInCfileError(
          vd->getNameAsString(),
          libtooling_utils::GetLocation(vd, result.SourceManager), path,
          line_number, results_list_);
    const VarDecl* prevd = vd->getPreviousDecl();
    if (prevd)
      ReportExternalVDDuplicationError(
          vd->getNameAsString(),
          libtooling_utils::GetLocation(vd, result.SourceManager),
          libtooling_utils::GetLocation(prevd, result.SourceManager), path,
          line_number, results_list_);
  }

 private:
  ResultsList* results_list_;
};

class FDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    auto source_manager = result.SourceManager;
    if (source_manager->isInSystemHeader(fd->getLocation())) return;
    if (fd->isThisDeclarationADefinition()) return;
    string path = libtooling_utils::GetFilename(fd, result.SourceManager);
    int line_number = result.Context->getSourceManager().getSpellingLineNumber(
        fd->getLocation());
    if (IsInCfile(result.SourceManager, fd))
      ReportExternalFDInCfileError(
          fd->getNameAsString(),
          libtooling_utils::GetLocation(fd, result.SourceManager), path,
          line_number, results_list_);
    const FunctionDecl* prefd = fd->getPreviousDecl();
    if (prefd)
      ReportExternalFDDuplicationError(
          fd->getNameAsString(),
          libtooling_utils::GetLocation(fd, result.SourceManager),
          libtooling_utils::GetLocation(prefd, result.SourceManager), path,
          line_number, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  // TODO: fix leaks when necessary
  results_list_ = results_list;
  fdcallback_ = new FDCallback;
  fdcallback_->Init(results_list_, &finder_);
  vdcallback_ = new VDCallback;
  vdcallback_->Init(results_list_, &finder_);
}

}  // namespace rule_8_5
}  // namespace misra
