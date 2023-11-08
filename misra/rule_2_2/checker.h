/*
Copyright 2022 Naive Systems Ltd.

This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.

Rule 2.2: There shall be no dead code

This checker match a call to an empty function and report error of 2.2.
*/

#ifndef ANALYZER_MISRA_RULE_2_2_CHECKER_H_
#define ANALYZER_MISRA_RULE_2_2_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra {
namespace rule_2_2 {
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

}  // namespace rule_2_2
}  // namespace misra

#endif  // ANALYZER_MISRA_RULE_2_2_CHECKER_H_
