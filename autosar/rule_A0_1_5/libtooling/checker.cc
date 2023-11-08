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

#include "autosar/rule_A0_1_5/libtooling/checker.h"

#include <glog/logging.h>

#include <unordered_map>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kVirtualFuncDeclString[] = "virtualFuncDecl";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "There shall be no unused named parameters in the set of parameters for a virtual function and all the functions that override it.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

std::string GetFunctionSignature(const FunctionDecl* func_decl) {
  std::string signature = func_decl->getQualifiedNameAsString();
  signature += "(";
  for (const ParmVarDecl* param : func_decl->parameters()) {
    signature += param->getOriginalType().getAsString();
    signature += ",";
  }
  signature += ")";
  return signature;
}

}  // namespace

namespace autosar {
namespace rule_A0_1_5 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(cxxMethodDecl(isVirtual(), unless(isImplicit()))
                         .bind(kVirtualFuncDeclString),
                     this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXMethodDecl* func_decl =
      result.Nodes.getNodeAs<CXXMethodDecl>(kVirtualFuncDeclString);
  VirtualFuncInfo func_info;
  func_info.path =
      misra::libtooling_utils::GetFilename(func_decl, result.SourceManager);
  func_info.line_number =
      misra::libtooling_utils::GetLine(func_decl, result.SourceManager);
  func_info.params_used_info_.resize(func_decl->param_size());
  for (int i = 0; i < func_decl->param_size(); ++i) {
    const ParmVarDecl* param = func_decl->getParamDecl(i);
    func_info.params_used_info_[i] = param->getName().empty() ||
                                     param->hasAttr<UnusedAttr>() ||
                                     param->isThisDeclarationReferenced();
  }
  for (const CXXMethodDecl* overridden_method_decl :
       func_decl->overridden_methods()) {
    func_info.overridden_method_names_.emplace_back(
        GetFunctionSignature(overridden_method_decl));
  }
  std::string func_decl_sig = GetFunctionSignature(func_decl);
  std::unordered_map<std::string, VirtualFuncInfo>::iterator func_info_it =
      funcs_info_.find(func_decl_sig);
  if (func_info_it != funcs_info_.end()) {
    func_info_it->second = std::move(func_info);
  } else {
    funcs_info_.insert(std::make_pair(func_decl_sig, std::move(func_info)));
  }
  MarkFuncParamsUsed(func_decl_sig);
}

void Callback::MarkFuncParamsUsed(const std::string& func_decl_sig) {
  // This function is used to update the status of functions. It walks through
  // all the base functions of the assigned one, and mark the used parameters of
  // the base functions. Then, it invokes this method on the base functions
  // recursively. When the function has no base functions, it returns. In this
  // way virtual functions could be updated on whether their parameters have
  // been used by themselves or their derived functions.
  std::unordered_map<std::string, VirtualFuncInfo>::iterator func_info_it =
      funcs_info_.find(func_decl_sig);
  if (func_info_it == funcs_info_.end()) {
    return;
  }
  for (const std::string& overridden_method_name :
       func_info_it->second.overridden_method_names_) {
    std::unordered_map<std::string, VirtualFuncInfo>::iterator
        overridden_method_it = funcs_info_.find(overridden_method_name);
    if (overridden_method_it == funcs_info_.end()) {
      continue;
    }
    for (unsigned int i = 0; i < func_info_it->second.params_used_info_.size();
         ++i) {
      if (func_info_it->second.params_used_info_[i]) {
        overridden_method_it->second.params_used_info_[i] = true;
      }
    }
    MarkFuncParamsUsed(overridden_method_name);
  }
}

void Callback::Report() {
  // Walk through the map and try to whether there exists a function which has
  // unused parameters. If so report it.
  for (const std::pair<std::string, VirtualFuncInfo>& func_it : funcs_info_) {
    for (bool is_used : func_it.second.params_used_info_) {
      if (!is_used) {
        ReportError(func_it.second.path, func_it.second.line_number,
                    results_list_);
        break;
      }
    }
  }
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

void Checker::Report() { callback_->Report(); }

}  // namespace libtooling
}  // namespace rule_A0_1_5
}  // namespace autosar
