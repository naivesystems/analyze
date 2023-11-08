/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "misra/rule_8_2/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

string getDeclName(const NamedDecl* decl) { return decl->getNameAsString(); }

void ReportFuncDeclParmNotNamedError(string name, string parm_path,
                                     int parm_line, ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\n"
      "unnamed parameter.\n"
      "function name: %s",
      name);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, parm_path, parm_line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_2_FUNC_DECL_PARM_NOT_NAMED_ERROR);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

void ReportFuncPointerParmNotNamedError(string parm_path, int parm_line,
                                        ResultsList* results_list) {
  std::string error_message =
      "[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\n"
      "function pointer with unnamed parameter.";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, parm_path, parm_line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_2_FUNC_POINTER_PARM_NOT_NAMED_ERROR);
  LOG(INFO) << error_message;
}

void ReportFuncDeclKRStyleError(string name, string parm_path, int parm_line,
                                ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\n"
      "K&R style is fobidden.\n"
      "function name: %s",
      name);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, parm_path, parm_line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_2_FUNC_DECL_KR_STYLE_ERROR);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

void ReportFuncDeclVoidError(string name, string parm_path, int parm_line,
                             ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C0513][misra-c2012-8.2]: function types shall be in prototype form with named parameters\n"
      "Missing 'void'.\n"
      "function name: %s",
      name);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, parm_path, parm_line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_8_2_FUNC_DECL_VOID_ERROR);
  pb_result->set_name(name);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_8_2 {

class FuncParmVarDeclCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(namedDecl().bind("functionParmCheck"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const NamedDecl* nd =
        result.Nodes.getNodeAs<NamedDecl>("functionParmCheck");
    ASTContext* context = result.Context;
    const FunctionType* ft = nd->getFunctionType();

    const SourceLocation loc = nd->getLocation();
    if (loc.isInvalid() || context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }

    if (isa<ParmVarDecl>(nd)) {
      // non-void parameter
      CheckParmVarDecl(cast<ParmVarDecl>(nd), result.SourceManager);
    } else if (ft) {
      // void parameter
      CheckFunctionType(nd, ft, result.SourceManager);
    }
  }

  void CheckFunctionType(const NamedDecl* nd, const FunctionType* ft,
                         SourceManager* source_manager) {
    if (ft->isFunctionNoProtoType()) {
      ReportFuncDeclVoidError(
          getDeclName(nd), libtooling_utils::GetFilename(nd, source_manager),
          libtooling_utils::GetLine(nd, source_manager), results_list_);
    }
  }

  void CheckParmVarDecl(const ParmVarDecl* pvd, SourceManager* source_manager) {
    const DeclContext* parent = pvd->getParentFunctionOrMethod();
    if (parent) {
      // check parameter declaration in function declaration
      if (isa<FunctionDecl>(parent)) {
        CheckDeclInFuncDecl(pvd, cast<FunctionDecl>(parent), source_manager);
      }
    } else {
      // check parameter declaration in function pointer
      CheckDeclInFuncPointer(pvd, source_manager);
    }
  }

  void CheckDeclInFuncDecl(const ParmVarDecl* pvd, const FunctionDecl* decl,
                           SourceManager* source_manager) {
    string function_name = getDeclName(decl);
    string parm_name = getDeclName(pvd);
    string parm_path = libtooling_utils::GetFilename(pvd, source_manager);
    int parm_line = libtooling_utils::GetLine(pvd, source_manager);

    // unnamed parameter
    if (parm_name.empty()) {
      ReportFuncDeclParmNotNamedError(function_name, parm_path, parm_line,
                                      results_list_);
      return;
    }

    if (decl->isThisDeclarationADefinition()) {
      // K&R style non-void function
      if (source_manager->isBeforeInSLocAddrSpace(
              decl->getFunctionTypeLoc().getRParenLoc(), pvd->getLocation())) {
        ReportFuncDeclKRStyleError(function_name, parm_path, parm_line,
                                   results_list_);
      }
    }
  }

  void CheckDeclInFuncPointer(const ParmVarDecl* pvd,
                              SourceManager* source_manager) {
    string parm_name = getDeclName(pvd);
    string parm_path = libtooling_utils::GetFilename(pvd, source_manager);
    int parm_line = libtooling_utils::GetLine(pvd, source_manager);
    if (parm_name.empty())
      ReportFuncPointerParmNotNamedError(parm_path, parm_line, results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  func_parm_named_callback_ = new FuncParmVarDeclCallback;
  func_parm_named_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_8_2
}  // namespace misra
