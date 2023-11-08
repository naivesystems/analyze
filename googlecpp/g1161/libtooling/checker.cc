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

#include "googlecpp/g1161/libtooling/checker.h"

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
      "When definitions in a .cc file do not need to be referenced outside that file, give them internal linkage by placing them in an unnamed namespace or declaring them static";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1161 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
  // Assume all global variables has distinct qualified name
  llvm::StringMap<llvm::DenseSet<TranslationUnitDecl*>> qname_to_TU_map;
  llvm::StringMap<pair<string, int>> qname_to_first_decl_file_line_map;

 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(tagDecl(unless(isImplicit())).bind("namedDecl"), this);
    finder->addMatcher(varDecl(unless(isImplicit())).bind("namedDecl"), this);
    finder->addMatcher(functionDecl(unless(isImplicit())).bind("namedDecl"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const auto* decl = result.Nodes.getNodeAs<NamedDecl>("namedDecl")) {
      if (misra::libtooling_utils::IsInSystemHeader(decl, result.Context))
        return;
      if (decl->isExternallyVisible()) {
        string qname = decl->getQualifiedNameAsString();
        string filename =
            misra::libtooling_utils::GetFilename(decl, result.SourceManager);
        // Record the first occurence for report location
        if (!qname_to_first_decl_file_line_map.count(qname))
          qname_to_first_decl_file_line_map[qname] = {
              filename,
              misra::libtooling_utils::GetLine(decl, result.SourceManager)};
        // Count the occurence across TUs
        qname_to_TU_map[qname].insert(result.Context->getTranslationUnitDecl());
      }
    }
  }

  void PostRun() {
    for (const auto& kv : qname_to_TU_map) {
      const auto& qname = kv.first();
      const auto& filenameset = kv.second;
      if (filenameset.size() == 1) {
        string filename;
        int line;
        std::tie(filename, line) = qname_to_first_decl_file_line_map[qname];
        StringRef fname(filename);
        if (!fname.endswith(".h") && !fname.endswith(".hpp"))
          ReportError(filename, line, results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
void Checker::PostRun() { callback_->PostRun(); }
}  // namespace libtooling
}  // namespace g1161
}  // namespace googlecpp
