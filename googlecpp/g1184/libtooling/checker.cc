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

#include "googlecpp/g1184/libtooling/checker.h"

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
      "Within each section, prefer grouping similar kinds of declarations "
      "together";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1184 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxRecordDecl(unless(isExpansionInSystemHeader()),
                                     unless(isImplicit()), isClass())
                           .bind("record_decl"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const auto* record_decl =
        result.Nodes.getNodeAs<CXXRecordDecl>("record_decl");
    auto const SM = result.SourceManager;
    auto currentAccess = AS_private;
    decls_by_access_ = {
        {AS_private, {}},
        {AS_protected, {}},
        {AS_public, {}},
    };
    for (auto const& decl : record_decl->decls()) {
      if (decl->isImplicit()) continue;
      if (const auto& access_spec_decl = dyn_cast<AccessSpecDecl>(decl)) {
        currentAccess = access_spec_decl->getAccess();
        continue;
      }
      auto& decls = decls_by_access_[currentAccess];
      auto const line = misra::libtooling_utils::GetLine(decl, SM);
      if (dyn_cast<TypedefDecl>(decl) || dyn_cast<TypeAliasDecl>(decl) ||
          dyn_cast<UsingDecl>(decl) || dyn_cast<CXXRecordDecl>(decl) ||
          dyn_cast<EnumDecl>(decl) ||
          (dyn_cast<FriendDecl>(decl) &&
           dyn_cast<FriendDecl>(decl)->getFriendType())) {
        decls.push_back({line, TypesAndTypeAliases});
      } else if (dyn_cast<VarDecl>(decl) &&
                 dyn_cast<VarDecl>(decl)->getType().isConstQualified()) {
        decls.push_back({line, StaticConstants});
      } else if (dyn_cast<CXXConstructorDecl>(decl) ||
                 (dyn_cast<CXXMethodDecl>(decl) &&
                  dyn_cast<CXXMethodDecl>(decl)->getDeclName().getAsString() ==
                      "operator=")) {
        decls.push_back({line, ConstructorsAndAssignmentOperators});
      } else if (dyn_cast<CXXDestructorDecl>(decl)) {
        decls.push_back({line, Destructor});
      } else if (const auto cxxMethodDecl = dyn_cast<CXXMethodDecl>(decl)) {
        decls.push_back({line, AllOtherFunctions,
                         // set possible_factory_func
                         cxxMethodDecl->getReturnType()->isPointerType()});
      } else if (dyn_cast<FieldDecl>(decl) || dyn_cast<VarDecl>(decl)) {
        decls.push_back({line, DataMembers});
      } else {
        // unknown kind of decl
        ReportError(misra::libtooling_utils::GetFilename(record_decl, SM),
                    misra::libtooling_utils::GetLine(record_decl, SM),
                    results_list_);
      }
    }
    for (auto const& decls : decls_by_access_) {
      auto expected_kind = TypesAndTypeAliases;
      for (auto const& decl : decls.second) {
        if (decl.kind < expected_kind)
          ReportError(misra::libtooling_utils::GetFilename(record_decl, SM),
                      decl.line_number, results_list_);
        if (decl.kind == AllOtherFunctions && decl.possible_factory_func &&
            expected_kind <= FactoryFunctions)
          expected_kind = FactoryFunctions;
        else
          expected_kind = decl.kind;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  enum Kind {
    TypesAndTypeAliases,
    StaticConstants,
    FactoryFunctions,
    ConstructorsAndAssignmentOperators,
    Destructor,
    AllOtherFunctions,
    DataMembers,
  };
  struct DeclInfo {
    int line_number;
    Kind kind;
    bool possible_factory_func = false;
  };
  std::unordered_map<clang::AccessSpecifier, std::vector<DeclInfo>>
      decls_by_access_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1184
}  // namespace googlecpp
