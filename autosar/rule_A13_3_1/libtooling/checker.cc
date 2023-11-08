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

#include "autosar/rule_A13_3_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A function that contains \"forwarding reference\" as its argument shall not be overloaded.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A13_3_1 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("function_decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* function_decl =
        result.Nodes.getNodeAs<FunctionDecl>("function_decl");
    const auto forwarding_ref_function_iter = forwarding_ref_functions.find(
        function_decl->getQualifiedNameAsString());

    if (forwarding_ref_function_iter ==
        forwarding_ref_functions
            .end()) {  // no function with the same name found
      // if has forwarding reference param
      if (const FunctionTemplateDecl* template_decl =
              function_decl->getDescribedFunctionTemplate()) {
        if (functionHasAnyForwardingReferenceParam(function_decl->parameters(),
                                                   template_decl)) {
          // let's ignore forwarding constructors for now
          // we will check it with clang-tidy
          // bugprone-forwarding-reference-overload
          if (!dyn_cast<CXXConstructorDecl>(function_decl)) {
            forwarding_ref_functions.emplace(
                function_decl->getQualifiedNameAsString(), function_decl);
          }
        }
      }
    } else {  // function with the same name found
      const FunctionDecl* rvalueref_function_decl =
          forwarding_ref_function_iter->second;

      // excluding an exception to the rule: =deleted
      if (function_decl->isDeleted()) {
        return;
      }

      // checks for overloading
      if (areTwoFunctionsInSameScope(rvalueref_function_decl, function_decl) &&
          isFunctionOverloaded(rvalueref_function_decl, function_decl)) {
        std::string path = misra::libtooling_utils::GetFilename(
            function_decl, result.SourceManager);
        int line_number = misra::libtooling_utils::GetLine(
            function_decl, result.SourceManager);
        ReportError(path, line_number, results_list_);
      }
      return;
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;

  // <key, value> = <function_name, function_decl>
  std::unordered_map<std::string, const FunctionDecl*> forwarding_ref_functions;

  // overloading requires both functions to be in the same scope
  bool areTwoFunctionsInSameScope(const FunctionDecl* first_function_decl,
                                  const FunctionDecl* second_function_decl) {
    return first_function_decl->getParent() ==
           second_function_decl->getParent();
  }

  // checks for forwarding reference in the function parameter list
  bool functionHasAnyForwardingReferenceParam(
      const ArrayRef<ParmVarDecl*> function_params,
      const FunctionTemplateDecl* template_decl) {
    for (const auto* param : function_params) {
      if (misra::libtooling_utils::isForwardingReference(
              param->getType(), template_decl->getTemplateDepth())) {
        return true;
      }
    }
    return false;
  }

  // checks for overloading
  bool isFunctionOverloaded(const FunctionDecl* overloaded_function_decl,
                            const FunctionDecl* function_decl) {
    const auto& overloaded_function_params =
        overloaded_function_decl->parameters();
    const auto& function_params = function_decl->parameters();

    // if the number of parameters differs, it must be overloaded
    if (overloaded_function_params.size() != function_params.size()) {
      return true;
    } else {
      // else, if the types of corresponding parameters differs, it must be
      // overloaded
      for (size_t i = 0; i < overloaded_function_params.size(); ++i) {
        const QualType overloaded_function_param_type =
            overloaded_function_params[i]->getType();
        const QualType function_param_type = function_params[i]->getType();

        if (overloaded_function_param_type.getCanonicalType() !=
            function_param_type.getCanonicalType()) {
          return true;
        }
      }
      return false;
    }
  }
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A13_3_1
}  // namespace autosar