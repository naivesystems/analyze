/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_RULE_3_2_2_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_3_2_2_LIBTOOLING_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_3_2_2 {
namespace libtooling {

class RecordCallback;
class FunctionCallback;
class ClassTemplateCallback;
class VarCallback;
class TypedefCallback;

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);

  clang::ast_matchers::MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  RecordCallback* rd_callback_;
  ClassTemplateCallback* ct_callback_;
  FunctionCallback* fun_callback_;
  VarCallback* var_callback_;
  TypedefCallback* td_callback_;
  clang::ast_matchers::MatchFinder finder_;
  analyzer::proto::ResultsList* results_list_;
};
}  // namespace libtooling
}  // namespace rule_3_2_2
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_RULE_3_2_2_LIBTOOLING_CHECKER_H_
