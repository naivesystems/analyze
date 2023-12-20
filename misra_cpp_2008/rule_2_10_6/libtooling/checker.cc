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

#include "misra_cpp_2008/rule_2_10_6/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-2.10.6]: 如果一个标识符已经表示了一个类型，那么在同一作用域内，它不得用于表示一个对象或一个函数";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_2_10_6 {
namespace libtooling {

map<string, const NamedDecl*> type_decl_;
map<string, const NamedDecl*> other_decl_;

void CheckTagDeclCallback::Init(ResultsList* results_list,
                                MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(namedDecl().bind("name"), this);
}

void CheckTagDeclCallback::run(const MatchFinder::MatchResult& result) {
  const NamedDecl* named_ = result.Nodes.getNodeAs<NamedDecl>("name");

  if (misra::libtooling_utils::IsInSystemHeader(named_, result.Context)) {
    return;
  }

  string name = named_->getQualifiedNameAsString();
  if (name == "") {
    return;
  }

  if (named_->isImplicit()) {
    return;
  }
  auto found_type = type_decl_.find(name);
  if (found_type != type_decl_.end() && !isa<TypeDecl>(named_) &&
      found_type->second->getDeclContext()) {
    const DeclContext* ctx = found_type->second->getDeclContext();
    if (!ctx->lookup(named_->getDeclName()).empty()) {
      ReportError(
          misra::libtooling_utils::GetFilename(named_, result.SourceManager),
          misra::libtooling_utils::GetLine(named_, result.SourceManager),
          results_list_);
    }
  }

  if (isa<TypeDecl>(named_)) {
    auto found_other = other_decl_.find(name);
    if (found_other != other_decl_.end()) {
      const DeclContext* ctx = found_other->second->getDeclContext();
      if (!ctx->lookup(named_->getDeclName()).empty()) {
        ReportError(misra::libtooling_utils::GetFilename(found_other->second,
                                                         result.SourceManager),
                    misra::libtooling_utils::GetLine(found_other->second,
                                                     result.SourceManager),
                    results_list_);
      }
    }
    type_decl_[name] = named_;
  } else if (isa<FunctionDecl>(named_) || isa<ValueDecl>(named_)) {
    other_decl_[name] = named_;
  }

  return;
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckTagDeclCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_2_10_6
}  // namespace misra_cpp_2008
