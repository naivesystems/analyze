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

#include "autosar/rule_A2_10_4/libtooling/checker.h"

#include <glog/logging.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;
using analyzer::proto::ResultsList;

namespace {
constexpr char kStaticDeclString[] = "staticDecl";
constexpr char kNamedDeclString[] = "namedDeclString";
void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "The identifier name of a non-member object with static storage duration or static function shall not be reused within a namespace.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A2_10_4 {
namespace libtooling {
void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(varDecl(isStaticStorageClass(), hasParent(namespaceDecl()))
                         .bind(kStaticDeclString),
                     this);
  finder->addMatcher(
      functionDecl(hasParent(namespaceDecl())).bind(kStaticDeclString), this);
  finder->addMatcher(namedDecl().bind(kNamedDeclString), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const ValueDecl* value_decl =
      result.Nodes.getNodeAs<ValueDecl>(kStaticDeclString);
  const NamedDecl* named_decl =
      result.Nodes.getNodeAs<NamedDecl>(kNamedDeclString);
  if (value_decl != nullptr) {
    if (isa<FunctionDecl>(value_decl) &&
        !cast<FunctionDecl>(value_decl)->isStatic()) {
      return;
    }
    const DeclContext* ctx_decl = value_decl->getDeclContext();
    if (!isa<NamespaceDecl>(ctx_decl) ||
        cast<NamespaceDecl>(ctx_decl)->isAnonymousNamespace()) {
      return;
    }
    std::string path =
        misra::libtooling_utils::GetFilename(value_decl, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(value_decl, result.SourceManager);
    std::unordered_map<std::string,
                       std::vector<std::tuple<std::string, int>>>::iterator
        static_decl_it =
            statics_map_.find(value_decl->getQualifiedNameAsString());
    if (static_decl_it == statics_map_.end()) {
      std::tuple<std::string, int> tp = std::make_tuple(path, line_number);
      std::vector<std::tuple<std::string, int>> vec = {tp};
      statics_map_.insert(
          std::make_pair(value_decl->getQualifiedNameAsString(), vec));
    } else {
      static_decl_it->second.push_back(std::make_tuple(path, line_number));
    }
  } else if (named_decl != nullptr) {
    if (named_decl->isImplicit() || named_decl->isInAnonymousNamespace()) {
      return;
    }
    std::unordered_map<std::string, unsigned int>::iterator named_decl_it =
        named_map_.find(named_decl->getQualifiedNameAsString());
    if (named_decl_it == named_map_.end()) {
      named_map_.insert(
          std::make_pair(named_decl->getQualifiedNameAsString(), 1));
    } else {
      ++named_decl_it->second;
    }
  }
}

void Callback::Report() {
  for (const std::pair<const std::string,
                       std::vector<std::tuple<std::string, int>>>& it :
       statics_map_) {
    std::unordered_map<std::string, unsigned int>::iterator named_it =
        named_map_.find(it.first);
    if (named_it != named_map_.end() && named_it->second > 1) {
      for (std::tuple<std::string, int> tp : it.second) {
        std::string path = "";
        int line_number = 0;
        std::tie(path, line_number) = tp;
        ReportError(path, line_number, results_list_);
      }
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

void Checker::Report() { callback_->Report(); }
}  // namespace libtooling
}  // namespace rule_A2_10_4
}  // namespace autosar
