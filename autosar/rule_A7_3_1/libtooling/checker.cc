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

#include "autosar/rule_A7_3_1/libtooling/checker.h"

#include <glog/logging.h>

#include <functional>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;
using analyzer::proto::ResultsList;

namespace {
constexpr char kCXXMethodDeclString[] = "cxxMethodDecl";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "All overloads of a function shall be visible from where it is called.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

bool IsOverrideFrom(const CXXMethodDecl* overridden_method,
                    const CXXMethodDecl* base_method) {
  if (overridden_method == base_method) {
    return true;
  }
  for (auto next_method : overridden_method->overridden_methods()) {
    if (IsOverrideFrom(next_method, base_method)) {
      return true;
    }
  }
  return false;
}

bool FindSameNameMethod(
    const CXXMethodDecl* cxx_method_decl, const CXXRecordDecl* cxx_record_decl,
    std::function<bool(const CXXMethodDecl*, const CXXMethodDecl*)> cmp_func) {
  if (!cxx_record_decl) {
    return false;
  }
  for (auto cxx_method_decl_it : cxx_record_decl->methods()) {
    // The checker walks through all methods defined in the record. If the
    // method isn't defined by the record and they are equal, which is
    // determined by `cmp_func`, there exists a method shadow possibly.
    if (cxx_method_decl->getParent() != cxx_record_decl &&
        cmp_func(cxx_method_decl, cxx_method_decl_it)) {
      bool flag = true;
      // To make sure that there is a method shadow, the method needs to visit
      // all its sibling using shadow decl nodes, which represent using
      // statements, and check their target declaration to make sure the
      // shadowed method declaration isn't imported into the record scope.
      for (auto decl : cxx_method_decl->getParent()->decls()) {
        if (const UsingShadowDecl* using_shadow_decl =
                dyn_cast<UsingShadowDecl>(decl)) {
          if (const CXXMethodDecl* shadow_cxx_method_decl =
                  dyn_cast<CXXMethodDecl>(using_shadow_decl->getTargetDecl())) {
            if (shadow_cxx_method_decl == cxx_method_decl_it) {
              flag = false;
              break;
            }
          }
        }
      }
      if (flag) {
        return true;
      }
    }
  }
  for (auto base_cxx_record_decl : cxx_record_decl->bases()) {
    if (FindSameNameMethod(cxx_method_decl,
                           base_cxx_record_decl.getType()->getAsCXXRecordDecl(),
                           cmp_func)) {
      return true;
    }
  }
  return false;
}
}  // namespace

namespace autosar {
namespace rule_A7_3_1 {
namespace libtooling {
void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(cxxMethodDecl().bind(kCXXMethodDeclString), this);
}

// When matching a cxx method decl, it checks its base classes' methods and try
// to find one with the same name. If succeeds, it means that some methods may
// be shadowed by this method, then it walks through all the sibling nodes of
// this method to make sure that the base method isn't imported by using
// statement. If so, report an error.
void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXMethodDecl* cxx_method_decl =
      result.Nodes.getNodeAs<CXXMethodDecl>(kCXXMethodDeclString);
  const CXXRecordDecl* cxx_record_decl = cxx_method_decl->getParent();
  if (!cxx_record_decl || cxx_method_decl->isImplicit()) {
    return;
  }
  std::string path = misra::libtooling_utils::GetFilename(cxx_method_decl,
                                                          result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(cxx_method_decl, result.SourceManager);
  if (cxx_method_decl->isVirtual()) {
    // If it's a virtual function, the checker needs to make sure that one of
    // them is not overridden by another one, becuase this won't lead to a
    // method shadow.
    if (FindSameNameMethod(cxx_method_decl, cxx_record_decl,
                           [](const CXXMethodDecl* child_method,
                              const CXXMethodDecl* parent_method) -> bool {
                             return child_method->getNameAsString() ==
                                        parent_method->getNameAsString() &&
                                    !IsOverrideFrom(child_method,
                                                    parent_method);
                           })) {
      ReportError(path, line_number, results_list_);
    }
  } else {
    // If it's a non-virtual function, the checker just needs to compare the
    // name.
    if (FindSameNameMethod(cxx_method_decl, cxx_record_decl,
                           [](const CXXMethodDecl* first_method,
                              const CXXMethodDecl* second_method) -> bool {
                             return first_method->getNameAsString() ==
                                    second_method->getNameAsString();
                           })) {
      ReportError(path, line_number, results_list_);
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_A7_3_1
}  // namespace autosar
