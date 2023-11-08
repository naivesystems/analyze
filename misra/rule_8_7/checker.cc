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

#include "misra/rule_8_7/checker.h"

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

void ReportSingleExternError(string name, misra::rule_8_7::location l,
                             ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0508][misra-c2012-8.7]: violation of misra-c2012-8.7\n"
      "Extern function or variable is only called at one translation unit\n"
      "function name: %s\n"
      "location: %s",
      name, l.loc);
  analyzer::proto::Result* pb_result = AddResultToResultsList(
      results_list, l.path, l.line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_8_7);
  pb_result->set_name(name);
  pb_result->set_other_filename(l.first_decl_path);
  pb_result->set_loc(l.loc);
  pb_result->set_other_loc(l.first_decl_loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_8_7 {

class ExternalVDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder,
            unordered_map<string, vector<location>>* vd_name_locations_) {
    results_list_ = results_list;
    finder->addMatcher(
        declRefExpr(unless(isExpansionInSystemHeader())).bind("dre"), this);
    finder->addMatcher(varDecl(unless(isExpansionInSystemHeader())).bind("vd"),
                       this);
    name_locations_ = vd_name_locations_;
  }

  void run(const MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    const DeclRefExpr* dre = result.Nodes.getNodeAs<DeclRefExpr>("dre");
    string path, loc, name;
    int line_number;
    const Decl* first_decl = nullptr;
    if (dre) {
      const ValueDecl* vald = dre->getDecl();
      if (libtooling_utils::IsInSystemHeader(vald, context)) {
        return;
      }
      if (!vald->hasExternalFormalLinkage()) {
        return;
      }
      path = libtooling_utils::GetFilename(dre, result.SourceManager);
      line_number = libtooling_utils::GetLine(dre, result.SourceManager);
      loc = libtooling_utils::GetLocation(dre, result.SourceManager);
      first_decl = vald->getCanonicalDecl();
      name = GetDeclName(vald);
    }

    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    if (vd) {
      if (!vd->isFileVarDecl() || !vd->hasExternalFormalLinkage()) {
        return;
      }
      if (vd->isThisDeclarationADefinition() ==
          VarDecl::DefinitionKind::DeclarationOnly) {
        // ignore DeclarationOnly case.
        return;
      }
      path = libtooling_utils::GetFilename(vd, result.SourceManager);
      line_number = libtooling_utils::GetLine(vd, result.SourceManager);
      loc = libtooling_utils::GetLocation(vd, result.SourceManager);
      first_decl = vd->getFirstDecl();
      name = GetDeclName(vd);
    }

    while (first_decl) {
      if (first_decl->isFirstDecl()) {
        break;
      }
      first_decl = first_decl->getPreviousDecl();
    }
    if (!first_decl) {
      return;
    }
    string first_decl_path =
        libtooling_utils::GetFilename(first_decl, result.SourceManager);
    string first_decl_loc =
        libtooling_utils::GetLocation(first_decl, result.SourceManager);
    location l = {path, line_number, loc, first_decl_path, first_decl_loc};
    // get the name location key-value pair we stored before
    auto it = name_locations_->find(name);
    if (it != name_locations_->end()) {
      // if the existing key-value pair has the same path as the current
      // function/value call then we should not add current ce to
      // name_locations_
      for (auto path_pair : it->second) {
        if (path_pair.path == path) {
          return;
        }
      }
      it->second.push_back(l);
    } else {
      name_locations_->emplace(make_pair(name, vector<location>{l}));
    }
  }

 private:
  ResultsList* results_list_;
  unordered_map<string, vector<location>>* name_locations_;
};

void Checker::Run() {
  for (auto& name_loc : vd_name_locations_) {
    if (name_loc.second.size() == 1) {
      ReportSingleExternError(name_loc.first, name_loc.second[0],
                              results_list_);
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  vd_callback_ = new ExternalVDCallback;
  vd_callback_->Init(results_list_, &finder_, &vd_name_locations_);
}

}  // namespace rule_8_7
}  // namespace misra
