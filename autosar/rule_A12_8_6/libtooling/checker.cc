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

#include "autosar/rule_A12_8_6/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/QualTypeNames.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list, string str = "") {
  std::string error_message =
      "Copy and move constructors and copy assignment and move assignment operators shall be declared protected or defined \"=delete\" in base class." +
      str;
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

void InitFinder(MatchFinder* finder,
                ast_matchers::MatchFinder::MatchCallback* callback) {
  finder->addMatcher(
      cxxRecordDecl(isClass(), hasDefinition(), unless(isImplicit()),
                    unless(isExpansionInSystemHeader()))
          .bind("crd"),
      callback);
}

}  // namespace

namespace autosar {
namespace rule_A12_8_6 {
namespace libtooling {

class CollectBasesCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  CollectBasesCallback(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}

  std::set<string> GetBases() { return bases_; }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXRecordDecl* crd = result.Nodes.getNodeAs<CXXRecordDecl>("crd");
    if (crd)
      for (const auto base : crd->bases()) {
        bases_.insert(TypeName::getFullyQualifiedName(
            base.getType(), *result.Context,
            PrintingPolicy(result.Context->getLangOpts()), true));
      }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::set<string> bases_{};
  friend class Checker;
};

class CheckBasesCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  CheckBasesCallback(analyzer::proto::ResultsList* results_list,
                     std::set<string> bases)
      : results_list_(results_list), bases_(bases) {}

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXRecordDecl* crd = result.Nodes.getNodeAs<CXXRecordDecl>("crd");
    QualType qt = result.Context->getTypeDeclType(crd);
    string type = TypeName::getFullyQualifiedName(
        qt, *result.Context, PrintingPolicy(result.Context->getLangOpts()),
        true);
    if (crd && bases_.find(type) != bases_.end()) {
      for (const CXXMethodDecl* cmd : crd->methods()) {
        if (cmd->getAccess() == clang::AS_protected || cmd->isDeleted()) return;
        const CXXConstructorDecl* ccd = dyn_cast<CXXConstructorDecl>(cmd);
        if ((ccd && ccd->isCopyOrMoveConstructor()) ||
            cmd->isCopyAssignmentOperator() || cmd->isMoveAssignmentOperator())
          ReportError(
              misra::libtooling_utils::GetFilename(cmd, result.SourceManager),
              misra::libtooling_utils::GetLine(cmd, result.SourceManager),
              results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  const std::set<string> bases_;
};

void Checker::InitCollectBasesCallback(
    analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback1_ = new CollectBasesCallback{results_list};
  InitFinder(&finder1_, callback1_);
}

void Checker::InitCheckBasesCallback() {
  callback2_ = new CheckBasesCallback{results_list_, callback1_->GetBases()};
  InitFinder(&finder2_, callback2_);
}

}  // namespace libtooling
}  // namespace rule_A12_8_6
}  // namespace autosar
