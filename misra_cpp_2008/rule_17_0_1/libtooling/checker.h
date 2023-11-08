/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#ifndef ANALYZER_MISRA_CPP_2008_RULE_17_0_1_LIBTOOLING_CHECKER_H_
#define ANALYZER_MISRA_CPP_2008_RULE_17_0_1_LIBTOOLING_CHECKER_H_

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

#include "misra/proto_util.h"

namespace misra_cpp_2008 {
namespace rule_17_0_1 {
namespace libtooling {

class Check : public clang::PPCallbacks {
 public:
  void Init(analyzer::proto::ResultsList* results_list_,
            clang::SourceManager* source_manager);

  void ReportError(std::string name, clang::SourceLocation loc);

  void MacroDefined(const clang::Token& macro_name_tok,
                    const clang::MacroDirective* md) override;

  void MacroUndefined(const clang::Token& macro_name_tok,
                      const clang::MacroDefinition& def,
                      const clang::MacroDirective* md) override;

 private:
  clang::SourceManager* source_manager_;
  analyzer::proto::ResultsList* results_list_;
  std::set<std::string> macros_ = {
      "assert",     "HUGE_VAL",  "NULL",         "SIGILL",     "va_arg",
      "BUFSIZ",     "LC_ALL",    "SIGINT",       "va_end",     "CLOCKS_PER_SEC",
      "LC_COLLATE", "SIGSEGV",   "va_start",     "EDOM",       "LC_CTYPE",
      "offsetof",   "SIGTERM",   "WCHAR_MAX",    "EOF",        "LC_MONETARY",
      "RAND_MAX",   "SIG_DFL",   "WCHAR_MIN",    "ERANGE",     "LC_NUMERIC",
      "SEEK_CUR",   "SIG_ERR",   "WEOF",         "errno",      "LC_TIME",
      "SEEK_END",   "SIG_IGN",   "EXIT_FAILURE", "L_tmpnam",   "SEEK_SET",
      "stderr",     "_IOFBF",    "EXIT_SUCCESS", "MB_CUR_MAX", "setjmp",
      "stdin",      "_IOLBF",    "FILENAME_MAX", "SIGABRT",    "stdout",
      "_IONBF",     "FOPEN_MAX", "SIGFPE",       "TMP_MAX",    "INT__MAX",
      "INT__MIN",   "sort"};
  std::set<std::string> builtin_macros_ = {"__LINE__",
                                           "__FILE__",
                                           "__DATE__",
                                           "__TIME__",
                                           "__STDC__",
                                           "__cplusplus",
                                           "__func__",
                                           "__STDCPP_DEFAULT_NEW_ALIGNMENT__",
                                           "__STDCPP_­BFLOAT16_­T__",
                                           "__STDCPP_­FLOAT16_­T__",
                                           "__STDCPP_FLOAT32_T__",
                                           "__STDCPP_FLOAT64_T__",
                                           "__STDCPP_FLOAT128_T__",
                                           "__STDC__",
                                           "__STDC_VERSION__",
                                           "__STDC_ISO_10646__",
                                           "__STDC_MB_MIGHT_NEQ_WC__",
                                           "__STDCPP_THREADS__",
                                           "__STDCPP_STRICT_POINTER_SAFETY__"};
  std::set<std::string> keywords_ = {
      "new",        "delete",       "and",         "not",
      "or",         "xor",          "bitand",      "bitor",
      "compl",      "and_eq",       "not_eq",      "or_eq",
      "xor_eq",     "false",        "true",        "template",
      "typename",   "dynamic_cast", "static_cast", "reinterpret_cast",
      "const_cast", "typeid",       "sizeof",      "case",
      "default",    "if",           "else",        "switch",
      "while",      "do",           "for",         "break",
      "continue",   "return",       "goto",        "friend",
      "typedef",    "auto",         "register",    "static",
      "extern",     "mutable",      "inline",      "virtual",
      "explicit",   "char",         "wchar_t",     "bool",
      "short",      "int",          "long",        "signed",
      "unsigned",   "float",        "double",      "void",
      "enum",       "namespace",    "using",       "asm",
      "extern",     "const",        "volatile",    "class",
      "struct",     "union",        "private",     "protected",
      "public",     "operator",     "try",         "catch",
      "throw",      "ifdef",        "ifndef",      "elif",
      "endif",      "include",      "define",      "undef",
      "pragma"};
};

class VarDeclVisitor : public clang::RecursiveASTVisitor<VarDeclVisitor> {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::SourceManager* source_manager);
  bool VisitVarDecl(clang::VarDecl* vd);

 private:
  analyzer::proto::ResultsList* results_list_;
  clang::SourceManager* source_manager_;
  std::set<std::string> objects_ = {
      "CHAR_BIT",       "FLT_DIG",        "INT_MIN",         "MB_LEN_MAX",
      "CHAR_MAX",       "FLT_EPSILON",    "LDBL_DIG",        "SCHAR_MAX",
      "CHAR_MIN",       "FLT_MANT_DIG",   "LDBL_EPSILON",    "SCHAR_MIN",
      "DBL_DIG",        "FLT_MAX",        "LDBL_MANT_DIG",   "SHRT_MAX",
      "DBL_EPSILON",    "FLT_MAX_10_EXP", "LDBL_MAX",        "SHRT_MIN",
      "DBL_MANT_DIG",   "FLT_MAX_EXP",    "LDBL_MAX_10_EXP", "UCHAR_MAX",
      "DBL_MAX",        "FLT_MIN",        "LDBL_MAX_EXP",    "UINT_MAX",
      "DBL_MAX_10_EXP", "FLT_MIN_10_EXP", "LDBL_MIN",        "ULONG_MAX",
      "DBL_MAX_EXP",    "FLT_MIN_EXP",    "LDBL_MIN_10_EXP", "USHRT_MAX",
      "DBL_MIN",        "FLT_RADIX",      "LDBL_MIN_EXP",    "DBL_MIN_10_EXP",
      "FLT_ROUNDS",     "LONG_MAX",       "DBL_MIN_EXP",     "INT_MAX",
      "LONG_MIN"};
};

class VarDeclConsumer : public clang::ASTConsumer {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            clang::SourceManager* source_manager) {
    results_list_ = results_list;
    visitor_.Init(results_list, source_manager);
  }
  virtual void HandleTranslationUnit(clang::ASTContext& context) {
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
  }

 private:
  VarDeclVisitor visitor_;
  analyzer::proto::ResultsList* results_list_;
  clang::SourceManager* source_manager_;
};

class Action : public clang::ASTFrontendAction {
 public:
  Action(analyzer::proto::ResultsList* results_list)
      : results_list_(results_list) {}
  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::make_unique<clang::ASTConsumer>();
  }
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, llvm::StringRef in_file) {
    auto consumer = std::make_unique<VarDeclConsumer>();
    consumer->Init(results_list_, &ci.getSourceManager());
    return consumer;
  }

  bool BeginSourceFileAction(clang::CompilerInstance& ci);

 private:
  analyzer::proto::ResultsList* results_list_;
};

class Checker : public clang::tooling::FrontendActionFactory {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<Action>(results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace libtooling
}  // namespace rule_17_0_1
}  // namespace misra_cpp_2008
#endif  // ANALYZER_MISRA_CPP_2008_RULE_17_0_1_LIBTOOLING_CHECKER_H_
