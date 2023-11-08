/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_17_0_1/libtooling/checker.h"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;

namespace misra_cpp_2008 {
namespace rule_17_0_1 {
namespace libtooling {

void Check::Init(analyzer::proto::ResultsList* results_list,
                 SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

void Check::ReportError(string name, SourceLocation loc) {
  string path = misra::libtooling_utils::GetRealFilename(loc, source_manager_);
  int line = misra::libtooling_utils::GetRealLine(loc, source_manager_);
  string error_message =
      "标准库中保留的标识符、宏和函数不得定义、重新定义或未定义";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_17_0_1);
  pb_result->set_name(name);
}

void Check::MacroDefined(const Token& name_tok,
                         const MacroDirective* directive) {
  auto loc = name_tok.getLocation();
  if (source_manager_->isInSystemHeader(loc)) return;
  string name = name_tok.getIdentifierInfo()->getName().str();
  if ((macros_.find(name) != macros_.end()) ||
      (keywords_.find(name) != keywords_.end())) {
    ReportError(name, loc);
  }
  if (builtin_macros_.find(name) != builtin_macros_.end()) {
    if (source_manager_->isWrittenInCommandLineFile(loc))
      return;  // ignore command line macro definitions
    ReportError(name, loc);
  }
}

void Check::MacroUndefined(const clang::Token& name_tok,
                           const clang::MacroDefinition& directive,
                           const clang::MacroDirective* undef) {
  auto loc = name_tok.getLocation();
  if (source_manager_->isInSystemHeader(loc)) return;
  string name = name_tok.getIdentifierInfo()->getName().str();
  if ((macros_.find(name) != macros_.end()) ||
      (builtin_macros_.find(name) != builtin_macros_.end()) ||
      (keywords_.find(name) != keywords_.end())) {
    ReportError(name, loc);
  }
}

bool Action::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<Check> callback(new Check());
  callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
}

void VarDeclVisitor::Init(analyzer::proto::ResultsList* results_list,
                          SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

bool VarDeclVisitor::VisitVarDecl(clang::VarDecl* vd) {
  if (source_manager_->isInSystemHeader(vd->getBeginLoc()))
    return true;  // skip system header
  std::string name = vd->getNameAsString();
  if (objects_.find(name) != objects_.end()) {
    auto loc = vd->getBeginLoc();
    string path =
        misra::libtooling_utils::GetRealFilename(loc, source_manager_);
    int line = misra::libtooling_utils::GetRealLine(loc, source_manager_);
    string error_message =
        "标准库中保留的标识符、宏和函数不得定义、重新定义或未定义";
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_17_0_1);
    pb_result->set_name(name);
  }
  return true;
}

}  // namespace libtooling
}  // namespace rule_17_0_1
}  // namespace misra_cpp_2008
