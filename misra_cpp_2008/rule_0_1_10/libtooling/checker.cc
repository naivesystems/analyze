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

#include "misra_cpp_2008/rule_0_1_10/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using analyzer::proto::ResultsList;

namespace {

// The libtooling implementation of this checker has another thread, i.e. using
// functionDecl->isReferenced() to check if the function definition is called.
// But this idea's'limitation is the same as the original checker:

// When there are only .cc and imported .h files, isReferenced() can check
// whether the defined functions are called or not.

// But when there are multiple .cc and . files, i.e. badcase2, goodcase5,
// goodcase6 (.h declares a function, 1.cc defines that function, 2.cc calls
// that function), isReferenced() cannot check whether the function has been
// called or not.

void ReportError(std::string path, int line_number, ResultsList* results_list) {
  string error_message = "每个被定义的函数必须至少被调用一次";
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_0_1_10);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_0_1_10 {
namespace libtooling {

typedef struct MethodInfo {
  std::string path;
  int line_number;
} loc_t;

typedef int64_t id_t;

class ParamCallback : public MatchFinder::MatchCallback {
 public:
  std::unordered_map<id_t, loc_t>
      fd_locs_;  //  Store all function declaration's ID and their location
  std::unordered_set<id_t> fd_used_set_;  // Store all used function's ID

  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    // match non-template functionDecl
    finder->addMatcher(
        functionDecl(isDefinition(), unless(isExpansionInSystemHeader()),
                     unless(isMain()), unless(isDefaulted()),
                     unless(hasParent(functionTemplateDecl())))
            .bind("fd"),
        this);
    // match template-instantiationed functionDecl
    finder->addMatcher(
        functionTemplateDecl(has(
            functionDecl(isDefinition(), unless(isExpansionInSystemHeader()),
                         unless(isMain()), unless(isDefaulted()),
                         has(templateArgument()))
                .bind("fd"))),
        this);
    finder->addMatcher(callExpr().bind("call_fd"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    if (fd) {
      if (!fd->doesThisDeclarationHaveABody()) return;
      fd = fd->getCanonicalDecl();
      id_t ID = fd->getID();
      loc_t info =
          loc_t{misra::libtooling_utils::GetFilename(fd, result.SourceManager),
                misra::libtooling_utils::GetLine(fd, result.SourceManager)};
      fd_locs_[ID] = info;
    }

    const CallExpr* call_fd = result.Nodes.getNodeAs<CallExpr>("call_fd");
    if (call_fd) {
      const FunctionDecl* func_call = call_fd->getDirectCallee();
      // if call_fd is not a funcion declaration, skip it.
      if (func_call) {
        id_t ID = func_call->getCanonicalDecl()->getID();
        fd_used_set_.insert(ID);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Run() const {
  for (auto const& it : callback_->fd_locs_) {
    auto const& id = it.first;
    auto const& info = it.second;
    if (callback_->fd_used_set_.find(id) == callback_->fd_used_set_.end()) {
      ReportError(info.path, info.line_number, results_list_);
    }
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new ParamCallback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_0_1_10
}  // namespace misra_cpp_2008
