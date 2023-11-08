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

#include "autosar/rule_A17_6_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Non-standard entities shall not be added to standard namespaces.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A17_6_1 {
namespace libtooling {
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto in_std_not_hash = allOf(
        hasAncestor(namespaceDecl(hasName("std"))),
        unless(hasAncestor(classTemplateSpecializationDecl(hasName("hash")))));
    finder->addMatcher(classTemplateSpecializationDecl(
                           hasAnyTemplateArgument(
                               refersToType(hasDeclaration(cxxRecordDecl()))),
                           in_std_not_hash, unless(hasName("hash")))
                           .bind("decl"),
                       this);
    finder->addMatcher(typeAliasDecl(in_std_not_hash).bind("alias_decl_in_std"),
                       this);
    finder->addMatcher(functionDecl(in_std_not_hash).bind("func_decl_in_std"),
                       this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const ClassTemplateSpecializationDecl* decl =
        result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("decl");
    if (decl) {
      const SourceLocation loc = decl->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        ReportError(
            misra::libtooling_utils::GetFilename(decl, result.SourceManager),
            misra::libtooling_utils::GetLine(decl, result.SourceManager),
            results_list_);
      }
    }
    const TypeAliasDecl* alias_decl_in_std =
        result.Nodes.getNodeAs<TypeAliasDecl>("alias_decl_in_std");
    if (alias_decl_in_std) {
      const SourceLocation loc = alias_decl_in_std->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        ReportError(misra::libtooling_utils::GetFilename(alias_decl_in_std,
                                                         result.SourceManager),
                    misra::libtooling_utils::GetLine(alias_decl_in_std,
                                                     result.SourceManager),
                    results_list_);
      }
    }
    const FunctionDecl* func_decl_in_std =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl_in_std");
    if (func_decl_in_std) {
      const SourceLocation loc = func_decl_in_std->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        ReportError(misra::libtooling_utils::GetFilename(func_decl_in_std,
                                                         result.SourceManager),
                    misra::libtooling_utils::GetLine(func_decl_in_std,
                                                     result.SourceManager),
                    results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A17_6_1
}  // namespace autosar
