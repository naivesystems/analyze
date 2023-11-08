/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_3_2_3/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {
void ReportError(ResultsList* results_list, string path, int line_number,
                 string loc, string other_loc, string name) {
  string error_message =
      "在多个翻译单元中使用的类型/对象/函数只应在一个文件中声明";
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_3_2_3);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_2_3 {
namespace libtooling {

struct NameInfo {
  string mainfile;
  string fileline;  // like sub/test.cc:8
};

void CheckUniqueOnNamedDecl(
    const NamedDecl* nd, SourceManager* sm, ASTContext* context,
    unordered_map<string, struct NameInfo>& name_filelines,
    ResultsList* results_list) {
  string name = nd->getQualifiedNameAsString();
  SourceLocation spelling_loc = sm->getSpellingLoc(nd->getLocation());
  std::string file =
      misra::libtooling_utils::GetLocationFilename(spelling_loc, sm);
  int line = misra::libtooling_utils::GetLocationLine(spelling_loc, sm);
  string fileline = absl::StrFormat("%s:%d", file, line);
  string mainfile = sm->getNonBuiltinFilenameForID(sm->getMainFileID())->str();

  auto find = name_filelines.find(name);
  if (find == name_filelines.end()) {
    name_filelines.insert(
        make_pair(name, (struct NameInfo){mainfile, fileline}));
    return;
  }
  if (find->second.fileline == fileline) {
    find->second.mainfile = mainfile;
    return;
  }
  if (find->second.mainfile != mainfile && find->second.fileline != fileline) {
    ReportError(results_list, file, line, fileline, find->second.fileline,
                name);
  }
}

class NamedCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(varDecl().bind("nd"), this);
    finder->addMatcher(functionDecl().bind("nd"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const NamedDecl* nd = result.Nodes.getNodeAs<NamedDecl>("nd");
    if (misra::libtooling_utils::IsInSystemHeader(nd, result.Context)) {
      return;
    }
    if (nd->getLinkageInternal() == clang::NoLinkage ||
        nd->getLinkageInternal() == clang::InternalLinkage ||
        nd->getLinkageInternal() == clang::UniqueExternalLinkage) {
      // cannot be referred to from other translation units
      return;
    }

    CheckUniqueOnNamedDecl(nd, result.SourceManager, result.Context,
                           name_filelines_, results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct NameInfo> name_filelines_;
};

class RecordCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(recordDecl().bind("rd"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>("rd");
    if (misra::libtooling_utils::IsInSystemHeader(rd, result.Context)) {
      return;
    }
    if (rd->getLinkageInternal() == clang::NoLinkage ||
        rd->getLinkageInternal() == clang::InternalLinkage ||
        rd->getLinkageInternal() == clang::UniqueExternalLinkage) {
      // cannot be referred to from other translation units
      return;
    }
    if (rd->isInjectedClassName()) {
      return;
    }

    CheckUniqueOnNamedDecl(rd, result.SourceManager, result.Context,
                           name_filelines_, results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct NameInfo> name_filelines_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  nd_callback_ = new NamedCallback;
  rd_callback_ = new RecordCallback;
  nd_callback_->Init(results_list_, &finder_);
  rd_callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_3_2_3
}  // namespace misra_cpp_2008
