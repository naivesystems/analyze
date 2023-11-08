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

#include "misra/rule_8_3/checker.h"

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

void ReportInvalidDeclarationError(string name, string path, int line,
                                   ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0512][misra-c2012-8.3]: violation of misra-c2012-8.3 for invalid declaration\n"
      "Name: %s",
      name);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_3_INVALID_DECL_ERROR);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

void ReportError(const FunctionDecl* fd, const MatchFinder::MatchResult& result,
                 string other_loc, ResultsList* results_list,
                 const char* reason) {
  // TODO: two locations, we hold first
  auto name = fd->getNameAsString();
  auto path = misra::libtooling_utils::GetFilename(fd, result.SourceManager);
  auto line = misra::libtooling_utils::GetLine(fd, result.SourceManager);
  auto loc = misra::libtooling_utils::GetLocation(fd, result.SourceManager);
  std::string error_message = absl::StrFormat(
      "[C0512][misra-c2012-8.3]: violation of misra-c2012-8.3 for %s\n"
      "Name: %s\n"
      "Location: %s\n"
      "Other location: %s",
      reason, name, loc, other_loc);
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_8_3_ERROR);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_8_3 {

class FDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    const SourceLocation loc = fd->getLocation();
    if (loc.isInvalid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    if (fd->isInvalidDecl()) {
      ReportInvalidDeclarationError(
          fd->getNameAsString(),
          libtooling_utils::GetFilename(fd, result.SourceManager),
          libtooling_utils::GetLine(fd, result.SourceManager), results_list_);
      return;
    }
    const FunctionDecl* other_fd = fd->getPreviousDecl();
    if (other_fd) {
      CheckAndReport(fd, other_fd, result);
    }
  }

 private:
  ResultsList* results_list_;
  bool CheckAndReport(const FunctionDecl* fd, const FunctionDecl* other_fd,
                      const MatchFinder::MatchResult& result) {
    if (fd->getNumParams() != other_fd->getNumParams()) {
      ReportError(fd, result,
                  libtooling_utils::GetLocation(other_fd, result.SourceManager),
                  results_list_, "param number not match");
    }
    for (unsigned int i = 0; i < fd->getNumParams(); i++) {
      // check for different param var name
      const ParmVarDecl* pd = fd->getParamDecl(i);
      const ParmVarDecl* other_pd = other_fd->getParamDecl(i);
      if (pd->getNameAsString() != other_pd->getNameAsString()) {
        ReportError(
            fd, result,
            libtooling_utils::GetLocation(other_fd, result.SourceManager),
            results_list_, "different param var name");
      }
      // check for different param type name
      QualType type = pd->getOriginalType();
      QualType other_type = other_pd->getOriginalType();
      if (type.getAsString() != other_type.getAsString()) {
        ReportError(
            fd, result,
            libtooling_utils::GetLocation(other_fd, result.SourceManager),
            results_list_, "different param type name");
      }
      // check for different param qualified type
      if (type.getCVRQualifiers() != other_type.getCVRQualifiers()) {
        ReportError(
            fd, result,
            libtooling_utils::GetLocation(other_fd, result.SourceManager),
            results_list_, "different param qualified type");
      }
    }
    return true;
  }
};

void Checker::Init(ResultsList* results_list) {
  // TODO: fix leaks when necessary
  results_list_ = results_list;
  callback_ = new FDCallback;
  callback_->Init(&finder_, results_list);
}

}  // namespace rule_8_3
}  // namespace misra
