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

#include "misra_cpp_2008/rule_14_6_1/libtooling/checker.h"

#include <glog/logging.h>

#include <climits>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string loc, int line_number, ResultsList* results_list) {
  std::string error_message =
      "在具有依赖基的类模板中，可以在该依赖基中找到的任何名称都应使用限定 ID 或 this->";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, loc, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_6_1);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_14_6_1 {
namespace libtooling {

// Key is the name of a class
// Value is a list of field and cxxmethod name
unordered_map<string, unordered_set<string>> dependent_base_names_;

// Collect all the name info of a dependent base class

// This checker will be run first to get the info we need
// and store the info in dependent_base_names_ map

// we will do the checking based on the info from dependent_base_names_
// Match var decl for checking if variable type in the dependent type
// Match declRefEcpr for checking fuction and variable name

// Then loop through dependent_base_names_ to
// identify if a type/name of a class in
// it's dependent base
// if a type/name in the dependent base, we then
// need to check if it has been qulified
// if not, report
class InfoFillCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ast_matchers::MatchFinder* finder) {
    finder->addMatcher(
        classTemplateDecl(hasDescendant(
            cxxRecordDecl(forEachDescendant(namedDecl().bind("name")))
                .bind("record"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* cxx_record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    auto* namedDecl = result.Nodes.getNodeAs<NamedDecl>("name");
    string name = cxx_record->getNameAsString();
    dependent_base_names_[name].insert(namedDecl->getNameAsString());
  }
};

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        classTemplateDecl(
            forEachDescendant(cxxMethodDecl(eachOf(
                forEachDescendant(varDecl().bind("var_decl")),
                // will filter out this->g(), this->i case
                // because above situation does not contain declRefExpr node
                forEachDescendant(declRefExpr().bind("decl_ref_expr"))))),
            hasDescendant(cxxRecordDecl().bind("record")))
            .bind("class_template"),
        this);
    finder->addMatcher(
        classTemplateDecl(forEachDescendant(cxxMethodDecl().bind("cxx_method")),
                          hasDescendant(cxxRecordDecl().bind("record")))
            .bind("class_template"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* class_template =
        result.Nodes.getNodeAs<ClassTemplateDecl>("class_template");
    auto* cxx_record = result.Nodes.getNodeAs<CXXRecordDecl>("record");

    if (misra::libtooling_utils::IsInSystemHeader(class_template,
                                                  result.Context)) {
      return;
    }
    if (!cxx_record->hasDefinition()) {
      return;
    }

    if (!cxx_record->hasAnyDependentBases()) {
      return;
    }

    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("var_decl");
    const DeclRefExpr* decl_ref_expr =
        result.Nodes.getNodeAs<DeclRefExpr>("decl_ref_expr");
    const CXXMethodDecl* cxx_method =
        result.Nodes.getNodeAs<CXXMethodDecl>("cxx_method");

    // if a var type is not be qualified
    // bad1
    if (var_decl) {
      if (llvm::isa<ParmVarDecl>(var_decl)) {
        return;
      }
      bool report_unqualified_type = IsTypeFoundInDependentBaseAndUnqualified(
          var_decl->getType(), cxx_record, result.SourceManager);
      if (report_unqualified_type) {
        ReportError(
            misra::libtooling_utils::GetFilename(var_decl,
                                                 result.SourceManager),
            misra::libtooling_utils::GetLine(var_decl, result.SourceManager),
            results_list_);
      }
    }
    // if a function call or variable used is not be qualified
    // bad2
    else if (decl_ref_expr) {
      string func_or_var_str = decl_ref_expr->getNameInfo().getAsString();
      string func_or_var_token = misra::libtooling_utils::GetTokenFromSourceLoc(
          result.SourceManager, decl_ref_expr->getBeginLoc(),
          decl_ref_expr->getEndLoc());

      // skip qualified identifier
      if (func_or_var_token.find("::") != string::npos) {
        return;
      }
      if (IfDeclNameFoundInDependentBase(func_or_var_str, cxx_record,
                                         result.SourceManager)) {
        ReportError(misra::libtooling_utils::GetFilename(decl_ref_expr,
                                                         result.SourceManager),
                    misra::libtooling_utils::GetLine(decl_ref_expr,
                                                     result.SourceManager),
                    results_list_);
      }
    }
    // if a function return type is not be qualified
    // bad3
    else if (cxx_method) {
      bool report_unqualified_type = IsTypeFoundInDependentBaseAndUnqualified(
          cxx_method->getReturnType(), cxx_record, result.SourceManager);
      if (report_unqualified_type) {
        ReportError(
            misra::libtooling_utils::GetFilename(cxx_method,
                                                 result.SourceManager),
            misra::libtooling_utils::GetLine(cxx_method, result.SourceManager),
            results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  string GetDependentBaseClassName(string base) {
    auto pos = base.find("<");
    if (pos == string::npos) {
      return "";
    }
    // if the base class is B<T>
    // get the string "B" only
    string name = base.substr(0, pos);
    return name;
  }

  bool IfDeclNameFoundInDependentBase(const string decl_str,
                                      const CXXRecordDecl* cxx_record,
                                      SourceManager* sm) {
    for (auto base_class = cxx_record->bases_begin();
         base_class != cxx_record->bases_end(); ++base_class) {
      // loop through the bases of a class
      // to get the name info of that base class
      auto base_name = GetDependentBaseClassName(
          base_class->getTypeSourceInfo()->getType().getAsString());
      auto find_in_base = dependent_base_names_.find(base_name);
      if (find_in_base == dependent_base_names_.end()) {
        continue;
      }
      auto name_in_base = find_in_base->second.find(decl_str);
      if (name_in_base != find_in_base->second.end()) {
        return true;
      }
    }
    return false;
  }

  bool IsTypeFoundInDependentBaseAndUnqualified(const QualType type,
                                                const CXXRecordDecl* cxx_record,
                                                SourceManager* sm) {
    // for skipping builtin type
    if (type.getCanonicalType() == type) {
      return false;
    }

    if (!IfDeclNameFoundInDependentBase(type.getAsString(), cxx_record, sm)) {
      return false;
    }

    bool is_qualified_type = false;
    // handle the case like typename B<T>::TYPE
    if (auto dt = llvm::dyn_cast<DependentNameType>(type)) {
      is_qualified_type = true;
    }
    // handle the case like ::Type
    else if (auto et = llvm::dyn_cast<ElaboratedType>(type)) {
      if (et->getQualifier()->getKind() == clang::NestedNameSpecifier::Global) {
        is_qualified_type = true;
      }
    }
    return !is_qualified_type;
  }
};

void InfoFillChecker::Init() {
  callback_ = new InfoFillCallback;
  callback_->Init(&finder_);
}
void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_14_6_1
}  // namespace misra_cpp_2008
