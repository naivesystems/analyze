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

#include "autosar/rule_A12_4_1/libtooling/checker.h"

#include <glog/logging.h>

#include <unordered_set>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kCXXRecordDeclString[] = "cxxRecordDecl";
constexpr char kErrorMessage[] =
    "Destructor of a base class shall be public virtual, public override or protected non-virtual.";
std::unordered_set<const CXXDestructorDecl*> checked_destructors = {};

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            kErrorMessage);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", kErrorMessage, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A12_4_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      cxxRecordDecl(unless(isImplicit())).bind(kCXXRecordDeclString), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXRecordDecl* cxx_record_decl =
      result.Nodes.getNodeAs<CXXRecordDecl>(kCXXRecordDeclString);
  if (misra::libtooling_utils::IsInSystemHeader(cxx_record_decl,
                                                result.Context) ||
      !cxx_record_decl->getDefinition()) {
    return;
  }
  for (const CXXBaseSpecifier& base : cxx_record_decl->bases()) {
    if (const CXXRecordDecl* base_decl = base.getType()->getAsCXXRecordDecl()) {
      for (const CXXMethodDecl* method : base_decl->methods()) {
        if (const CXXDestructorDecl* destructor =
                dyn_cast<CXXDestructorDecl>(method)) {
          if ((destructor->getAccess() == AS_public &&
               (destructor->isVirtual() ||
                destructor->getAttr<OverrideAttr>())) ||
              (destructor->getAccess() == AS_protected &&
               !destructor->isVirtual()) ||
              checked_destructors.find(destructor) !=
                  checked_destructors.end()) {
            continue;
          } else {
            std::string path = misra::libtooling_utils::GetFilename(
                destructor, result.SourceManager);
            int line_number = misra::libtooling_utils::GetLine(
                destructor, result.SourceManager);
            checked_destructors.insert(destructor);
            ReportError(path, line_number, results_list_);
          }
        }
      }
    }
  }
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A12_4_1
}  // namespace autosar
