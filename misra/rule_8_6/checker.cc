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

#include "misra/rule_8_6/checker.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <glog/logging.h>

#include "absl/strings/str_format.h"

namespace misra {
namespace rule_8_6 {

using clang::ASTContext;
using clang::VarDecl;
using clang::ast_matchers::DeclarationMatcher;
using clang::ast_matchers::MatchFinder;
using clang::ast_matchers::varDecl;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::unordered_map;

using analyzer::proto::ResultsList;
using proto_util::AddResultToResultsList;

CountExternalIdentifierDefinitionCallback::
    CountExternalIdentifierDefinitionCallback(ResultsList* results_list) {
  results_list_ = results_list;
}

void CountExternalIdentifierDefinitionCallback::run(
    const MatchFinder::MatchResult& result) {
  ASTContext* context = result.Context;
  const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("variableDefinition");

  clang::FullSourceLoc location = context->getFullLoc(vd->getBeginLoc());
  if (location.isInvalid() || location.isInSystemHeader()) {
    return;
  }

  if (!vd->isFileVarDecl()) {
    // skip non file scope declaration
    return;
  }
  if (vd->isThisDeclarationADefinition() ==
      VarDecl::DefinitionKind::DeclarationOnly) {
    // ignore DeclarationOnly case.
    return;
  }
  shared_ptr<VariableDeclaration> declaration =
      make_shared<VariableDeclaration>(result.SourceManager, vd);

  if (!HandleDeclarationInSameFile(declaration)) return;

  HandleDeclarationInDifferentFile(declaration);
}

bool CountExternalIdentifierDefinitionCallback::HandleDeclarationInSameFile(
    shared_ptr<VariableDeclaration> declaration) {
  string varname = declaration->name_;
  string begin_loc = declaration->begin_loc_;
  string filename = begin_loc.substr(0, begin_loc.find(":"));

  auto entry_iter = file_var_decl_map_.find(filename);
  if (entry_iter == file_var_decl_map_.end()) {
    unordered_map<string, shared_ptr<VariableDeclaration>> new_var_decl_map;
    new_var_decl_map[varname] = declaration;
    file_var_decl_map_[filename] = new_var_decl_map;
  } else {
    auto exist_var_decl_map = entry_iter->second;
    auto iter = exist_var_decl_map.find(varname);
    if (iter == exist_var_decl_map.end()) {
      exist_var_decl_map[varname] = declaration;
    } else {
      auto old_vd = iter->second;
      if (old_vd->IsDefinition() && declaration->IsDefinition()) {
        ReportError(old_vd.get(), declaration.get());
        return false;
      } else if (declaration->IsDefinition()) {
        exist_var_decl_map[varname] = declaration;
      }
    }
  }
  return true;
}

bool CountExternalIdentifierDefinitionCallback::
    HandleDeclarationInDifferentFile(
        shared_ptr<VariableDeclaration> declaration) {
  string varname = declaration->name_;
  string begin_loc = declaration->begin_loc_;
  string filename = begin_loc.substr(0, begin_loc.find(":"));

  // find in definition map
  auto definition_iter = definition_map_.find(varname);
  if (definition_iter == definition_map_.end()) {
    if (declaration->IsDefinition()) {
      definition_map_[varname] = declaration;
    }
  } else {
    auto old_vd = definition_iter->second;
    string old_filename = old_vd->begin_loc_.substr(0, begin_loc.find(":"));
    if (old_filename != filename) {
      ReportError(old_vd.get(), declaration.get());
      return false;
    }
  }

  // find in tentative definition map
  auto tentative_iter = tentative_definition_map_.find(varname);
  if (declaration->IsDefinition()) {
    if (tentative_iter == tentative_definition_map_.end()) return true;
    auto old_vd = tentative_iter->second;
    string old_filename = old_vd->begin_loc_.substr(0, begin_loc.find(":"));
    if (old_filename != filename) {
      ReportError(old_vd.get(), declaration.get());
      return false;
    }
  } else {
    // only can be tentative definition
    if (tentative_iter == tentative_definition_map_.end()) {
      tentative_definition_map_[varname] = declaration;
    }
  }

  return true;
}

void CountExternalIdentifierDefinitionCallback::ReportError(
    const VariableDeclaration* declaration1,
    const VariableDeclaration* declaration2) const {
  string error_message = absl::StrFormat(
      "[C0509][misra-c2012-8.6]: error duplicated definition\n"
      "duplicated variable name: %s\n"
      "first definition location: %s\n"
      "duplicated definition location: %s",
      declaration1->name_, declaration1->begin_loc_, declaration2->begin_loc_);
  LOG(INFO) << error_message;
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, declaration1->path,
                             declaration1->line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_8_6);
  pb_result->set_name(declaration1->name_);
  pb_result->set_loc(declaration1->begin_loc_);
  pb_result->set_other_loc(declaration2->begin_loc_);
}

Checker::Checker(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = CountExternalIdentifierDefinitionCallback(results_list_);
  DeclarationMatcher varDeclMatcher = varDecl().bind("variableDefinition");
  finder_.addMatcher(varDeclMatcher, &callback_);
}

}  // namespace rule_8_6
}  // namespace misra
