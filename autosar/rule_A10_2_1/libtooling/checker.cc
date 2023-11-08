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

#include "autosar/rule_A10_2_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Non-virtual public or protected member functions shall not be redefined in derived classes.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A10_2_1 {
namespace libtooling {

bool isRedefined(const CXXMethodDecl* a, const CXXMethodDecl* b) {
  auto* identifierA = a->getIdentifier();
  auto* identifierB = b->getIdentifier();
  if (!identifierA || !identifierB) {
    return false;
  }
  if (identifierA->getName() == identifierB->getName() &&
      a->getType() == b->getType()) {
    return true;
  } else {
    return false;
  }
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        cxxMethodDecl(
            isUserProvided(),
            hasAncestor(
                cxxRecordDecl(hasAnyBase(unless(isPrivate()))).bind("rd")),
            unless(isExpansionInSystemHeader()))
            .bind("decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXMethodDecl* decl = result.Nodes.getNodeAs<CXXMethodDecl>("decl");
    const CXXRecordDecl* rd = result.Nodes.getNodeAs<CXXRecordDecl>("rd");

    for (auto base : rd->bases()) {
      auto* baseRecordDecl = base.getType()->getAsCXXRecordDecl();
      if (!baseRecordDecl) {
        continue;
      }
      for (auto method : baseRecordDecl->methods()) {
        if (method->isUserProvided() && isRedefined(method, decl)) {
          if (!method->isVirtual() &&
              method->getAccess() != clang::AS_private) {
            std::string path = misra::libtooling_utils::GetFilename(
                decl, result.SourceManager);
            int line_number =
                misra::libtooling_utils::GetLine(decl, result.SourceManager);
            ReportError(path, line_number, results_list_);
            return;
          }
          break;  // if this is a compliant redefinition, then check next base.
                  // see bad case 4
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A10_2_1
}  // namespace autosar
