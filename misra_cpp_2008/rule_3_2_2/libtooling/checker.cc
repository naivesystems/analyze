/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_3_2_2/libtooling/checker.h"

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
  string error_message = "不应违背单一定义规则";
  std::vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_3_2_2);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_2_2 {
namespace libtooling {

struct NameInfo {
  string source;
  string fileline;  // like sub/test.cc:8
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

void CheckUniqueOnNameDecl(const NamedDecl* d, SourceManager* sm,
                           ASTContext* context,
                           unordered_map<string, struct NameInfo>& name_infos,
                           ResultsList* results_list) {
  if (misra::libtooling_utils::IsInSystemHeader(d, context)) {
    return;
  }
  string name = d->getQualifiedNameAsString();

  SourceLocation spelling_loc = sm->getSpellingLoc(d->getLocation());
  string file = misra::libtooling_utils::GetLocationFilename(spelling_loc, sm);
  int line = misra::libtooling_utils::GetLocationLine(spelling_loc, sm);
  string fileline = absl::StrFormat("%s:%d", file, line);

  auto find = name_infos.find(name);
  if (find == name_infos.end()) {
    name_infos.insert(make_pair(name, (struct NameInfo){{}, fileline}));
    return;
  }
  if (find->second.fileline != fileline) {
    ReportError(results_list, file, line, fileline, find->second.fileline,
                name);
  }
}

void CheckIdentOnNameDecl(const NamedDecl* d, SourceManager* sm,
                          ASTContext* context,
                          unordered_map<string, struct NameInfo>& name_infos,
                          ResultsList* results_list) {
  if (misra::libtooling_utils::IsInSystemHeader(d, context)) {
    return;
  }
  string name = d->getQualifiedNameAsString();
  auto char_range = Lexer::makeFileCharRange(
      CharSourceRange::getTokenRange(d->getSourceRange()), *sm,
      context->getLangOpts());
  string source =
      Lexer::getSourceText(char_range, *sm, context->getLangOpts()).str();

  SourceLocation spelling_loc = sm->getSpellingLoc(d->getLocation());
  string file = misra::libtooling_utils::GetLocationFilename(spelling_loc, sm);
  int line = misra::libtooling_utils::GetLocationLine(spelling_loc, sm);
  string fileline = absl::StrFormat("%s:%d", file, line);

  auto find = name_infos.find(name);
  if (find == name_infos.end()) {
    name_infos.insert(make_pair(name, (struct NameInfo){source, fileline}));
    return;
  }
  if (find->second.source != source && find->second.fileline != fileline) {
    ReportError(results_list, file, line, fileline, find->second.fileline,
                name);
  }
}

void CheckUniqueOnFunctionDecl(const FunctionDecl* d, SourceManager* sm,
                               ASTContext* context, map_t& name_parms_infos,
                               ResultsList* results_list) {
  if (misra::libtooling_utils::IsInSystemHeader(d, context)) {
    return;
  }
  string name = d->getQualifiedNameAsString();
  unsigned nparms = d->getNumParams();
  std::vector<string> parms;
  for (auto it = d->param_begin(); it != d->param_end(); it++) {
    parms.push_back((**it).getOriginalType().getAsString());
  }

  SourceLocation spelling_loc = sm->getSpellingLoc(d->getLocation());
  string file = misra::libtooling_utils::GetLocationFilename(spelling_loc, sm);
  int line = misra::libtooling_utils::GetLocationLine(spelling_loc, sm);
  string fileline = absl::StrFormat("%s:%d", file, line);

  auto find = name_parms_infos.find(std::make_tuple(name, nparms, parms));
  if (find == name_parms_infos.end()) {
    name_parms_infos.insert(make_pair(std::make_tuple(name, nparms, parms),
                                      (struct NameInfo){{}, fileline}));
    return;
  }
  if (find->second.fileline != fileline) {
    ReportError(results_list, file, line, fileline, find->second.fileline,
                name);
  }
}

void CheckIdentOnFunctionDecl(const FunctionDecl* d, SourceManager* sm,
                              ASTContext* context, map_t& name_parms_infos,
                              ResultsList* results_list) {
  if (misra::libtooling_utils::IsInSystemHeader(d, context)) {
    return;
  }
  string name = d->getQualifiedNameAsString();
  unsigned nparms = d->getNumParams();
  std::vector<string> parms;
  for (auto it = d->param_begin(); it != d->param_end(); it++) {
    parms.push_back((**it).getOriginalType().getAsString());
  }

  auto char_range = Lexer::makeFileCharRange(
      CharSourceRange::getTokenRange(d->getSourceRange()), *sm,
      context->getLangOpts());
  string source =
      Lexer::getSourceText(char_range, *sm, context->getLangOpts()).str();

  SourceLocation spelling_loc = sm->getSpellingLoc(d->getLocation());
  string file = misra::libtooling_utils::GetLocationFilename(spelling_loc, sm);
  int line = misra::libtooling_utils::GetLocationLine(spelling_loc, sm);
  string fileline = absl::StrFormat("%s:%d", file, line);

  auto find = name_parms_infos.find(std::make_tuple(name, nparms, parms));
  if (find == name_parms_infos.end()) {
    name_parms_infos.insert(make_pair(std::make_tuple(name, nparms, parms),
                                      (struct NameInfo){source, fileline}));
    return;
  }
  if (find->second.source != source && find->second.fileline != fileline) {
    ReportError(results_list, file, line, fileline, find->second.fileline,
                name);
  }
}

class RecordCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(recordDecl(isDefinition()).bind("d"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const NamedDecl* d = result.Nodes.getNodeAs<NamedDecl>("d");
    if (d->getQualifiedNameAsString() == "(anonymous)") {
      return;
    }
    CheckIdentOnNameDecl(d, result.SourceManager, result.Context, name_infos_,
                         results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct NameInfo> name_infos_;
};

class ClassTemplateCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(classTemplateDecl().bind("d"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const ClassTemplateDecl* d = result.Nodes.getNodeAs<ClassTemplateDecl>("d");
    if (d->isThisDeclarationADefinition()) {
      CheckIdentOnNameDecl(d, result.SourceManager, result.Context, name_infos_,
                           results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct NameInfo> name_infos_;
};

class FunctionCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(isDefinition(),
                     unless(hasParent(cxxRecordDecl(isLambda()))))
            .bind("d"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* d = result.Nodes.getNodeAs<FunctionDecl>("d");
    if (d->isThisDeclarationADefinition()) {
      if (d->isInlineSpecified()) {
        CheckIdentOnFunctionDecl(d, result.SourceManager, result.Context,
                                 name_parms_infos_, results_list_);
      } else {
        CheckUniqueOnFunctionDecl(d, result.SourceManager, result.Context,
                                  name_parms_infos_, results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  map_t name_parms_infos_;
};

class VarCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        varDecl(isDefinition(), hasExternalFormalLinkage()).bind("d"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const NamedDecl* d = result.Nodes.getNodeAs<NamedDecl>("d");
    CheckUniqueOnNameDecl(d, result.SourceManager, result.Context, name_infos_,
                          results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct NameInfo> name_infos_;
};

class TypedefCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(typedefDecl().bind("d"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const NamedDecl* d = result.Nodes.getNodeAs<NamedDecl>("d");
    CheckIdentOnNameDecl(d, result.SourceManager, result.Context, name_infos_,
                         results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  unordered_map<string, struct NameInfo> name_infos_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  rd_callback_ = new RecordCallback;
  ct_callback_ = new ClassTemplateCallback;
  fun_callback_ = new FunctionCallback;
  var_callback_ = new VarCallback;
  td_callback_ = new TypedefCallback;
  rd_callback_->Init(results_list_, &finder_);
  ct_callback_->Init(results_list_, &finder_);
  fun_callback_->Init(results_list_, &finder_);
  var_callback_->Init(results_list_, &finder_);
  td_callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_3_2_2
}  // namespace misra_cpp_2008
