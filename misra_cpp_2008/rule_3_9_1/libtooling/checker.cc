/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_3_9_1/libtooling/checker.h"

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

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "在所有声明和重新声明中，用于对象、函数返回类型或函数参数的类型的每个词符必须对应相同";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_9_1 {
namespace libtooling {

class FDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    std::string path_ =
        misra::libtooling_utils::GetFilename(fd, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(fd, result.SourceManager);
    const SourceLocation loc = fd->getLocation();
    if (loc.isInvalid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    // check for different return type
    // It is reported an error if functions differ only in their return type,
    // and the decl that has a semantic error is regarded as an invalid decl.
    if (fd->isInvalidDecl()) {
      ReportError(path_, line_number_, results_list_);
      return;
    }
    const FunctionDecl* other_fd = fd->getPreviousDecl();
    if (other_fd) {
      CheckAndReport(fd, other_fd, result);
    }
  }

 private:
  ResultsList* results_list_;
  bool CheckAndReport(const FunctionDecl* fd, const FunctionDecl* other_fd,
                      const MatchFinder::MatchResult& result) {
    std::string path_ =
        misra::libtooling_utils::GetFilename(fd, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(fd, result.SourceManager);
    if (fd->getNumParams() != other_fd->getNumParams()) {
      ReportError(path_, line_number_, results_list_);
    }
    for (unsigned int i = 0; i < fd->getNumParams(); i++) {
      const ParmVarDecl* pd = fd->getParamDecl(i);
      const ParmVarDecl* other_pd = other_fd->getParamDecl(i);
      // check for different type token
      string type_token = misra::libtooling_utils::GetTokenFromSourceLoc(
          result.SourceManager, pd->getBeginLoc(), pd->getLocation());
      string other_type_token = misra::libtooling_utils::GetTokenFromSourceLoc(
          result.SourceManager, other_pd->getBeginLoc(),
          other_pd->getLocation());
      if (type_token != other_type_token) {
        ReportError(path_, line_number_, results_list_);
      }
    }
    return true;
  }
};

class VDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(MatchFinder* finder, ResultsList* results_list) {
    results_list_ = results_list;
    finder->addMatcher(varDecl().bind("vd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    std::string path_ =
        misra::libtooling_utils::GetFilename(vd, result.SourceManager);
    int line_number_ =
        misra::libtooling_utils::GetLine(vd, result.SourceManager);
    const SourceLocation loc = vd->getLocation();
    if (loc.isInvalid() ||
        result.Context->getSourceManager().isInSystemHeader(loc)) {
      return;
    }
    const VarDecl* other_vd = vd->getPreviousDecl();
    if (other_vd) {
      string type_token = getCleanTypeToken(result.SourceManager, vd);
      string other_type_token =
          getCleanTypeToken(result.SourceManager, other_vd);
      if (type_token != other_type_token) {
        ReportError(path_, line_number_, results_list_);
      }
    }
  }

 private:
  ResultsList* results_list_;
  string getCleanTypeToken(SourceManager* source_manager, const VarDecl* vd) {
    string type_token = misra::libtooling_utils::GetTokenFromSourceLoc(
        source_manager, vd->getBeginLoc(), vd->getLocation());
    // getBeginLoc返回的是变量类型名出现的第一个位置，getLocation返回的是变量名出现的第一个位置
    // 这样获得的字符串除了类型名还有变量名的第一个字母，所以需要删除最后一位
    type_token.erase(type_token.end() - 1);
    // For C++, the keyword indicating external storage is 'extern'.
    // '__private_extern__' works only in C.
    if (vd->hasExternalStorage()) {
      int pos = getKeywordLocInTypeToken("extern", type_token);
      if (pos != string::npos) {
        type_token.erase(pos, 7);  // length of "extern "
      }
    }
    // Variables with keyword 'static' have global storage.
    if (vd->hasGlobalStorage()) {
      int pos = getKeywordLocInTypeToken("static", type_token);
      if (pos != string::npos) {
        type_token.erase(pos, 7);  // length of 'static '
      }
    }
    // Erase scopes and scope resolution operators (the qualified name prefix).
    int op_pos = type_token.find_last_of("::");
    if (op_pos != string::npos) {
      int space_pos = type_token.find_last_of(" ");
      if (space_pos != string::npos && space_pos < op_pos) {
        type_token.erase(space_pos + 1);  // erase "XX::XXX::"
      }
    }
    return type_token;
  }

  int getKeywordLocInTypeToken(string keyword, string type_token) {
    int pos = type_token.find(keyword + " ");
    if (pos != string::npos) {
      // If the type token starts with 'keyword ', which must be a keyword;
      if (pos == 0) {
        return pos;
      } else {
        // otherwise, a keyword will show as ' keyword '.
        int keyword_pos = type_token.find(" " + keyword + " ");
        if (keyword_pos != string::npos) {
          return keyword_pos + 1;
        }
      }
    }
    return -1;
  }
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new FDCallback;
  callback_->Init(&finder_, results_list);
  vd_callback_ = new VDCallback;
  vd_callback_->Init(&finder_, results_list);
}

}  // namespace libtooling
}  // namespace rule_3_9_1
}  // namespace misra_cpp_2008
