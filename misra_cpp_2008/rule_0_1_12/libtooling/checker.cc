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

#include "misra_cpp_2008/rule_0_1_12/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using analyzer::proto::ResultsList;

namespace {

struct MethodInfo {
  std::string loc;
  std::string path;
  int line_number;
};

void ReportError(std::string path, int line_number, ResultsList* results_list) {
  string error_message =
      "在一个虚拟函数和所有覆盖它的函数的形参集中，不应有未使用的（命名或未命名）形参";
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_0_1_12);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_0_1_12 {
namespace libtooling {
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxMethodDecl(unless(isPure()), anyOf(isVirtual(), isOverride()))
            .bind("bad_md"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* fd = result.Nodes.getNodeAs<CXXMethodDecl>("bad_md");

    // skip system header
    if (misra::libtooling_utils::IsInSystemHeader(fd, result.Context)) {
      return;
    }
    // ignore function without method body
    if (!fd->hasBody()) return;
    auto it = param_use_count_.find(fd->getNameAsString());
    if (it != param_use_count_.end()) {
      // check if the function is duplicated
      MethodInfo first = checked_fd_[fd->getNameAsString()][0];
      if (first.loc ==
          misra::libtooling_utils::GetLocation(fd, result.SourceManager)) {
        return;
      }
      // have overridden method
      for (size_t i = 0; i < fd->param_size(); ++i) {
        if (fd->getParamDecl(i)->isUsed()) {
          it->second[i] = 1;
        }
      }
      MethodInfo info = ((struct MethodInfo){
          misra::libtooling_utils::GetLocation(fd, result.SourceManager),
          misra::libtooling_utils::GetFilename(fd, result.SourceManager),
          misra::libtooling_utils::GetLine(fd, result.SourceManager)});
      checked_fd_[fd->getNameAsString()].push_back(info);
    } else {
      // first time
      std::vector<MethodInfo> list_of_fds;
      std::vector<int> param_usage(fd->param_size(), 0);
      for (size_t i = 0; i < fd->param_size(); ++i) {
        if (fd->getParamDecl(i)->isUsed()) {
          param_usage[i] = 1;
        }
      }
      // add to map for checking parameter usage
      param_use_count_[fd->getNameAsString()] = param_usage;
      // add to map for functions sharing the same name (overridden)
      MethodInfo info = ((struct MethodInfo){
          misra::libtooling_utils::GetLocation(fd, result.SourceManager),
          misra::libtooling_utils::GetFilename(fd, result.SourceManager),
          misra::libtooling_utils::GetLine(fd, result.SourceManager)});
      list_of_fds.push_back(info);
      checked_fd_[fd->getNameAsString()] = list_of_fds;
    }
    return;
  }

  void Report() {
    for (auto it = param_use_count_.begin(); it != param_use_count_.end();
         ++it) {
      for (int count : it->second) {
        if (count == 0) {
          for (auto fds : checked_fd_[it->first]) {
            ReportError(fds.path, fds.line_number, results_list_);
          }
          break;
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<std::string, vector<MethodInfo>> checked_fd_;
  std::unordered_map<std::string, vector<int>> param_use_count_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}

void Checker::Report() { callback_->Report(); }
}  // namespace libtooling
}  // namespace rule_0_1_12
}  // namespace misra_cpp_2008
