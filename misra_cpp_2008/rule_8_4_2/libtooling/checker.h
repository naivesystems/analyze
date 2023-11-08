/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_RULE_8_4_2_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_8_4_2_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_8_4_2 {
namespace libtooling {

class CXXMethodCallback;
class FunctionCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  FunctionCallback* fun_callback_;
  CXXMethodCallback* method_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace rule_8_4_2
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_RULE_8_4_2_LIBTOOLING_CHECKER_H_
