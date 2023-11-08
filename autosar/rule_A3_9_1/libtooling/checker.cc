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

#include "autosar/rule_A3_9_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Fixed width integer types from <cstdint>, indicating the size and "
      "signedness, shall be used in place of the basic numerical types.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A3_9_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  auto const& cast_bind = optionally(
      has(implicitCastExpr(hasCastKind(CK_IntegralCast)).bind("cast")));
  finder->addMatcher(
      varDecl(unless(isExpansionInSystemHeader()), cast_bind).bind("var"),
      this);
  finder->addMatcher(
      fieldDecl(unless(isExpansionInSystemHeader()), cast_bind).bind("var"),
      this);
  finder->addMatcher(
      parmVarDecl(unless(isExpansionInSystemHeader()), cast_bind).bind("var"),
      this);
  finder->addMatcher(
      functionDecl(unless(isExpansionInSystemHeader())).bind("func"), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const ValueDecl* var = result.Nodes.getNodeAs<ValueDecl>("var");
  const FunctionDecl* func = result.Nodes.getNodeAs<FunctionDecl>("func");
  if ((!var) && (!func)) return;
  const ImplicitCastExpr* imp_cast =
      result.Nodes.getNodeAs<ImplicitCastExpr>("cast");
  clang::QualType type;
  if (var) {
    type = var->getType();
  } else {
    if (func->isMain()) return;
    type = func->getReturnType();
  }
  while (type->isPointerType()) type = type->getPointeeType();
  while (type->isArrayType())
    type =
        cast<ArrayType>(type->getUnqualifiedDesugaredType())->getElementType();
  std::string typeString = type.getAsString();
  // refer to https://en.cppreference.com/w/cpp/language/types
  const std::string notAllowedTypes[] = {
      "signed char",
      "unsigned char",
      "short",
      "short int",
      "signed short",
      "signed short int",
      "unsigned short",
      "unsigned short int",
      "int",
      "signed",
      "signed int",
      "unsigned",
      "unsigned int",
      "long",
      "long int",
      "signed long",
      "signed long int",
      "unsigned long",
      "unsigned long int",
      "long long",
      "long long int",
      "signed long long",
      "signed long long int",
      "unsigned long long",
      "unsigned long long int",
  };
  bool found = std::find(std::begin(notAllowedTypes), std::end(notAllowedTypes),
                         typeString) != std::end(notAllowedTypes);
  if (!found && !(typeString == "char" && imp_cast)) return;
  const Decl* decl = var;
  if (!var) decl = func;
  std::string path =
      misra::libtooling_utils::GetFilename(decl, result.SourceManager);
  int line_number =
      misra::libtooling_utils::GetLine(decl, result.SourceManager);
  ReportError(path, line_number, results_list_);
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A3_9_1
}  // namespace autosar
