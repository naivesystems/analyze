/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_14_5_1/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra_cpp_2008 {
namespace rule_14_5_1 {
namespace libtooling {

/*
For arguments whose type is a class template specialization, their associated
namespaces are the namespaces in which any template arguments are members.
See https://en.cppreference.com/w/cpp/language/adl for more details.

Our design:

[In AssociatedNSChecker]
1) For each ClassTemplateSpecializationDecl, find its associated namespaces by
using Sema::FindAssociatedClassesAndNamespaces. To use this function, we do some
preparation such as building ASTs, and constructing Exprs.
2) For each CXXMethodDecl in the ClassTemplateSpecializationDecl, look up in the
found associated namespace to find out whether there is any function's name is
the same as its name by using DeclContext::lookup.
3) Collect the function names with its associated namespace which is from
the declaration context of each template argument(usually a CXXRecord).

Note: Some functions with the same name in associated namespaces may not be
non-member generic functions.

[In GenericFDChecker]
4) For each non-member generic function, find out whether it is in the
associated_NS_func_map. If its located namespace and function name are
collected in associated_NS_func_map, report an error.

In conclusion, we first collect candidate functions that may violate this rule,
and then find out whether there is a non-member generic function in the
candidate functions and report an error in its line.
*/

std::unordered_map<string, vector<string>> associated_NS_func_map;
std::unordered_map<string, string> func_loc_map;

class AssociatedNSCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder) {
    finder->addMatcher(
        cxxMethodDecl(
            hasParent(classTemplateSpecializationDecl().bind("cls_tmplt")))
            .bind("method"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const ClassTemplateSpecializationDecl* tmplt_ =
        result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("cls_tmplt");
    const CXXMethodDecl* method =
        result.Nodes.getNodeAs<CXXMethodDecl>("method");
    if (misra::libtooling_utils::IsInSystemHeader(tmplt_, result.Context)) {
      return;
    }
    if (result.Nodes.getNodeAs<CXXConstructorDecl>("method") != nullptr ||
        result.Nodes.getNodeAs<CXXDestructorDecl>("method") != nullptr) {
      return;
    }
    for (auto tmplt_arg : tmplt_->getTemplateArgs().asArray()) {
      // const TemplateArgument&  = tmplt_list[i];
      // The namespaces and classes associated with the types of the
      // template arguments provided for template type parameters (excluding
      // template template parameters)
      // Refer to https://clang.llvm.org/doxygen/SemaLookup_8cpp_source.html.
      // For more knowledges of type template parameters, please check
      // https://en.cppreference.com/w/cpp/language/template_parameters.
      if (tmplt_arg.getKind() != TemplateArgument::ArgKind::Type) {
        continue;
      }
      QualType tmplt_arg_type = tmplt_arg.getAsType();
      if (tmplt_arg_type->isRecordType()) {
        CXXRecordDecl* record_decl = cast<CXXRecordDecl>(
            cast<RecordType>(tmplt_arg_type.getTypePtr())->getDecl());
        auto decl_context = record_decl->getDeclContext();
        if (decl_context->isNamespace()) {
          auto namespace_decl =
              NamespaceDecl::castFromDeclContext(decl_context);
          if (!namespace_decl->lookup(method->getDeclName()).empty()) {
            string func_name = method->getNameAsString();
            string func_loc = misra::libtooling_utils::GetLocation(
                method, result.SourceManager);
            associated_NS_func_map[namespace_decl->getNameAsString()].push_back(
                func_name);
            func_loc_map[func_name] = func_loc;
          }
        }
      }
    }
  }
};

class GenericFDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    // match non-member generic functions
    finder->addMatcher(functionDecl(hasParent(functionTemplateDecl(
                                        hasParent(namespaceDecl().bind("ns")))),
                                    unless(isTemplateInstantiation()))
                           .bind("fd"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd_ = result.Nodes.getNodeAs<FunctionDecl>("fd");
    std::string path_ =
        misra::libtooling_utils::GetFilename(fd_, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(fd_, result.SourceManager);
    if (misra::libtooling_utils::IsInSystemHeader(fd_, result.Context)) {
      return;
    }
    const NamespaceDecl* ns = result.Nodes.getNodeAs<NamespaceDecl>("ns");
    string ns_name = ns->getNameAsString();
    // If the non-member generic function is in collected function names, report
    // an error in its line.
    string func_name = fd_->getNameAsString();
    if (associated_NS_func_map[ns_name].empty() ||
        func_loc_map[func_name].empty()) {
      return;
    }
    if (find(associated_NS_func_map[ns_name].begin(),
             associated_NS_func_map[ns_name].end(),
             func_name) != associated_NS_func_map[ns_name].end()) {
      string func_loc =
          misra::libtooling_utils::GetLocation(fd_, result.SourceManager);
      ReportError(func_name, path_, line_number_, func_loc,
                  func_loc_map[func_name]);
    }
  }

 private:
  ResultsList* results_list_;
  void ReportError(string name, string filename, int line, string loc1,
                   string loc2) {
    string error_message =
        "非成员泛型函数只能在不是关联命名空间的命名空间中声明";
    std::vector<std::string> locations{loc1, loc2};
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddMultipleLocationsResultToResultsList(
            results_list_, filename, line, error_message, locations);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_5_1);
    pb_result->set_name(name);
    pb_result->set_loc(loc1);
    pb_result->set_other_loc(loc2);
  }
};

// The AssociatedNSChecker is to collect associated namespaces and candidate
// function names for a class template specialization.
void AssociatedNSChecker::Init() {
  associated_ns_callback_ = new AssociatedNSCallback;
  associated_ns_callback_->Init(&finder_);
}

// The GenericFDCallback is to check whether the non-member generic function is
// in the candidate functions (whose name is the same as one funtion in the
// corresponding class template specialization).
void GenericFDChecker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  generic_fd_callback_ = new GenericFDCallback;
  generic_fd_callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_14_5_1
}  // namespace misra_cpp_2008
