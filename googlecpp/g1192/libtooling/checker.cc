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

#include "googlecpp/g1192/libtooling/checker.h"

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
      "Friend classes and functions should only be defined in the same file";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1192 {
namespace libtooling {

struct MethodInfo {
  std::string path;
  int line_number;
};

class FriendInSameFileCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  std::unordered_map<std::string, MethodInfo>
      friend_func_decl_locs_;  // Store all friend function declaration
  std::unordered_map<std::string, MethodInfo>
      friend_class_decl_locs_;  // Store all friend class declaration
  std::unordered_map<std::string, MethodInfo>
      func_def_locs_;  // Store all function definition
  std::unordered_map<std::string, MethodInfo>
      class_def_locs_;  // Store all class definition

  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // Get all friend declaration(including class and function)
    finder->addMatcher(
        cxxRecordDecl(unless(isExpansionInSystemHeader()),
                      forEachDescendant(friendDecl().bind("friend_decl"))),
        this);
    // Get all class definition
    finder->addMatcher(
        cxxRecordDecl(unless(isExpansionInSystemHeader()), isDefinition())
            .bind("class_def"),
        this);
    // Get all function definition
    finder->addMatcher(
        functionDecl(isDefinition(), unless(isMain()), unless(isDefaulted()))
            .bind("func_def"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    SourceManager* source_manager = result.SourceManager;

    auto* friend_decl = result.Nodes.getNodeAs<FriendDecl>("friend_decl");
    if (friend_decl) {
      MethodInfo info = MethodInfo{
          misra::libtooling_utils::GetFilename(friend_decl, source_manager),
          misra::libtooling_utils::GetLine(friend_decl, source_manager)};
      // for friend function
      if (friend_decl->getFriendDecl()) {
        friend_func_decl_locs_[friend_decl->getFriendDecl()
                                   ->getQualifiedNameAsString()] = info;
      } else {
        friend_decl->getFriendType();
        // for friend class
        std::string class_type =
            friend_decl->getFriendType()
                ->getType()
                .getAsString();  // The class_type string will like "class A" or
                                 // "class A<T>"
        std::string class_name =
            getClassName(class_type);  // Subtract the class name
        friend_class_decl_locs_[class_name] = info;
      }
    }

    auto* class_def = result.Nodes.getNodeAs<CXXRecordDecl>("class_def");
    if (class_def) {
      auto class_def_name = class_def->getQualifiedNameAsString();
      MethodInfo info = MethodInfo{
          misra::libtooling_utils::GetFilename(class_def, source_manager),
          misra::libtooling_utils::GetLine(class_def, source_manager)};
      class_def_locs_[class_def_name] = info;
    }

    auto* func_def = result.Nodes.getNodeAs<FunctionDecl>("func_def");
    if (func_def) {
      auto func_def_name = func_def->getQualifiedNameAsString();
      MethodInfo info = MethodInfo{
          misra::libtooling_utils::GetFilename(func_def, source_manager),
          misra::libtooling_utils::GetLine(func_def, source_manager)};
      func_def_locs_[func_def_name] = info;
    }
  }

 private:
  static constexpr int class_prefix_length =
      6;  // The i = 6 indicates the length of string "class "

  analyzer::proto::ResultsList* results_list_;

  // Subtract the class name from string like "class A" or "class A<T>"
  std::string getClassName(std::string class_str) {
    std::string class_name = "";
    for (int i = class_prefix_length; i < class_str.size(); i++) {
      if (class_str[i] == '<') break;
      class_name += class_str[i];
    }
    return class_name;
  }
};

void Checker::Run() const {
  for (auto const& item : callback_->friend_func_decl_locs_) {
    auto const& func_name = item.first;
    auto const& info = item.second;
    if (callback_->func_def_locs_.find(func_name) !=
        callback_->func_def_locs_
            .end()) {  // If the declared friend function has been defined
      if (info.path != callback_->func_def_locs_[func_name]
                           .path) {  // Judge whether they are in the same file
        ReportError(info.path, info.line_number, results_list_);
      }
    } else {  // The declared friend function doesn't been defined
      ReportError(info.path, info.line_number, results_list_);
    }
  }

  for (auto item : callback_->friend_class_decl_locs_) {
    auto const& class_name = item.first;
    auto const& info = item.second;
    if (callback_->class_def_locs_.find(class_name) !=
        callback_->class_def_locs_
            .end()) {  // If the declared friend class has been defined
      if (info.path != callback_->class_def_locs_[class_name]
                           .path) {  // Judge whether they are in the same file
        ReportError(info.path, info.line_number, results_list_);
      }
    } else {  // The declared friend class doesn't been defined
      ReportError(info.path, info.line_number, results_list_);
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new FriendInSameFileCallback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1192
}  // namespace googlecpp
