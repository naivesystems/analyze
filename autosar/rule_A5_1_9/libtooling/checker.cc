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

#include "autosar/rule_A5_1_9/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kUnnamedLambdaDeclString[] = "unnamedLambdaDecl";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Identical unnamed lambda expressions shall be replaced with a named function or a named lambda expression.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A5_1_9 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      lambdaExpr(unless(hasAncestor(varDecl()))).bind(kUnnamedLambdaDeclString),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const LambdaExpr* lambda_expr =
      result.Nodes.getNodeAs<LambdaExpr>(kUnnamedLambdaDeclString);
  const unsigned start_offset = result.SourceManager->getFileOffset(
                     lambda_expr->getBeginLoc()),
                 end_offset = result.SourceManager->getFileOffset(
                     lambda_expr->getEndLoc()),
                 length = end_offset - start_offset + 1;
  std::string text = result.SourceManager
                         ->getBufferData(result.SourceManager->getFileID(
                             lambda_expr->getBeginLoc()))
                         .data();
  text = text.substr(start_offset, length);
  std::string path =
      misra::libtooling_utils::GetFilename(lambda_expr, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(lambda_expr, result.SourceManager);
  std::unordered_map<
      std::string, std::vector<std::tuple<std::string, int>>>::iterator vec_it =
      unnamed_lambda_map_.find(text);
  if (vec_it == unnamed_lambda_map_.end()) {
    std::vector<std::tuple<std::string, int>> vec = {
        std::make_tuple(path, line_number)};
    unnamed_lambda_map_.insert(std::make_pair(text, vec));
  } else {
    vec_it->second.push_back(std::make_tuple(path, line_number));
  }
}

void Callback::Report() {
  for (const std::pair<const std::string,
                       std::vector<std::tuple<std::string, int>>>& map_it :
       unnamed_lambda_map_) {
    if (map_it.second.size() > 1) {
      for (const std::tuple<std::string, int>& vec_it : map_it.second) {
        std::string path;
        int line_number;
        std::tie(path, line_number) = vec_it;
        ReportError(path, line_number, results_list_);
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

void Checker::Report() { callback_->Report(); }

}  // namespace libtooling
}  // namespace rule_A5_1_9
}  // namespace autosar
