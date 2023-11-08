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

#include "autosar/rule_A8_5_4/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang::ast_matchers;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "If a class has a user-declared constructor that takes a parameter of type std::initializer_list, then it shall be the only onstructor apart from special member function constructors.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A8_5_4 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      cxxRecordDecl(isDefinition(), hasDescendant(cxxConstructorDecl()))
          .bind("class"),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXRecordDecl* classDecl =
      result.Nodes.getNodeAs<CXXRecordDecl>("class");
  if (misra::libtooling_utils::IsInSystemHeader(classDecl, result.Context)) {
    return;
  }
  int initListCtorIndex = -1;

  // check whether there is a constructor that takes a parameter of type
  // std::initializer_list in this matched class
  int i = 0;
  for (const auto* constructor : classDecl->ctors()) {
    const auto params = constructor->parameters();
    if (params.size() == 1) {
      const ParmVarDecl* param = params.front();
      QualType paramType = param->getType();
      std::basic_string<char> typePrefix = "initializer_list";
      if (paramType.getAsString().find(typePrefix) != std::string::npos) {
        initListCtorIndex = i;
        break;
      }
    }
    i++;
  }

  if (initListCtorIndex >= 0) {
    // there is a initializer_list constructor
    // report error when meeting other constructors (except special member
    // function constructors)
    i = 0;
    for (const auto* constructor : classDecl->ctors()) {
      if (!constructor->isDefaultConstructor() &&
          !constructor->isCopyConstructor() &&
          !constructor->isMoveConstructor() && i != initListCtorIndex) {
        std::string path = misra::libtooling_utils::GetFilename(
            constructor, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(constructor, result.SourceManager);
        ReportError(path, line_number, results_list_);
      }
      i++;
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
}  // namespace rule_A8_5_4
}  // namespace autosar
