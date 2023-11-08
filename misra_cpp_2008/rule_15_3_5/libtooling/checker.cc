/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_15_3_5/libtooling/checker.h"

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

namespace misra_cpp_2008 {
namespace rule_15_3_5 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxCatchStmt(
            has(varDecl(hasType(type(hasUnqualifiedDesugaredType(type(
                            recordType(hasDeclaration(tagDecl(isClass()))))))))
                    .bind("vd"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    if (misra::libtooling_utils::IsInSystemHeader(vd, result.Context)) {
      return;
    }
    string error_message = "类类型异常应始终通过引用捕获";
    string path =
        misra::libtooling_utils::GetFilename(vd, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(vd, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_15_3_5);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_15_3_5
}  // namespace misra_cpp_2008
