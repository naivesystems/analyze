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

#include "autosar/rule_A12_7_1/libtooling/checker.h"

#include <glog/logging.h>

#include <unordered_set>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kCXXConstructorDeclString[] = "cxxMethodDecl";
constexpr char kErrorMessage[] =
    "If the behavior of a user-defined special member function is identical to implicitly defind special member function, then it shall be defined \"=default\" or be left undefined.";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            kErrorMessage);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", kErrorMessage, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A12_7_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(cxxConstructorDecl().bind(kCXXConstructorDeclString),
                     this);
  finder->addMatcher(cxxDestructorDecl().bind(kCXXConstructorDeclString), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXMethodDecl* cxx_method =
      result.Nodes.getNodeAs<CXXMethodDecl>(kCXXConstructorDeclString);
  std::string path =
      misra::libtooling_utils::GetFilename(cxx_method, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(cxx_method, result.SourceManager);
  if (misra::libtooling_utils::IsInSystemHeader(cxx_method, result.Context) ||
      cxx_method->isImplicit() || cxx_method->isExplicitlyDefaulted()) {
    return;
  }
  if (const CXXConstructorDecl* cxx_constructor_decl =
          dyn_cast<CXXConstructorDecl>(cxx_method)) {
    if (cxx_constructor_decl->isDefaultConstructor()) {
      if (cxx_constructor_decl->inits().empty()) {
        if (cxx_constructor_decl->hasBody() &&
            cxx_constructor_decl->getBody()->children().empty()) {
          ReportError(path, line_number, results_list_);
        }
      }
    } else if (cxx_constructor_decl->isCopyOrMoveConstructor()) {
      std::unordered_set<const FieldDecl*> fields = {};
      for (const FieldDecl* field :
           cxx_constructor_decl->getParent()->fields()) {
        fields.insert(field);
      }
      for (const CXXCtorInitializer* init : cxx_constructor_decl->inits()) {
        if (init->isMemberInitializer()) {
          fields.erase(init->getMember());
        }
      }
      if (fields.empty()) {
        if (cxx_constructor_decl->hasBody() &&
            cxx_constructor_decl->getBody()->children().empty()) {
          ReportError(path, line_number, results_list_);
        }
      }
    }
  } else if (const CXXDestructorDecl* cxx_destructor_decl =
                 dyn_cast<CXXDestructorDecl>(cxx_method)) {
    if (cxx_destructor_decl->hasBody() &&
        cxx_destructor_decl->getBody()->children().empty()) {
      ReportError(path, line_number, results_list_);
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
}  // namespace rule_A12_7_1
}  // namespace autosar
