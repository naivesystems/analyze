/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_rule_7_3_6_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_rule_7_3_6_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_7_3_6 {
namespace libtooling {

class UsingCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  UsingCallback* callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_7_3_6
}  // namespace misra_cpp_2008

#endif  // ANALYZER_MISRA_CPP_2008_rule_7_3_6_LIBTOOLING_CHECKER_H_
