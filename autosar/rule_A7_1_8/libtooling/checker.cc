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

#include "autosar/rule_A7_1_8/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>
#include <llvm/ADT/StringRef.h>

#include <regex>

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
      "A non-type specifier shall be placed before a type specifier in a declaration.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A7_1_8 {
namespace libtooling {

StringRef getDeclSource(SourceLocation startLoc, SourceLocation endLoc,
                        const ast_matchers::MatchFinder::MatchResult& result) {
  clang::SourceRange range =
      SourceRange(result.SourceManager->getSpellingLoc(startLoc),
                  result.SourceManager->getSpellingLoc(endLoc));
  auto char_range = Lexer::makeFileCharRange(
      CharSourceRange::getTokenRange(range), *result.SourceManager,
      result.Context->getLangOpts());
  auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                     result.Context->getLangOpts());
  return source;
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("decl"), this);

    finder->addMatcher(
        varDecl(unless(isExpansionInSystemHeader())).bind("decl"), this);

    finder->addMatcher(
        fieldDecl(unless(isExpansionInSystemHeader())).bind("decl"), this);

    finder->addMatcher(
        typedefDecl(unless(isExpansionInSystemHeader())).bind("typedef_decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    set<string> non_type_specifier_set{
        "friend",       "constexpr", "register", "static",  "extern",
        "thread_local", "mutable",   "inline",   "virtual", "explicit"};

    const DeclaratorDecl* decl = result.Nodes.getNodeAs<DeclaratorDecl>("decl");
    const TypedefDecl* typedef_decl =
        result.Nodes.getNodeAs<TypedefDecl>("typedef_decl");

    // c++ regex: [:w:]匹配大写字母、小写字母、数字和下划线
    regex re_token("[[:w:]]+");
    if (decl) {
      StringRef source;
      if (isa<FunctionDecl>(decl)) {
        source = getDeclSource(decl->getTypeSpecStartLoc(),
                               decl->getTypeSpecEndLoc(), result);
      } else {
        // find non-type specifier between TypeSpecStartLoc and decl_EndLoc
        source = getDeclSource(decl->getTypeSpecStartLoc(), decl->getEndLoc(),
                               result);
      }
      // delete comment
      string str = source.str();
      regex re_comment("//[^\n\r]*");
      regex re_multi_line_comment("/[*]([^*]|([*]+[^*/]))*[*]+/");
      str = regex_replace(str, re_comment, " ");
      str = regex_replace(str, re_multi_line_comment, " ");
      // get all possible specifiers
      smatch match;
      string::const_iterator begin = str.begin();
      string::const_iterator end = str.end();
      while (regex_search(begin, end, match, re_token)) {
        if (non_type_specifier_set.find(match[0]) !=
            non_type_specifier_set.end()) {
          std::string path =
              misra::libtooling_utils::GetFilename(decl, result.SourceManager);
          int line_number =
              misra::libtooling_utils::GetLine(decl, result.SourceManager);
          ReportError(path, line_number, results_list_);
          return;
        }
        begin = match[0].second;
      }
    }
    if (typedef_decl) {
      FullSourceLoc location =
          result.Context->getFullLoc(typedef_decl->getBeginLoc());
      if (location.isInvalid()) {
        return;
      }
      auto source = getDeclSource(typedef_decl->getBeginLoc(),
                                  typedef_decl->getEndLoc(), result);
      string str = source.str();
      smatch match;
      regex_search(str, match, re_token);
      // the first token in typedef decl should be 'typedef'
      if (match[0] != "typedef") {
        std::string path = misra::libtooling_utils::GetFilename(
            typedef_decl, result.SourceManager);
        int line_number = misra::libtooling_utils::GetLine(
            typedef_decl, result.SourceManager);
        ReportError(path, line_number, results_list_);
      }
    }
    /*
      实现思路：
        找TypeSpec，对于
          static int a = 10;
        TypeSpec是'int a'
        只要non-type-specifier出现在TypeSpec的中间或者后面，就报错。
        对于TypedefDecl，如果第一个token不是typedef就报错。

      其他尝试（直接判断token位置关系，e.g. cppcheck tokenlist）：
        目前没找到解决方法的问题：给定一个token，如何判断它是一个type-specifier？
        尝试过提取decl的UnqualifiedType，然后QualType::getAsString()转成字符串，再和token比较，
        但是有一些情况处理不了，比如auto和数组，很难准确地拿到那一个type-specifier-token.
          (1) auto：
            希望拿到'auto'，但往往拿到的是推导后的类型，比如'double'
          (2) 数组：
            typedef int myint;
            myint arr[2][3];
            希望拿到'myint'，但是要找ArrayElementType的话类型就是Type，不能转QualType，所以没法转字符串，只能Type::getCanonicalTypeInternal()拿到'int'

        此外，cppcheck tokenlist中不包含inline.
    */
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
}  // namespace rule_A7_1_8
}  // namespace autosar
