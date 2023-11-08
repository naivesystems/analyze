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

#include "googlecpp/g1174/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Limit the use of protected to those member functions that might need to be accessed from subclasses";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1174 {
namespace libtooling {

// const CXXRecordDecl*, const CXXMethodDecl*
unordered_map<std::string, unordered_set<std::string>> protected_decl_map;
// const CXXRecordDecl*, const CXXMethodDecl*
set<pair<std::string, std::string>> protected_use_set;
// const CXXMethodDecl*
unordered_map<std::string, pair<string, int>> protected_name_location;

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    auto protectedFunctionDefinitionMatcher =
        cxxRecordDecl(
            unless(isExpansionInSystemHeader()),
            forEach(cxxMethodDecl(isProtected()).bind("protecteddecl")))
            .bind("classdecled");
    finder->addMatcher(protectedFunctionDefinitionMatcher, this);
    auto protectedFunctionUseMatcher = cxxMethodDecl(
        unless(isExpansionInSystemHeader()),
        hasDeclContext(cxxRecordDecl().bind("classused")),
        hasBody(forEachDescendant(cxxMemberCallExpr(callee(
            cxxMethodDecl(isProtected(), hasDeclContext(cxxRecordDecl().bind(
                                             "callee_decl_class")))
                .bind("method"))))));
    finder->addMatcher(protectedFunctionUseMatcher, this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const auto* protected_decl =
            result.Nodes.getNodeAs<CXXMethodDecl>("protecteddecl")) {
      const auto* class_decled =
          result.Nodes.getNodeAs<CXXRecordDecl>("classdecled");
      // These `if(!...)` must fail because it's in same match.
      if (!class_decled) return;

      auto class_decled_name = class_decled->getQualifiedNameAsString();
      auto protected_decl_name = protected_decl->getQualifiedNameAsString();
      protected_decl_map[class_decled_name].insert(protected_decl_name);
      string path = misra::libtooling_utils::GetFilename(protected_decl,
                                                         result.SourceManager);
      int line_number = misra::libtooling_utils::GetLine(protected_decl,
                                                         result.SourceManager);
      protected_name_location[protected_decl_name] = {path, line_number};
    }
    if (const auto* class_decled =
            result.Nodes.getNodeAs<CXXRecordDecl>("callee_decl_class")) {
      const auto* method = result.Nodes.getNodeAs<CXXMethodDecl>("method");
      // These `if(!...)` must fail because it's in same match.
      if (!method) return;

      const auto* class_used =
          result.Nodes.getNodeAs<CXXRecordDecl>("classused");
      // These `if(!...)` must fail because it's in same match.
      if (!class_used) return;

      auto class_used_name = class_used->getQualifiedNameAsString();
      auto class_decled_name = class_decled->getQualifiedNameAsString();
      auto method_name = method->getQualifiedNameAsString();

      // Store call only in derived classes
      if (class_used_name == class_decled_name) return;
      protected_use_set.insert({class_decled_name, method_name});
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Run() const {
  for (auto it : protected_decl_map) {
    auto record = it.first;
    for (auto method : it.second) {
      if (0 == protected_use_set.count({record, method})) {
        string path;
        int line_number;
        tie(path, line_number) = protected_name_location[method];
        ReportError(path, line_number, results_list_);
      }
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1174
}  // namespace googlecpp
