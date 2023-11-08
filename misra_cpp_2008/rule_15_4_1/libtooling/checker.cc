/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_15_4_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {
void ReportError(ResultsList* results_list, string path, int line_number,
                 string loc, string other_loc, string name) {
  string error_message =
      "如果一个函数是用一个异常规范声明的，那么同一个函数的所有声明（在其他翻译单元中）都应该用相同的 type-ids 集声明";
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_15_4_1);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  pb_result->set_name(name);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_15_4_1 {
namespace libtooling {

struct ExceptionSpecInfo {
  string file;
  int linenum;
  std::set<std::string> exceptions;
};

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("d"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* d = result.Nodes.getNodeAs<FunctionDecl>("d");
    string name = d->getQualifiedNameAsString();
    auto excepts = d->getType()
                       ->getAs<FunctionProtoType>()
                       ->getExceptionSpecInfo()
                       .Exceptions;
    std::set<string> str_excepts;
    for (auto e = excepts.begin(); e != excepts.end(); e++) {
      str_excepts.insert(e->getCanonicalType().getAsString());
    }
    auto known = name_infos_.find(name);
    if (known != name_infos_.end()) {
      // name exists in name_infos_
      if (d->getExceptionSpecType() !=
              clang::ExceptionSpecificationType::EST_Dynamic ||
          str_excepts != known->second.exceptions) {
        string path =
            misra::libtooling_utils::GetFilename(d, result.SourceManager);
        int linenum = misra::libtooling_utils::GetLine(d, result.SourceManager);
        ReportError(results_list_, path, linenum,
                    absl::StrFormat("%s:%d:1", known->second.file,
                                    known->second.linenum),
                    absl::StrFormat("%s:%d:1", path, linenum), name);
      }
    } else {
      // name is new to name_infos_, no need to check
      auto spectype = d->getExceptionSpecType();
      if (spectype != clang::ExceptionSpecificationType::EST_Dynamic)
        return;  // exception spec types have no typeid set
      name_infos_[name] = {
          misra::libtooling_utils::GetFilename(d, result.SourceManager),
          misra::libtooling_utils::GetLine(d, result.SourceManager),
          str_excepts};
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct ExceptionSpecInfo> name_infos_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_15_4_1
}  // namespace misra_cpp_2008
