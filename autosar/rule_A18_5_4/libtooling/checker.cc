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

#include "autosar/rule_A18_5_4/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "If a project has sized or unsized versionof operator “delete” globally defined, then both sized and unsized versions shall be defined.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A18_5_4 {

namespace libtooling {
// 分别匹配有size参数和unsize的operator delete函数
// 将他们根据第一个指针参数的类型分类存储起来
// 匹配完成后在ReportResult中去判断有没有配对的函数
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(hasName("operator delete"),
                     hasParent(translationUnitDecl()), parameterCountIs(2),
                     hasParameter(0, hasType(pointerType())),
                     hasParameter(1, hasType(qualType(hasDeclaration(
                                         namedDecl(hasName("std::size_t")))))))
            .bind("decl_unsized"),
        this);
    finder->addMatcher(
        functionDecl(hasName("operator delete"),
                     hasParent(translationUnitDecl()),
                     hasParameter(0, hasType(pointerType())),
                     parameterCountIs(1), hasDescendant(compoundStmt()))
            .bind("decl_sized"),
        this);
  }

  void ReportResult() {
    if (decl_unsized_storage_.size()) {
      for (auto it = decl_unsized_storage_.begin();
           it != decl_unsized_storage_.end();) {
        // 寻找指针类型配对的函数
        if (decl_sized_storage_.count(it->first) == 0) {
          it->second();
          it = decl_unsized_storage_.erase(it);
        } else {
          decl_sized_storage_.erase(it->first);
          it = decl_unsized_storage_.erase(it);
        }
      }
    }
    if (decl_sized_storage_.size()) {
      for (auto it = decl_sized_storage_.begin();
           it != decl_sized_storage_.end();) {
        if (decl_unsized_storage_.count(it->first) == 0) {
          it->second();
          it = decl_sized_storage_.erase(it);
        } else {
          decl_unsized_storage_.erase(it->first);
          it = decl_sized_storage_.erase(it);
        }
      }
    }
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* decl_unsized =
        result.Nodes.getNodeAs<FunctionDecl>("decl_unsized");
    const FunctionDecl* decl_sized =
        result.Nodes.getNodeAs<FunctionDecl>("decl_sized");
    if (decl_unsized) {
      const SourceLocation loc = decl_unsized->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        auto file_name = misra::libtooling_utils::GetFilename(
            decl_unsized, result.SourceManager);
        auto file_line = misra::libtooling_utils::GetLine(decl_unsized,
                                                          result.SourceManager);
        // 获得第一个指针的类型
        auto first_parm = *(decl_unsized->param_begin());
        auto pointer_type = first_parm->getQualifiedNameAsString();
        // 存储指针类型和此次的函数调用
        decl_unsized_storage_.insert(
            {pointer_type,
             std::bind(ReportError, file_name, file_line, results_list_)});
      }
    } else {
      const SourceLocation loc = decl_sized->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        auto file_name = misra::libtooling_utils::GetFilename(
            decl_sized, result.SourceManager);
        auto file_line =
            misra::libtooling_utils::GetLine(decl_sized, result.SourceManager);
        // 获得第一个指针的类型
        auto first_parm = *(decl_sized->param_begin());
        auto pointer_type = first_parm->getQualifiedNameAsString();
        // 存储指针类型和相对应的函数
        decl_sized_storage_.insert(
            {pointer_type,
             std::bind(ReportError, file_name, file_line, results_list_)});
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, function<void()>> decl_unsized_storage_;
  unordered_map<string, function<void()>> decl_sized_storage_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

void Checker::ReportResult() { callback_->ReportResult(); }

}  // namespace libtooling
}  // namespace rule_A18_5_4
}  // namespace autosar
