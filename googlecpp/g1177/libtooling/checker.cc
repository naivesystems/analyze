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

#include "googlecpp/g1177/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Define operators only on your own types. More precisely, define them in the same headers, .cc files, and namespaces as the types they operate on";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace googlecpp {
namespace g1177 {
namespace libtooling {
class CastCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    auto namespace_decl = hasAncestor(namespaceDecl().bind("r_ns"));
    auto cxx_record_decl =
        cxxRecordDecl(optionally(namespace_decl)).bind("record");

    auto matcher =
        functionDecl(
            unless(isExpansionInSystemHeader()),
            optionally(hasAncestor(namespaceDecl().bind("f_ns"))),
            isDefinition(), misra::libtooling_utils::OperatorOverloading(),
            forEachDescendant(parmVarDecl(anyOf(
                hasType(cxx_record_decl), hasType(pointsTo(cxx_record_decl)),
                hasType(references(cxx_record_decl))))))
            .bind("func");
    finder->addMatcher(matcher, this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* sm = result.SourceManager;
    const auto* func_decl = result.Nodes.getNodeAs<FunctionDecl>("func");
    const auto* cxx_record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    const auto* record_ns = result.Nodes.getNodeAs<NamespaceDecl>("r_ns");
    const auto* func_ns = result.Nodes.getNodeAs<NamespaceDecl>("f_ns");
    auto record_file = misra::libtooling_utils::GetFilename(cxx_record, sm);
    auto func_decl_file = misra::libtooling_utils::GetFilename(func_decl, sm);
    if (record_ns == func_ns && record_file == func_decl_file) {
      return;
    }
    ReportError(
        misra::libtooling_utils::GetFilename(func_decl, result.SourceManager),
        misra::libtooling_utils::GetLine(func_decl, result.SourceManager),
        results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new CastCallback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1177
}  // namespace googlecpp
