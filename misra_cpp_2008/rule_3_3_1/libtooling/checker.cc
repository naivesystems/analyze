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

#include "misra_cpp_2008/rule_3_3_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;
using llvm::isa;

namespace {
void ReportError(string path, int line_number, ResultsList* results_list_) {
  std::string error_message =
      absl::StrFormat("含有外部链接的对象或函数必须在一个头文件中声明");
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_3_3_1);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_3_1 {
namespace libtooling {

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  finder_.addMatcher(
      namedDecl(anyOf(varDecl(), functionDecl()), hasExternalFormalLinkage())
          .bind("decl"),
      this);
}

void Checker::run(const MatchFinder::MatchResult& result) {
  clang::ASTContext* context = result.Context;
  const NamedDecl* decl_const =
      result.Nodes.getNodeAs<clang::NamedDecl>("decl");
  auto decl = const_cast<NamedDecl*>(decl_const);
  FullSourceLoc location = context->getFullLoc(decl->getBeginLoc());
  if (location.isValid() && !location.isInSystemHeader()) {
    if (misra::libtooling_utils::isInHeader(decl_const, result.SourceManager)) {
      setDeclsInHeader(decl);
    } else {
      checkDeclsInSource(decl, result.SourceManager);
    }
  }
}

bool Checker::hasDefinition(NamedDecl* decl) {
  if (isa<VarDecl>(decl)) {
    auto varDecl = cast<VarDecl>(decl);
    return varDecl->hasDefinition() != VarDecl::DeclarationOnly;
  }
  if (isa<FunctionDecl>(decl)) {
    auto funcDecl = cast<FunctionDecl>(decl);
    return funcDecl->isDefined();
  }
  return false;
}

bool Checker::isDefinition(NamedDecl* decl) {
  if (isa<VarDecl>(decl)) {
    auto varDecl = cast<VarDecl>(decl);
    return varDecl->hasInit();
  }
  if (isa<FunctionDecl>(decl)) {
    auto funcDecl = cast<FunctionDecl>(decl);
    return funcDecl->isThisDeclarationADefinition();
  }
  return false;
}

bool Checker::isMainFunc(NamedDecl* decl) {
  if (isa<FunctionDecl>(decl)) {
    return cast<FunctionDecl>(decl)->isMain();
  }
  return false;
}

NamedDecl* Checker::getDefinition(NamedDecl* decl) {
  if (isa<FunctionDecl>(decl)) {
    return cast<FunctionDecl>(decl)->getDefinition();
  }
  if (isa<VarDecl>(decl)) {
    return cast<VarDecl>(decl)->getDefinition();
  }
  return nullptr;
}

void Checker::setDeclsInHeader(NamedDecl* decl) {
  if (isa<VarDecl>(decl) || isa<FunctionDecl>(decl)) {
    if (auto def = getDefinition(decl)) {
      if (decl_state_.find(def) == decl_state_.end()) {
        decl_state_[def] = DECLARED_IN_HEADER;
      } else {
        decl_state_.erase(def);
      }
    }
  }
}

void Checker::checkDeclsInSource(NamedDecl* decl,
                                 SourceManager* sourceManager) {
  if (!decl->isExternallyVisible() || isMainFunc(decl)) {
    return;
  }
  if (isDefinition(decl)) {
    if (decl_state_.find(decl) == decl_state_.end()) {
      decl_state_[decl] = DECLARED_WITH_EXETRNAL_LINKAGE;
      std::string filename =
          misra::libtooling_utils::GetFilename(decl, sourceManager);
      int line_number = misra::libtooling_utils::GetLine(decl, sourceManager);
      decl_record_[decl] = make_pair(filename, line_number);
    } else {
      decl_state_.erase(decl);
    }
  } else if (!hasDefinition(decl)) {
    ReportError(misra::libtooling_utils::GetFilename(decl, sourceManager),
                misra::libtooling_utils::GetLine(decl, sourceManager),
                results_list_);
  }
}

void Checker::reportInValidDecl() {
  for (auto pair : decl_state_) {
    if (pair.second == DECLARED_WITH_EXETRNAL_LINKAGE) {
      ReportError(decl_record_[pair.first].first,
                  decl_record_[pair.first].second, results_list_);
    }
  }
}
}  // namespace libtooling
}  // namespace rule_3_3_1
}  // namespace misra_cpp_2008
