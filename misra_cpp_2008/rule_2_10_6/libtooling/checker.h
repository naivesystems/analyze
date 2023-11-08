/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_rule_2_10_6_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_rule_2_10_6_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "clang/Sema/Scope.h"
#include "clang/Sema/Sema.h"
#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_2_10_6 {
namespace libtooling {

class CheckTagDeclCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::ast_matchers::MatchFinder* finder);
  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override;

 private:
  analyzer::proto::ResultsList* results_list_;
};

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  CheckTagDeclCallback* record_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace rule_2_10_6
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_rule_2_10_6_LIBTOOLING_CHECKER_H_
