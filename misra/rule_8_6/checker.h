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

#ifndef ANALYZER_MISRA_RULE_8_6_CHECKER_H_
#define ANALYZER_MISRA_RULE_8_6_CHECKER_H_

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <unordered_map>

#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"

namespace misra {
namespace rule_8_6 {

using clang::VarDecl;
using clang::ast_matchers::MatchFinder;
using std::shared_ptr;
using std::string;
using std::unordered_map;

using analyzer::proto::ResultsList;

/**
 * In C, pure declarations (that are not also definitions) are preceded with
 * the keyword "extern". C has a special "tentative definition" rule which
 * allows multiple definitions for the same variable in the same translation
 * unit so long as they all match and at most one has an initializer.
 * So one variable declaration could have three definition kind:
 * DeclarationOnly, TentativeDefinition and Definition.
 *
 * Misra-c2012-8.6 allows one variable has mutiple tentative definition and
 * one definition in same transaction unit, but doesn't allow tentative
 * definition and definition in cross transaction unit.
 *
 * In our implemetation, we will find definition in same file first. If we
 * find two definition variables have same name in one file, we will report
 * error. To implement this, we hold a two dimension map file_var_decl_map:
 * filename -> variable name -> VariableDeclaration
 * When we meet a new variable declaration, we will try to find a exist
 * old variable which in same file and with same name. If we can't find a
 * old one, then we insert the new to map. If we found it and both the new
 * and the old are definition, then report error. If only the new one is
 * definition, then we update the map by setting the new one to the value.
 *
 * Then we hold two map for global state, one named definition_map, another
 * named tentative_definition_map. We will try to find a value in
 * definition_map. If we find one and they are not in same file, then report
 * error. Then we try to find a value in tentative_definition_map, if we find
 * one and current variable_declaration is definition and they are not in
 * same file, then report error.
 */
struct VariableDeclaration {
  string name_;
  string begin_loc_;
  VarDecl::DefinitionKind definition_kind_;
  string path;
  int line_number;

  VariableDeclaration(clang::SourceManager* source_manager,
                      const clang::VarDecl* decl) {
    this->name_ = decl->getNameAsString();
    this->begin_loc_ = libtooling_utils::GetLocation(decl, source_manager);
    this->definition_kind_ = decl->isThisDeclarationADefinition();
    this->path = libtooling_utils::GetFilename(decl, source_manager);
    this->line_number = libtooling_utils::GetLine(decl, source_manager);
  }

  bool IsDefinition() const {
    if (definition_kind_ == VarDecl::DefinitionKind::Definition) {
      return true;
    }
    return false;
  }
};

class CountExternalIdentifierDefinitionCallback
    : public MatchFinder::MatchCallback {
 public:
  explicit CountExternalIdentifierDefinitionCallback(){};
  explicit CountExternalIdentifierDefinitionCallback(ResultsList* results_list);
  void run(const MatchFinder::MatchResult& result) override;
  bool HandleDeclarationInSameFile(shared_ptr<VariableDeclaration> declaration);
  bool HandleDeclarationInDifferentFile(
      shared_ptr<VariableDeclaration> declaration);

 private:
  unordered_map<string, shared_ptr<VariableDeclaration>>
      tentative_definition_map_;
  unordered_map<string, shared_ptr<VariableDeclaration>> definition_map_;
  unordered_map<string, unordered_map<string, shared_ptr<VariableDeclaration>>>
      file_var_decl_map_;
  ResultsList* results_list_;

  void ReportError(const VariableDeclaration* declaration1,
                   const VariableDeclaration* declaration2) const;
};

class Checker {
 public:
  explicit Checker(ResultsList* results_list);
  MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  MatchFinder finder_;
  CountExternalIdentifierDefinitionCallback callback_;
  ResultsList* results_list_;
};

}  // namespace rule_8_6
}  // namespace misra

#endif
