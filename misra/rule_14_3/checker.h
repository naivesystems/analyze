/*
Copyright 2022 Naive Systems Ltd.

This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.

Rule 14.3:

This checker match a simple case: `do { } while(0)`.
The result of this will be used to trim false positive cases in infer.
*/

#ifndef ANALYZER_MISRA_RULE_14_3_CHECKER_H_
#define ANALYZER_MISRA_RULE_14_3_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_14_3 {
class AssignOpCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  AssignOpCallback* callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_14_3
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_14_3_CHECKER_H_
