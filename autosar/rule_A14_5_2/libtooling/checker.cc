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

#include "autosar/rule_A14_5_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/QualTypeNames.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Class members that are not dependent on template class parameters should be defined in a separate base class.";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A14_5_2 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxRecordDecl(isClass(), hasDefinition(),
                                     hasParent(classTemplateDecl()),
                                     unless(isExpansionInSystemHeader()))
                           .bind("crd"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CXXRecordDecl* crd = result.Nodes.getNodeAs<CXXRecordDecl>("crd");
    for (const Decl* child : crd->decls()) {
      bool need_report = false;
      if (const FieldDecl* fd = dynamic_cast<const FieldDecl*>(child)) {
        const Expr* init = fd->getInClassInitializer();
        if (!fd->getType()->isDependentType() && (!init || !isDependent(init)))
          need_report = true;
      } else if (const TagDecl* nested_TD =
                     dynamic_cast<const TagDecl*>(child)) {
        if (!ContainsDependentMember(nested_TD) && !nested_TD->isImplicit())
          need_report = true;
      } else if (const CXXMethodDecl* cmd =
                     dynamic_cast<const CXXMethodDecl*>(child)) {
        if (!cmd->getType()->isDependentType() && !ContainsDependentMember(cmd))
          need_report = true;
      }

      if (need_report)
        ReportError(GetFilename(child, result.SourceManager),
                    GetLine(child, result.SourceManager), results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  bool ContainsDependentMember(const DeclContext* dc) {
    if (!dc) return false;
    for (const Decl* child : dc->decls()) {
      const Expr* init = nullptr;
      if (const EnumConstantDecl* ecd =
              dynamic_cast<const EnumConstantDecl*>(child)) {
        init = ecd->getInitExpr();
      } else if (const FieldDecl* fd = dynamic_cast<const FieldDecl*>(child)) {
        init = fd->getInClassInitializer();
      } else if (const VarDecl* fd = dynamic_cast<const VarDecl*>(child)) {
        init = fd->getInit();
      }
      if (init && isDependent(init)) return true;

      if (const ValueDecl* vd = dynamic_cast<const ValueDecl*>(child)) {
        // This section is checking whether there exists a dependent type in
        // decls. However, EnumConstants have type of their parent enum and
        // enums are dependent when they are under a templated class no
        // matter if there exists a template parameter. As a result, we
        // only need to check initialization expression for EnumConstant
        const EnumConstantDecl* ecd = dynamic_cast<const EnumConstantDecl*>(vd);
        if (vd->getType()->isDependentType() && !ecd) return true;
      } else if (const TagDecl* nested_TD =
                     dynamic_cast<const TagDecl*>(child)) {
        if (ContainsDependentMember(nested_TD)) return true;
      }
    }
    return false;
  }
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A14_5_2
}  // namespace autosar
