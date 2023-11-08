/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_16_0_6/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;

namespace misra_cpp_2008 {
namespace rule_16_0_6 {
namespace libtooling {

void ReportError(SourceLocation loc, SourceManager* source_manager,
                 analyzer::proto::ResultsList* results_list) {}

void FindMacroDefineCallback::Init(analyzer::proto::ResultsList* results_list,
                                   SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

void FindMacroDefineCallback::MacroDefined(const Token& macro_name_tok,
                                           const MacroDirective* md) {
  if (md->getMacroInfo()->isObjectLike())
    return;  // we only care about function-like macros
  auto loc = md->getLocation();
  if (source_manager_->isInSystemHeader(loc)) return;
  std::set<const IdentifierInfo*> parms(md->getMacroInfo()->params().begin(),
                                        md->getMacroInfo()->params().end());
  auto tokens = md->getMacroInfo()->tokens();
  for (int i = 0; i < tokens.size(); i++) {
    if (tokens[i].getKind() != clang::tok::TokenKind::identifier) continue;
    auto ident = tokens[i].getIdentifierInfo();
    if (parms.find(ident) == parms.end())
      continue;  // not a macro parameter identifier
    if ((i > 0) &&
        ((tokens[i - 1].getKind() == clang::tok::TokenKind::hashhash) ||
         (tokens[i - 1].getKind() == clang::tok::TokenKind::hash)))
      continue;  // as `#x` or ` a ## x`
    if ((i < tokens.size() - 1) &&
        (tokens[i + 1].getKind() == clang::tok::TokenKind::hashhash))
      continue;  // as `x ## b`
    if ((i > 0) &&
        (tokens[i - 1].getKind() == clang::tok::TokenKind::l_paren) &&
        (i < tokens.size() - 1) &&
        (tokens[i + 1].getKind() == clang::tok::TokenKind::r_paren))
      continue;  // as (x)
    string path =
        misra::libtooling_utils::GetRealFilename(loc, source_manager_);
    int line = misra::libtooling_utils::GetRealLine(loc, source_manager_);
    string error_message =
        "在类函数宏的定义中，参数的每个实例都应该用括号括起来，除非它被用作#或##的操作数";
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_16_0_6);
    return;
  }

  // return;
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<FindMacroDefineCallback> callback(
      new FindMacroDefineCallback());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
}

}  // namespace libtooling
}  // namespace rule_16_0_6
}  // namespace misra_cpp_2008
