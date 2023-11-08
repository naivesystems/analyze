/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_14_8_1/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {
struct Loc {
  int line;
  string file;
  enum LocType {
    TemplateSpecializationType,
    TemplateType,
  } type;
  string to_string() const {
    return file + " : " + std::to_string(line) + " : " + std::to_string(type);
  }
  bool operator<(const Loc& oth) const { return to_string() < oth.to_string(); }
};

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "重载的函数模板不应显式特化";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_14_8_1);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_14_8_1 {
namespace libtooling {

AST_MATCHER(FunctionTemplateDecl, isFirstDecl) { return Node.isFirstDecl(); }

class OverloadedTemplateCallBack : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(isExplicitTemplateSpecialization()).bind("func_decl"),
        this);
    finder->addMatcher(
        functionTemplateDecl(isFirstDecl()).bind("func_template_decl"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* sm = result.SourceManager;
    const FunctionTemplateDecl* func_template_decl =
        result.Nodes.getNodeAs<FunctionTemplateDecl>("func_template_decl");
    const FunctionDecl* func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl");
    string template_name;
    if (func_template_decl) {
      if (misra::libtooling_utils::IsInSystemHeader(func_template_decl,
                                                    result.Context)) {
        return;
      }
      template_name = func_template_decl->getQualifiedNameAsString();
      struct Loc loc;
      loc.file = misra::libtooling_utils::GetFilename(func_template_decl, sm);
      loc.line = misra::libtooling_utils::GetLine(func_template_decl, sm);
      loc.type = Loc::TemplateType;

      template_name_loc_[template_name].insert(loc);
    }
    if (func_decl) {
      if (misra::libtooling_utils::IsInSystemHeader(func_decl,
                                                    result.Context)) {
        return;
      }
      auto primary_template = func_decl->getPrimaryTemplate();
      if (primary_template == nullptr) {
        return;
      }
      template_name = primary_template->getQualifiedNameAsString();
      template_name_loc_[template_name].insert(
          {misra::libtooling_utils::GetLine(func_decl, sm),
           misra::libtooling_utils::GetFilename(func_decl, sm),
           Loc::TemplateSpecializationType});
    }
    auto& template_set = template_name_loc_[template_name];
    int overload_count = 0;
    for (auto& loc : template_set) {
      if (loc.type == Loc::TemplateType) {
        overload_count++;
      }
    }
    if (overload_count > 1) {
      vector<Loc> should_erase;
      for (auto& loc : template_set) {
        if (loc.type == Loc::TemplateSpecializationType) {
          ReportError(loc.file, loc.line, results_list_);
          should_erase.push_back(loc);
        }
      }
      for (auto& loc : should_erase) {
        template_set.erase(loc);
      }
    }
  }

 private:
  ResultsList* results_list_;
  // coz two functionTemplateDecl have no relationship,
  // we store the overloaded functionTemplateDecl in a map,
  // key is the deck name, value is the list of locs

  // when we match a explict specialization, we need to check if that
  // specialization's name is in this map and the size of loc list greater than
  // 1

  // the size of loc list greater than 1 means that this function template has
  // overloaded
  unordered_map<string, set<struct Loc>> template_name_loc_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new OverloadedTemplateCallBack;
  callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_14_8_1
}  // namespace misra_cpp_2008
