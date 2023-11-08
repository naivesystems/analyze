/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_8_4_2/libtooling/checker.h"

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
      "函数的重新声明中用作形参的标识符必须与原声明中的标识符相同";
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_8_4_2);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_8_4_2 {
namespace libtooling {

struct NameInfo {
  string fileline;  // like sub/test.cc:8
  std::vector<string> names;
};

typedef std::tuple<string, unsigned, std::vector<string>> key_t;

struct key_hash {
  std::size_t operator()(const key_t& k) const {
    return std::hash<string>{}(std::get<0>(k));
  }
};

struct key_equal {
  bool operator()(const key_t& v0, const key_t& v1) const {
    return (std::get<0>(v0) == std::get<0>(v1)) &&
           (std::get<1>(v0) == std::get<1>(v1)) &&
           (std::get<2>(v0) == std::get<2>(v1));
  }
};

typedef unordered_map<key_t, struct NameInfo, key_hash, key_equal> map_t;

bool ok(string v1, string v2) {
  string empty = "";
  return (v1 == v2) || (v1 == empty) || (v2 == empty);
}

class FunctionCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl(unless(cxxMethodDecl())).bind("d"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* d = result.Nodes.getNodeAs<FunctionDecl>("d");
    if (misra::libtooling_utils::IsInSystemHeader(d, result.Context)) {
      return;
    }
    string name = d->getQualifiedNameAsString();
    unsigned nparms = d->getNumParams();
    std::vector<string> types, names;
    for (auto it = d->param_begin(); it != d->param_end(); it++) {
      types.push_back((**it).getOriginalType().getAsString());
      names.push_back((**it).getQualifiedNameAsString());
    }

    SourceLocation spelling_loc =
        result.SourceManager->getSpellingLoc(d->getLocation());
    string file = misra::libtooling_utils::GetLocationFilename(
        spelling_loc, result.SourceManager);
    int line = misra::libtooling_utils::GetLocationLine(spelling_loc,
                                                        result.SourceManager);
    string fileline = absl::StrFormat("%s:%d", file, line);

    auto find = name_parms_infos_.find(std::make_tuple(name, nparms, types));
    if (find == name_parms_infos_.end()) {
      name_parms_infos_.insert(make_pair(std::make_tuple(name, nparms, types),
                                         (struct NameInfo){fileline, names}));
      return;
    }
    if (find->second.fileline != fileline) {
      for (auto i = 0; i < nparms; i++) {
        if (!ok(names[i], find->second.names[i])) {
          ReportError(results_list_, file, line, fileline,
                      find->second.fileline, name);
          return;
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  map_t name_parms_infos_;
};

class CXXMethodCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxMethodDecl().bind("md"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXMethodDecl* d = result.Nodes.getNodeAs<CXXMethodDecl>("md");
    if (misra::libtooling_utils::IsInSystemHeader(d, result.Context)) {
      return;
    }
    if (d->size_overridden_methods() == 0) {
      return;
    }
    string name = d->getQualifiedNameAsString();
    auto pred = (*d->begin_overridden_methods());
    for (auto i = 0; i < d->getNumParams(); i++) {
      if (!ok(d->getParamDecl(i)->getQualifiedNameAsString(),
              pred->getParamDecl(i)->getQualifiedNameAsString())) {
        SourceLocation spelling_loc =
            result.SourceManager->getSpellingLoc(d->getLocation());
        string file = misra::libtooling_utils::GetLocationFilename(
            spelling_loc, result.SourceManager);
        int line = misra::libtooling_utils::GetLocationLine(
            spelling_loc, result.SourceManager);
        string fileline = absl::StrFormat("%s:%d", file, line);
        SourceLocation pred_spelling_loc =
            result.SourceManager->getSpellingLoc(d->getLocation());
        string pred_fileline =
            absl::StrFormat("%s:%d",
                            misra::libtooling_utils::GetLocationFilename(
                                pred_spelling_loc, result.SourceManager),
                            misra::libtooling_utils::GetLocationLine(
                                pred_spelling_loc, result.SourceManager));

        ReportError(results_list_, file, line, fileline, pred_fileline, name);
        return;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  map_t name_parms_infos_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  fun_callback_ = new FunctionCallback;
  method_callback_ = new CXXMethodCallback;
  fun_callback_->Init(results_list_, &finder_);
  method_callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_8_4_2
}  // namespace misra_cpp_2008
