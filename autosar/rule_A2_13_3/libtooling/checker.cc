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

#include "autosar/rule_A2_13_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const Decl* decl, std::string error_message,
                 const ast_matchers::MatchFinder::MatchResult& result,
                 ResultsList* results_list_) {
  std::string path =
      misra::libtooling_utils::GetFilename(decl, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(decl, result.SourceManager);

  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

void ReportError(const Stmt* stmt, std::string error_message,
                 const ast_matchers::MatchFinder::MatchResult& result,
                 ResultsList* results_list_) {
  std::string path =
      misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(stmt, result.SourceManager);

  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A2_13_3 {
namespace libtooling {

auto isWideChar(const Type* type) -> bool {
  while (type->isArrayType() || type->isPointerType()) {
    type = type->getPointeeOrArrayElementType();
  }
  return type->isWideCharType();
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        varDecl(unless(isExpansionInSystemHeader())).bind("value_decl"), this);
    finder->addMatcher(
        fieldDecl(unless(isExpansionInSystemHeader())).bind("value_decl"),
        this);
    finder->addMatcher(
        parmVarDecl(unless(isExpansionInSystemHeader())).bind("value_decl"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("func_decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    std::string error_message =
        absl::StrFormat("Type wchar_t shall not be used.");

    const ValueDecl* value_decl =
        result.Nodes.getNodeAs<ValueDecl>("value_decl");
    if (value_decl) {
      if (isWideChar(
              value_decl->getType().getNonReferenceType().getTypePtr())) {
        ReportError(value_decl, error_message, result, results_list_);
      }
    }

    const FunctionDecl* func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl");
    if (func_decl) {
      if (isWideChar(
              func_decl->getReturnType().getNonReferenceType().getTypePtr())) {
        ReportError(func_decl, error_message, result, results_list_);
      }
    }

    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A2_13_3
}  // namespace autosar
