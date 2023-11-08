/*
Copyright 2023 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_AUTOSAR_A14_5_1_LIBTOOLING_CHECKER_H_
#define ANALYZER_AUTOSAR_A14_5_1_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace autosar {
namespace rule_A14_5_1 {
namespace libtooling {

class Callback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  Callback* callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_A14_5_1
}  // namespace autosar

#endif  // ANALYZER_AUTOSAR_A14_5_1_LIBTOOLING_CHECKER_H_
