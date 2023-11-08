/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_17_0_2/libtooling/checker.h"

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
namespace rule_17_0_2 {
namespace libtooling {

void Check::Init(analyzer::proto::ResultsList* results_list,
                 SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
  macros_ = {"NULL",           "offsetof", "EXIT_FAILURE", "EXIT_SUCCESS",
             "va_arg",         "va_end",   "va_start",     "setjmp",
             "CLOCKS_PER_SEC", "SIGABRT",  "SIGILL",       "SIGSEGV",
             "SIG_DFL",        "SIG_IGN",  "SIGFPE",       "SIGINT",
             "SIGTERM",        "SIG_ERR",  "assert",       "EDOM",
             "ERANGE",         "errno",    "WEOF",         "WCHAR_MAX",
             "WCHAR_MIN",      "WEOF",     "MB_CUR_MAX",   "LC_ALL",
             "LC_COLLATE",     "LC_CTYPE", "LC_MONETARY",  "LC_NUMERIC",
             "LC_TIME",        "HUGE_VAL", "RAND_MAX",     "BUFSIZ",
             "FOPEN_MAX",      "SEEK_CUR", "TMP_MAX",      "_IONBF",
             "stdout",         "EOF",      "L_tmpnam",     "SEEK_END",
             "_IOFBF",         "stderr",   "FILENAME_MAX", "SEEK_SET",
             "_IOLBF",         "stdin"};
}

void Check::MacroDefined(const Token& name_tok,
                         const MacroDirective* directive) {
  if (source_manager_->isInSystemHeader(directive->getLocation())) return;
  string name = name_tok.getIdentifierInfo()->getName().str();
  const MacroInfo* macro_info = directive->getMacroInfo();
  if (macros_.find(name) != macros_.end()) {
    auto loc = macro_info->getDefinitionLoc();
    string path =
        misra::libtooling_utils::GetRealFilename(loc, source_manager_);
    int line = misra::libtooling_utils::GetRealLine(loc, source_manager_);
    string error_message = "标准库宏和对象的名称不得重复使用";
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_17_0_2);
    pb_result->set_name(name);
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
  objects_ = {"CHAR_BIT",
              "CHAR_MAX",
              "CHAR_MIN",
              "INT_MAX",
              "INT_MIN",
              "LONG_MAX",
              "LONG_MIN",
              "MB_LEN_MAX",
              "SCHAR_MAX",
              "SCHAR_MIN",
              "SHRT_MAX",
              "SHRT_MIN",
              "UCHAR_MAX",
              "UINT_MAX",
              "ULONG_MAX",
              "USHRT_MAX",
              "DBL_DIG",
              "DBL_EPSILON",
              "DBL_MANT_DIG",
              "DBL_MAX",
              "DBL_MAX_10_EXP",
              "DBL_MAX_EXP",
              "DBL_MIN",
              "DBL_MIN_10_EXP",
              "DBL_MIN_EXP",
              "FLT_DIG",
              "FLT_EPSILON",
              "FLT_MANT_DIG",
              "FLT_MAX",
              "FLT_MAX_10_EXP",
              "FLT_MAX_EXP",
              "FLT_MIN",
              "FLT_MIN_10_EXP",
              "FLT_MIN_EXP",
              "FLT_RADIX",
              "FLT_ROUNDS",
              "LDBL_DIG",
              "LDBL_EPSILON",
              "LDBL_MANT_DIG",
              "LDBL_MAX",
              "LDBL_MAX_10_EXP",
              "LDBL_MAX_EXP",
              "LDBL_MIN",
              "LDBL_MIN_10_EXP",
              "LDBL_MIN_EXP",
              "nothrow",
              "cin",
              "cout",
              "cerr",
              "clog",
              "wcin",
              "wcout",
              "wcerr",
              "wclog"};
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
    string error_message = "标准库宏和对象的名称不得重复使用";
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_17_0_2);
    pb_result->set_name(name);
  }
  return true;
}

}  // namespace libtooling
}  // namespace rule_17_0_2
}  // namespace misra_cpp_2008
