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

#include "misra_cpp_2008/rule_2_10_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using analyzer::proto::ResultsList;
using namespace llvm;

namespace {
void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "在内部作用域声明的标识符不得隐藏在外部作用域声明的标识符";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

bool isPotenialVarHiddenContext(const DeclContext* context) {
  return context && (isa<FunctionDecl>(context) ||
                     (isa<NamespaceDecl>(context) &&
                      cast<NamespaceDecl>(context)->isAnonymousNamespace()) ||
                     (isa<CXXRecordDecl>(context) &&
                      cast<CXXRecordDecl>(context)->isLambda()));
}

// Check whether this variable is declared in a lambda function and not
// captured by this lambda function.
bool isInLambdaFunctionNotInCaptures(const NamedDecl* decl) {
  const DeclContext* context = decl->getDeclContext();
  if (const CXXMethodDecl* cxx_method_decl = dyn_cast<CXXMethodDecl>(context)) {
    if (const CXXRecordDecl* cxx_record_decl = cxx_method_decl->getParent()) {
      if (cxx_record_decl->isLambda()) {
        bool existSameNameInCaptures = false;
        for (const LambdaCapture& capture : cxx_record_decl->captures()) {
          if (capture.capturesVariable() &&
              capture.getCapturedVar()->getNameAsString() ==
                  decl->getNameAsString()) {
            existSameNameInCaptures = true;
            break;
          }
        }
        if (!existSameNameInCaptures) {
          return true;
        }
      }
    }
  }
  return false;
}

bool existHiddenVar(const VarDecl* var_decl) {
  // There are two steps to check whether a variable is hidden. The first step
  // is to check its siblings. This could help to find cases like
  // ```
  // int x;
  // {
  // int x;
  // }
  // ```
  for (const Decl* decl : var_decl->getDeclContext()->decls()) {
    if (decl == var_decl) {
      break;
    } else if (const NamedDecl* named_decl = dyn_cast<NamedDecl>(decl)) {
      if (named_decl->getNameAsString() == var_decl->getNameAsString()) {
        return !isInLambdaFunctionNotInCaptures(var_decl);
      }
    }
  }
  // The next step is to walk through its ancestors and try to find a same-name
  // identifier. The loop will only continue when the ancestor context are in
  // the following cases:
  // 1. a cxx record declaration if it's a lambda function class
  // 2. a anonymous namespace declaration
  // 3. a function declaration
  for (const DeclContext* context = var_decl->getDeclContext();
       isPotenialVarHiddenContext(context); context = context->getParent()) {
    for (const Decl* decl : context->getParent()->decls()) {
      if (const NamedDecl* named_decl = dyn_cast<NamedDecl>(decl)) {
        if (named_decl->getNameAsString() == var_decl->getNameAsString()) {
          return !isInLambdaFunctionNotInCaptures(var_decl);
        }
      }
    }
  }
  return false;
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_2_10_2 {
namespace libtooling {
constexpr char kVarDeclString[] = "varDecl";

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder);
  void run(const MatchFinder::MatchResult& result);

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(varDecl().bind(kVarDeclString), this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>(kVarDeclString);
  FullSourceLoc location = result.Context->getFullLoc(var_decl->getBeginLoc());
  if (var_decl->getStorageClass() == SC_Extern ||
      var_decl->getNameAsString().empty() || location.isInvalid() ||
      location.isInSystemHeader()) {
    return;
  }
  if (existHiddenVar(var_decl)) {
    std::string path =
        misra::libtooling_utils::GetFilename(var_decl, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(var_decl, result.SourceManager);
    ReportError(path, line_number, results_list_);
  }
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_2_10_2
}  // namespace misra_cpp_2008
