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

#include "misra/rule_12_3/checker.h"

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

void CommaErr(string path, int linenum, ResultsList* results_list) {
  string error_message =
      "[C0603][misra-c2012-12.3]: comma operator should not be used";
  LOG(INFO) << error_message;
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_12_3);
}

}  // namespace

namespace misra {
namespace rule_12_3 {

class CommaCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(binaryOperation(hasOperatorName(",")).bind("comma"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* comma = result.Nodes.getNodeAs<Expr>("comma");
    const ASTContext* context = result.Context;

    if (comma) {
      clang::FullSourceLoc location = context->getFullLoc(comma->getBeginLoc());
      if (location.isInvalid() || location.isInSystemHeader()) {
        return;
      }
      CommaErr(libtooling_utils::GetFilename(comma, result.SourceManager),
               libtooling_utils::GetLine(comma, result.SourceManager),
               results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  comma_callback_ = new CommaCallback;
  comma_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_12_3
}  // namespace misra
