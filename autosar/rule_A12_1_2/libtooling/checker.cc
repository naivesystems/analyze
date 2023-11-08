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

#include "autosar/rule_A12_1_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kCXXConstructorDeclString[] = "cxxConstructorDeclString";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Both NSDMI and a non_static member initializer in a constructor shall not be used in the same type.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A12_1_2 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      cxxConstructorDecl(unless(isImplicit()), unless(isCopyConstructor()),
                         unless(isMoveConstructor()))
          .bind(kCXXConstructorDeclString),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXConstructorDecl* cxx_constructor_decl =
      result.Nodes.getNodeAs<CXXConstructorDecl>(kCXXConstructorDeclString);
  const CXXRecordDecl* cxx_record_decl = cxx_constructor_decl->getParent();
  std::string path = misra::libtooling_utils::GetFilename(cxx_constructor_decl,
                                                          result.SourceManager);
  int line_number = misra::libtooling_utils::GetLine(cxx_constructor_decl,
                                                     result.SourceManager);
  // The method checks whether any member variable is initialized by NSDMI. If
  // so, all members of this class should be initialized in the same way.
  bool isNSDMI = false;
  for (const Decl* decl : cxx_record_decl->decls()) {
    if (const FieldDecl* field_decl = dyn_cast<FieldDecl>(decl)) {
      if (field_decl->hasInClassInitializer()) {
        isNSDMI = true;
        break;
      }
    }
  }
  // If not, it's impossible to have conflicts on the initialization way.
  if (!isNSDMI) {
    return;
  }
  // Then it checks the initialization list of the constructor. For all members
  // should initialized by NSDMI, so if any member is in the list explicitly,
  // report the error.
  for (CXXCtorInitializer* init : cxx_constructor_decl->inits()) {
    if (init->isWritten()) {
      ReportError(path, line_number, results_list_);
      return;
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
}  // namespace rule_A12_1_2
}  // namespace autosar
