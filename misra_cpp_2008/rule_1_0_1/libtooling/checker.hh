/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_RULE_1_0_1_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_1_0_1_LIBTOOLING_CHECKER_H_

#include <clang/Basic/Diagnostic.h>
#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_1_0_1 {
namespace libtooling {

class Checker : public clang::DiagnosticConsumer {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& d) override;


 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_1_0_1
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_RULE_1_0_1_LIBTOOLING_CHECKER_H_
