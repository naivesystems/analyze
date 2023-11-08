/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_RULE_18_0_5_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_18_0_5_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

using namespace clang;

namespace misra_cpp_2008 {
namespace rule_18_0_5 {
namespace libtooling {

class SpecificFunctionUsageCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  SpecificFunctionUsageCallback* callback_;
  ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace rule_18_0_5
}  // namespace misra_cpp_2008

#endif  // ANALYZER_MISRA_CPP_2008_RULE_18_0_5_LIBTOOLING_CHECKER_H_
