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

#include "misra/rule_5_9/checker.h"

#include <glog/logging.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string kind, string name, string loc, string other_loc,
                 string path, int line_number, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1101][misra-c2012-5.9]: %s with internal linkage shall be unique\n"
      "Name: %s\n"
      "Location: %s\n"
      "Other location: %s",
      kind, name, loc, other_loc);
  vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_5_9);
  pb_result->set_kind(kind);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_5_9 {

class InternalVDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list,
            unordered_map<string, string>* internal_name_locations,
            unordered_map<string, string>* non_internal_name_locations,
            MatchFinder* finder) {
    results_list_ = results_list;
    internal_name_locations_ = internal_name_locations;
    non_internal_name_locations_ = non_internal_name_locations;
    finder->addMatcher(varDecl(isDefinition()).bind("vd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    // skip system header
    if (libtooling_utils::IsInSystemHeader(vd, result.Context)) {
      return;
    }
    string name = vd->getNameAsString();
    string loc = libtooling_utils::GetLocation(vd, result.SourceManager);
    string path = libtooling_utils::GetFilename(vd, result.SourceManager);
    int line_number = libtooling_utils::GetLine(vd, result.SourceManager);
    auto internal_it = internal_name_locations_->find(name);
    auto non_internal_it = non_internal_name_locations_->find(name);
    if (vd->getLinkageInternal() == InternalLinkage) {
      if (internal_it == internal_name_locations_->end()) {
        internal_name_locations_->emplace(name, loc);
      } else {
        ReportError("variable", name, loc, internal_it->second, path,
                    line_number, results_list_);
      }
      if (non_internal_it != non_internal_name_locations_->end()) {
        ReportError("variable", name, loc, non_internal_it->second, path,
                    line_number, results_list_);
      }
    } else {
      if (internal_it != internal_name_locations_->end()) {
        ReportError("variable", name, loc, internal_it->second, path,
                    line_number, results_list_);
      }
      if (non_internal_it == non_internal_name_locations_->end()) {
        non_internal_name_locations_->emplace(name, loc);
      }
    }
  }

 private:
  unordered_map<string, string>* internal_name_locations_;
  unordered_map<string, string>* non_internal_name_locations_;
  ResultsList* results_list_;
};

class InternalFDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list,
            unordered_map<string, string>* internal_name_locations,
            unordered_map<string, string>* non_internal_name_locations,
            MatchFinder* finder) {
    results_list_ = results_list;
    internal_name_locations_ = internal_name_locations;
    non_internal_name_locations_ = non_internal_name_locations;
    finder->addMatcher(functionDecl(isDefinition()).bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    // skip system header
    if (libtooling_utils::IsInSystemHeader(fd, result.Context)) {
      return;
    }
    string name = fd->getNameAsString();
    string loc = libtooling_utils::GetLocation(fd, result.SourceManager);
    string path = libtooling_utils::GetFilename(fd, result.SourceManager);
    int line_number = libtooling_utils::GetLine(fd, result.SourceManager);
    auto internal_it = internal_name_locations_->find(name);
    auto non_internal_it = non_internal_name_locations_->find(name);
    if (!fd->isInlineSpecified() &&
        fd->getLinkageInternal() == InternalLinkage) {
      if (internal_it != internal_name_locations_->end()) {
        ReportError("function", name, loc, internal_it->second, path,
                    line_number, results_list_);
      } else {
        internal_name_locations_->emplace(name, loc);
      }
      if (non_internal_it != non_internal_name_locations_->end()) {
        ReportError("function", name, loc, non_internal_it->second, path,
                    line_number, results_list_);
      }
    } else {
      if (internal_it != internal_name_locations_->end()) {
        ReportError("function", name, loc, internal_it->second, path,
                    line_number, results_list_);
      }
      if (non_internal_it == non_internal_name_locations_->end()) {
        non_internal_name_locations_->emplace(name, loc);
      }
    }
  }

 private:
  unordered_map<string, string>* internal_name_locations_;
  unordered_map<string, string>* non_internal_name_locations_;
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  // TODO: fix leaks when necessary
  results_list_ = results_list;
  internal_vd_callback_ = new InternalVDCallback;
  internal_vd_callback_->Init(results_list_, &internal_name_locations_,
                              &non_internal_name_locations_, &finder_);
  internal_fd_callback_ = new InternalFDCallback;
  internal_fd_callback_->Init(results_list_, &internal_name_locations_,
                              &non_internal_name_locations_, &finder_);
}

}  // namespace rule_5_9
}  // namespace misra
