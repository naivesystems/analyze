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

#include "misra_cpp_2008/rule_0_1_11/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using analyzer::proto::ResultsList;

namespace {

struct MethodInfo {
  std::string path;
  int line_number;
};

void ReportError(std::string path, int line_number, ResultsList* results_list) {
  string error_message =
      "非虚（non-virtual）函数不得有未使用的（命名或未命名）形参";
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_0_1_11);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_0_1_11 {
namespace libtooling {
class ParamCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("fd"), this);
    finder->addMatcher(cxxMethodDecl(unless(isVirtual())).bind("fd"), this);
    // match callback usage of certain function
    finder->addMatcher(
        declRefExpr(unless(hasAncestor(callExpr())), to(functionDecl()))
            .bind("callback_fd"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const DeclRefExpr* callback_fd =
        result.Nodes.getNodeAs<DeclRefExpr>("callback_fd");

    // callback_fd is to match the potential callback usage of previous
    // functionDecl. If a callback usage of a previous functionDecl is found,
    // then remove the functionDecl from the map so that no error will be
    // reported.
    if (callback_fd) {
      // skip system header
      if (misra::libtooling_utils::IsInSystemHeader(callback_fd,
                                                    result.Context)) {
        return;
      }
      // get actual function declaration name with potential qualifier.
      std::string function_name =
          callback_fd->getDecl()->getQualifiedNameAsString();

      // look up function_name
      auto it = callback_list_.find(function_name);
      if (it != callback_list_.end()) {
        callback_list_.erase(function_name);
      }
    }

    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    if (fd) {
      // skip system header
      if (misra::libtooling_utils::IsInSystemHeader(fd, result.Context)) {
        return;
      }
      if (fd->isDefaulted() || !fd->doesThisDeclarationHaveABody()) {
        return;
      }
      for (size_t i = 0; i < fd->param_size(); ++i) {
        const ParmVarDecl* pd = fd->getParamDecl(i);
        if (pd->getNameAsString().empty()) {
          // if the parameter is unnamed, save it for upcoming callback check.
          MethodInfo info = ((struct MethodInfo){
              misra::libtooling_utils::GetFilename(pd, result.SourceManager),
              misra::libtooling_utils::GetLine(pd, result.SourceManager)});
          auto it = callback_list_.find(fd->getQualifiedNameAsString());
          if (it != callback_list_.end())
            it->second.push_back(info);
          else
            callback_list_[fd->getQualifiedNameAsString()] = {info};
          break;
        }
        if (!(pd->isUsed() || pd->isReferenced())) {
          // report if the parameter is unused.
          std::string path =
              misra::libtooling_utils::GetFilename(pd, result.SourceManager);
          int line_number =
              misra::libtooling_utils::GetLine(pd, result.SourceManager);
          ReportError(path, line_number, results_list_);
          break;
        }
      }
    }
  }

  void Report() {
    for (auto it = callback_list_.begin(); it != callback_list_.end(); ++it) {
      for (auto entry : it->second) {
        ReportError(entry.path, entry.line_number, results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  // key is function qualifier(if any) + function name, value is a list of
  // function references with the given key as name.
  std::unordered_map<std::string, vector<MethodInfo>> callback_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new ParamCallback;
  callback_->Init(result_list, &finder_);
}
void Checker::Report() { callback_->Report(); }
}  // namespace libtooling
}  // namespace rule_0_1_11
}  // namespace misra_cpp_2008
