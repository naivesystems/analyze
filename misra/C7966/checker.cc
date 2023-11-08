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

#include "misra/C7966/checker.h"

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
struct Loc {
  int line;
  int count;
  string begin_loc;
};
void ReportError(string filename, int line, ResultsList* results_list) {
  std::string error_message = "[C7966][NAIVESYSTEMS_C7966]: violation of C7966";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, filename, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_NAIVESYSTEMS_C7966);
  pb_result->set_external_message("malloc分配的大小应为4的倍数");
  LOG(INFO) << error_message;
  std::cout << line << "\n";
}
}  // namespace

namespace misra {
namespace C7966 {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(callExpr().bind("ce"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* ce = result.Nodes.getNodeAs<CallExpr>("ce");
    if (libtooling_utils::IsInSystemHeader(ce, result.Context)) {
      return;
    }
    const FunctionDecl* fd = ce->getDirectCallee();
    if (!fd) {
      return;
    }
    IdentifierInfo* info = fd->getIdentifier();
    if (!info) {
      return;
    }
    if (!(info->isStr("malloc") || info->isStr("_MALLOC"))) {
      return;
    }
    if (ce->getNumArgs() == 1) {
      clang::Expr::EvalResult rint;
      const Expr* e = ce->getArg(0);
      if (e->isEvaluatable(*result.Context)) {
        e->EvaluateAsInt(rint, *result.Context);
        if (!rint.Val.isInt() || rint.Val.getInt().getExtValue() % 4 == 0) {
          return;
        }
        ReportError(libtooling_utils::GetFilename(ce, result.SourceManager),
                    libtooling_utils::GetLine(ce, result.SourceManager),
                    results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace C7966
}  // namespace misra
