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

#include "misra/rule_5_1/checker.h"

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

bool ContainNonAsciiChar(std::string s) {
  for (unsigned char c : s) {
    if (c >= 128) {
      return true;
    }
  }
  return false;
}

string CalcDistinctName(string name, int prefix_length, bool case_sensitive) {
  name = name.substr(0, prefix_length);
  if (!case_sensitive) {
    for (char& c : name) {
      c = tolower(c);
    }
  }
  return name;
}

void ReportNonAsciiError(string kind, string name, string path, int line_number,
                         ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1109][misra-c2012-5.1]: contain non-ASCII characters\n"
      "%s: %s",
      kind, name);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_5_1_NON_ASCII_ERROR);
  pb_result->set_kind(kind);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

void ReportDistinctError(string kind, string name, string conflict_name,
                         string loc, string other_loc, string path,
                         int line_number, ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1109][misra-c2012-5.1]: violation of misra-c2012-5.1\n"
      "%s: %s\n"
      "First identifier location: %s\n"
      "Duplicated identifier location: %s",
      kind, name, loc, other_loc);
  vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_5_1_DISTINCT_ERROR);
  pb_result->set_kind(kind);
  pb_result->set_name(name);
  pb_result->set_external_message(conflict_name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_5_1 {

class ExternalVDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int prefix_length, bool case_sensitive, bool implicit_decl,
            unordered_map<string, identifier>* name_locations,
            ResultsList* results_list, MatchFinder* finder) {
    prefix_length_ = prefix_length;
    case_sensitive_ = case_sensitive;
    name_locations_ = name_locations;
    results_list_ = results_list;
    finder->addMatcher(varDecl().bind("vd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    if (!vd->hasExternalFormalLinkage() || vd->hasExternalStorage() ||
        vd->isWeak()) {
      return;
    }
    // skip system header
    if (libtooling_utils::IsInSystemHeader(vd, result.Context)) {
      return;
    }
    string name = vd->getNameAsString();
    string path = libtooling_utils::GetFilename(vd, result.SourceManager);
    int line_number = libtooling_utils::GetLine(vd, result.SourceManager);
    string loc = libtooling_utils::GetLocation(vd, result.SourceManager);
    if (ContainNonAsciiChar(name)) {
      ReportNonAsciiError("Variable", name, path, line_number, results_list_);
      return;
    }
    if (!implicit_decl_ && vd->isImplicit()) {
      return;
    }
    identifier id = {name, loc, vd->isImplicit()};
    string distinct_name =
        CalcDistinctName(name, prefix_length_, case_sensitive_);
    auto it = name_locations_->find(distinct_name);
    if (it == name_locations_->end()) {
      name_locations_->emplace(distinct_name, id);
    } else {
      identifier conflict_id = it->second;
      // Two implicit declarations are regarded as the same identifiers
      if (conflict_id.is_implicit && id.is_implicit &&
          id.name == conflict_id.name) {
        return;
      }
      if (loc != conflict_id.loc) {
        ReportDistinctError("Variable", name, conflict_id.name, loc,
                            conflict_id.loc, path, line_number, results_list_);
      }
    }
  }

 private:
  int prefix_length_;
  bool case_sensitive_;
  bool implicit_decl_;
  unordered_map<string, identifier>* name_locations_;
  ResultsList* results_list_;
};

class ExternalFDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int prefix_length, bool case_sensitive, bool implicit_decl,
            unordered_map<string, identifier>* name_locations,
            ResultsList* results_list, MatchFinder* finder) {
    prefix_length_ = prefix_length;
    case_sensitive_ = case_sensitive;
    implicit_decl_ = implicit_decl;
    name_locations_ = name_locations;
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("fd"), this);
    finder->addMatcher(callExpr(unless(isExpansionInSystemHeader())).bind("ce"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("ce");
    string name, path, loc, distinct_name;
    int line_number;
    identifier id;
    if (ce && implicit_decl_) {
      if (!ce->getCalleeDecl() || !ce->getCalleeDecl()->isImplicit()) {
        return;
      }
      name = ce->getCalleeDecl()->getAsFunction()->getNameAsString();
      path = libtooling_utils::GetFilename(ce, result.SourceManager);
      line_number = libtooling_utils::GetLine(ce, result.SourceManager);
      loc = libtooling_utils::GetLocation(ce, result.SourceManager);
      if (ContainNonAsciiChar(name)) {
        ReportNonAsciiError("Function", name, path, line_number, results_list_);
        return;
      }
      distinct_name = CalcDistinctName(name, prefix_length_, case_sensitive_);
      id = {name, loc, true};
    }

    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    if (fd) {
      if (!fd->hasExternalFormalLinkage() || fd->isWeak()) {
        return;
      }
      // skip system header
      if (libtooling_utils::IsInSystemHeader(fd, result.Context)) {
        return;
      }
      // skip function declarations in header
      if (!fd->doesThisDeclarationHaveABody()) {
        return;
      }
      name = fd->getNameAsString();
      path = libtooling_utils::GetFilename(fd, result.SourceManager);
      line_number = libtooling_utils::GetLine(fd, result.SourceManager);
      loc = libtooling_utils::GetLocation(fd, result.SourceManager);
      if (ContainNonAsciiChar(name)) {
        ReportNonAsciiError("Function", name, path, line_number, results_list_);
        return;
      }
      distinct_name = CalcDistinctName(name, prefix_length_, case_sensitive_);
      id = {name, loc, false};
    }
    auto it = name_locations_->find(distinct_name);
    if (it == name_locations_->end()) {
      name_locations_->emplace(distinct_name, id);
    } else {
      identifier conflict_id = it->second;
      // Two implicit declarations are regarded as the same identifiers
      if (conflict_id.is_implicit && id.is_implicit &&
          id.name == conflict_id.name) {
        return;
      }
      if (loc != conflict_id.loc) {
        ReportDistinctError("Function", name, conflict_id.name, loc,
                            conflict_id.loc, path, line_number, results_list_);
      }
    }
  }

 private:
  int prefix_length_;
  bool case_sensitive_;
  bool implicit_decl_;
  unordered_map<string, identifier>* name_locations_;
  ResultsList* results_list_;
};

void Checker::Init(int prefix_length, bool case_sensitive, bool implicit_decl,
                   ResultsList* results_list) {
  // TODO: fix leaks when necessary
  results_list_ = results_list;
  external_vd_callback_ = new ExternalVDCallback;
  external_vd_callback_->Init(prefix_length, case_sensitive, implicit_decl,
                              &name_locations_, results_list_, &finder_);
  external_fd_callback_ = new ExternalFDCallback;
  external_fd_callback_->Init(prefix_length, case_sensitive, implicit_decl,
                              &name_locations_, results_list_, &finder_);
}

}  // namespace rule_5_1
}  // namespace misra
