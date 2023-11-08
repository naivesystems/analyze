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

#include "autosar/rule_A5_2_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A cast shall not remove any const or volatile qualification from the "
      "type of a pointer or reference.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A5_2_3 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      explicitCastExpr(unless(isExpansionInSystemHeader())).bind("cast"), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const ExplicitCastExpr* cast =
      result.Nodes.getNodeAs<ExplicitCastExpr>("cast");
  QualType src_type = cast->getSubExpr()->getType();
  QualType dst_type = cast->getTypeAsWritten();
  if (!src_type->isPointerType() && !src_type->isReferenceType()) return;
  size_t src_type_const_count = 0, src_type_volatile_count = 0;
  size_t dst_type_const_count = 0, dst_type_volatile_count = 0;
  while (src_type->isPointerType() || src_type->isReferenceType()) {
    if (src_type->isPointerType()) src_type = src_type->getPointeeType();
    if (src_type->isReferenceType()) src_type = src_type.getNonReferenceType();
    if (src_type.isConstQualified()) ++src_type_const_count;
    if (src_type.isVolatileQualified()) ++src_type_volatile_count;
  }
  while (dst_type->isPointerType() || dst_type->isReferenceType()) {
    if (dst_type->isPointerType()) dst_type = dst_type->getPointeeType();
    if (dst_type->isReferenceType()) dst_type = dst_type.getNonReferenceType();
    if (dst_type.isConstQualified()) ++dst_type_const_count;
    if (dst_type.isVolatileQualified()) ++dst_type_volatile_count;
  }
  if ((src_type_const_count > dst_type_const_count) ||
      (src_type_volatile_count > dst_type_volatile_count)) {
    ReportError(
        misra::libtooling_utils::GetFilename(cast, result.SourceManager),
        misra::libtooling_utils::GetLine(cast, result.SourceManager),
        results_list_);
  }
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A5_2_3
}  // namespace autosar
