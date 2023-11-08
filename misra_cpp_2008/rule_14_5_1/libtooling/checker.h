/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_rule_14_5_1_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_rule_14_5_1_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "clang/Tooling/Tooling.h"
#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_14_5_1 {
namespace libtooling {

class AssociatedNSCallback;
class GenericFDCallback;

class AssociatedNSChecker {
 public:
  void Init();
  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  AssociatedNSCallback* associated_ns_callback_;
  clang::ast_matchers::MatchFinder finder_;
};

class GenericFDChecker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  GenericFDCallback* generic_fd_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_14_5_1
}  // namespace misra_cpp_2008

#endif  // ANALYZER_MISRA_CPP_2008_rule_14_5_1_LIBTOOLING_CHECKER_H_
