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

#include "autosar/rule_A14_7_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const string& path, int line_number,
                 ResultsList* results_list) {
  string error_message =
      "A type used as a template argument shall provide all members that are used by the template.";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A14_7_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(classTemplateSpecializationDecl().bind("ctsd"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const ClassTemplateSpecializationDecl* ctsd =
        result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("ctsd");
    if (ctsd) {
      SourceLocation point_of_instantiation = ctsd->getPointOfInstantiation();
      if ((point_of_instantiation.isValid() &&
           result.SourceManager->isInSystemHeader(point_of_instantiation)) ||
          (point_of_instantiation.isInvalid() &&
           IsInSystemHeader(ctsd, result.Context)))
        return;
      CXXRecordDecl* template_crd =
          ctsd->getSpecializedTemplate()->getTemplatedDecl();
      ASTVisitor visitor;
      visitor.TraverseDecl(template_crd);
      bool exist_undeclared_member = false;
      for (unsigned i = 0; i < ctsd->getTemplateArgs().size(); i++) {
        const TemplateArgument arg = ctsd->getTemplateArgs()[i];
        const CXXRecordDecl* arg_crd = arg.getAsType()->getAsCXXRecordDecl();
        for (const CXXDependentScopeMemberExpr* cdsme :
             visitor.getDependentMemberExprs()) {
          const TemplateTypeParmType* ttpt =
              dyn_cast<TemplateTypeParmType>(cdsme->getBaseType());
          if (i == ttpt->getIndex() &&
              !hasMember(arg_crd, cdsme->getMember().getAsString())) {
            exist_undeclared_member = true;
            break;
          }
        }
        if (exist_undeclared_member) break;
      }
      if (!exist_undeclared_member) return;
      if (point_of_instantiation.isValid()) {  // Instantiation
        ReportError(
            GetLocationFilename(point_of_instantiation, result.SourceManager),
            GetLocationLine(point_of_instantiation, result.SourceManager),
            results_list_);
      } else {  // Explicit Specialization
        ReportError(GetFilename(ctsd, result.SourceManager),
                    GetLine(ctsd, result.SourceManager), results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
  bool hasMember(const CXXRecordDecl* crd, string member) {
    if (!crd) return false;
    for (const FieldDecl* field : crd->fields()) {
      if (field->getNameAsString() == member) return true;
    }
    for (const CXXMethodDecl* method : crd->methods()) {
      if (method->getNameAsString() == member) return true;
    }
    return false;
  }
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A14_7_1
}  // namespace autosar
