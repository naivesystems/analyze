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

#include "misra/rule_7_4/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string loc, std::string path, int line_number,
                 ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0901][misra-c2012-7.4]: Assignment violation of misra-c2012-7.4\n"
      "try to assign string literal to object with improper type\n"
      "Location: %s",
      loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_7_4);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_7_4 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(std::map<std::string, StringLiteralInfo>* count_str_literal_cast,
            ResultsList* results_list, MatchFinder* finder) {
    count_str_literal_cast_ = count_str_literal_cast;
    results_list_ = results_list;
    finder->addMatcher(
        castExpr((hasSourceExpression(ignoringParenCasts(stringLiteral()))))
            .bind("fromStringLiteral"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CastExpr* ce = result.Nodes.getNodeAs<CastExpr>("fromStringLiteral");

    // skip system header
    if (libtooling_utils::IsInSystemHeader(ce, result.Context)) {
      return;
    }
    ASTContext* context = result.Context;
    std::string path = libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number = libtooling_utils::GetLine(ce, result.SourceManager);
    std::string location =
        libtooling_utils::GetLocation(ce, result.SourceManager);
    if (!ce->getType()->isPointerType()) {
      ReportError(location, path, line_number, results_list_);
      return;
    }
    QualType destination_type = ce->getType()->getPointeeType();
    QualType source_type = ce->getSubExpr()->getType();
    source_type = source_type->isPointerType() ? source_type->getPointeeType()
                                               : source_type;
    destination_type.removeLocalVolatile();
    destination_type.removeLocalRestrict();
    if (count_str_literal_cast_->find(location) ==
        count_str_literal_cast_->end()) {
      StringLiteralInfo new_str_leteral = {1, line_number, path};
      count_str_literal_cast_->emplace(location, new_str_leteral);
    } else {
      StringLiteralInfo old_str_leteral = count_str_literal_cast_->at(location);
      old_str_leteral.count_ += 1;
      count_str_literal_cast_->find(location)->second = old_str_leteral;
    }
    QualType base_dtype =
        destination_type.getCanonicalType().getUnqualifiedType();
    QualType base_stype = source_type.getCanonicalType().getUnqualifiedType();

    if (destination_type.isConstQualified() && base_dtype == base_stype) {
      // exclude pointer const-qualified char
      StringLiteralInfo old_str_leteral = count_str_literal_cast_->at(location);
      old_str_leteral.count_ -= 2;
      count_str_literal_cast_->find(location)->second = old_str_leteral;
    }
  }

 private:
  ResultsList* results_list_;
  std::map<string, StringLiteralInfo>* count_str_literal_cast_;
};

void Checker::Init(
    analyzer::proto::ResultsList* results_list,
    std::map<std::string, StringLiteralInfo>* count_str_literal_cast) {
  results_list_ = results_list;
  callback_ = new CastCallback;
  count_str_literal_cast_ = count_str_literal_cast;
  callback_->Init(count_str_literal_cast_, results_list_, &finder_);
}

void Checker::FindInvalidStringLiteralAssignment() {
  for (auto it = count_str_literal_cast_->begin();
       it != count_str_literal_cast_->end(); ++it) {
    if (it->second.count_ > 0) {
      ReportError(it->first, it->second.path_, it->second.line_, results_list_);
    }
  }
}

}  // namespace rule_7_4
}  // namespace misra
