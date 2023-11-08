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

#include "autosar/rule_A8_5_2/libtooling/checker.h"

#include <glog/logging.h>

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
      "Braced-initialization {}, without equalssign, shall be used for variable initialization.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A8_5_2 {
namespace libtooling {
// builtin type:
// bad cases: int i1 = 10; int i2 = {10}; int i3(10);
// good cases: int i4{10};

// class type:
// 无initializer_list构造函数：
// bad cases: A a1 = {1,5}; A a2(1,5);
// good cases: A a3{1,5};

// 有initializer_list构造函数:
// bad cases: C c1 = {1,5};
// good cases: C c2(1,5); c3{1,5};

// 在上述例子中, i1和i3的AST相同
// |-VarDecl 0x55f17f046c28 <test.cpp:24:1, col:10> col:5 i1 'int' cinit
// | `-IntegerLiteral 0x55f17f046cd8 <col:10> 'int' 10
// 所以,builtin_decl_unless_brace 用来匹配i1和i3

// i2和i4的AST相同，只能用字符串来区分
// |-VarDecl 0x55f17f046d68 <line:25:1, col:13> col:5 i2 'int' cinit
// | `-InitListExpr 0x55f17f046e48 <col:10, col:13> 'int'
// |   `-IntegerLiteral 0x55f17f046dd0 <col:11> 'int' 10
//
// a1,a2,a3,c2的AST相同，只能用字符串来区分
// -VarDecl 0x563280c2f4f8 <col:5, col:14> col:7 a2 'A' callinit
//  |   `-CXXConstructExpr 0x563280c2f5c8 <col:7, col:14> 'A' 'void (int, int)'

// c1和c3的AST相同，只能用字符串来区分
// VarDecl 0x55bf789324d8 <col:5, col:16> col:7 c2 'C' cinits
//  `-ExprWithCleanups 0x55bf78932680 <col:12, col:16> 'C'
//    `-CXXConstructExpr 0x55bf78932650 <col:12, col:16> 'C' 'void
//    (std::initializer_list<int>)' list std::initializer_list
//      `-CXXStdInitializerListExpr 0x55bf78932638 <col:12, col:16>
//      'std::initializer_list<int>':'std::initializer_list<int>'

// 上述需要用字符串进行区分的情况，都含有CXXConstructExpr或
// InitListExpr,所以使用decl将上述情况全部匹配。

class Callback : public MatchFinder::MatchCallback {
 public:
  // 判断是否用()和=进行初始化
  bool IsNotInitWithBrace(const MatchFinder::MatchResult& result,
                          const SourceLocation& begin,
                          const SourceLocation& end) {
    auto source_str = misra::libtooling_utils::GetTokenFromSourceLoc(
        result.SourceManager, begin, end);
    // 匹配包含在/*和*/中的字符串
    regex comment_regex(R"(\/\*.*?\*\/)");
    // 删除原字符串中使用/**/注释的部分
    auto result_str = regex_replace(source_str, comment_regex, "");
    if (result_str.find("=") != string::npos) {
      return true;
    } else if (result_str.find('(') != string::npos &&
               result_str.find(')') != string::npos) {
      return true;
    }
    return false;
  }

  // 判断是否用=初始化
  bool IsInitWithEqualSign(const MatchFinder::MatchResult& result,
                           const SourceLocation& begin,
                           const SourceLocation& end) {
    auto source_str = misra::libtooling_utils::GetTokenFromSourceLoc(
        result.SourceManager, begin, end);
    // 匹配包含在/*和*/中的字符串
    regex comment_regex(R"(\/\*.*?\*\/)");
    // 删除原字符串中使用/**/注释的部分
    auto result_str = regex_replace(source_str, comment_regex, "");
    if (result_str.find("=") != string::npos) {
      return true;
    }
    return false;
  }

  // 寻找是否有std::initializer_list作为参数的构造函数
  bool ContainsInitializerListParameters(
      const VarDecl* decl, const MatchFinder::MatchResult& result) {
    QualType var_type = decl->getType();
    if (auto record_decl = var_type->getAsCXXRecordDecl()) {
      for (auto ctor : record_decl->ctors()) {
        for (auto param : ctor->parameters())
          if (param->getType().getAsString().find("std::initializer_list") !=
              string::npos) {
            return true;
          }
      }
    }
    return false;
  }

  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;

    // classType定义是会有initListExpr或cxxConstructExpr的，如果这两种都没有，那就是builtin.
    // 没有用hasType(builtinType)进行限制的原因是，clang的builtinType不包含std::int8_t这种类型
    finder->addMatcher(
        varDecl(
            hasInitializer(
                allOf(unless(initListExpr()), unless(cxxConstructExpr()))),
            unless(isInstantiated()), unless(hasType(pointerType())),
            unless(hasDescendant(
                cxxStdInitializerListExpr()) /*忽略使用std::initializer_list进行初始化的变量*/))
            .bind("builtin_decl_unless_brace"),
        this);

    // 这个匹配器之所以没用cxxStdInitializerListExpr
    // 去忽略使用std::initializer_list进行初始化的原因是下面这种情况
    // class A{
    //   A(int i, int j){};
    //   A(std::initializer_list){};
    // };
    // A(1,2)会去使用第一个构造函数，此时AST中没有cxxStdInitializerListExpr,但这个是合法的
    // 故需要去run寻找他是否含有以std::initializer_list作为参数的构造函数
    finder->addMatcher(
        varDecl(anyOf(hasDescendant(initListExpr()),
                      hasDescendant(cxxConstructExpr())),
                unless(hasType(pointerType())), unless(isInstantiated()))
            .bind("decl"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const VarDecl* builtin_decl_unless_brace =
        result.Nodes.getNodeAs<VarDecl>("builtin_decl_unless_brace");
    if (builtin_decl_unless_brace) {
      const SourceLocation loc = builtin_decl_unless_brace->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        ReportError(misra::libtooling_utils::GetFilename(
                        builtin_decl_unless_brace, result.SourceManager),
                    misra::libtooling_utils::GetLine(builtin_decl_unless_brace,
                                                     result.SourceManager),
                    results_list_);
      }
    }

    const VarDecl* decl = result.Nodes.getNodeAs<VarDecl>("decl");
    if (decl) {
      const SourceLocation loc = decl->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        // 首先判断是不是有std::initializer_list为参数的构造函数
        if (ContainsInitializerListParameters(decl, result)) {
          // 判断是否用=进行初始化
          if (!IsInitWithEqualSign(result, decl->getBeginLoc(),
                                   decl->getEndLoc())) {
            return;
          }
          ReportError(
              misra::libtooling_utils::GetFilename(decl, result.SourceManager),
              misra::libtooling_utils::GetLine(decl, result.SourceManager),
              results_list_);
          // 没有std::initializer_list为参数的构造函数，判断是否用()和=进行初始化
        } else if (IsNotInitWithBrace(result, decl->getBeginLoc(),
                                      decl->getEndLoc()))
          ReportError(
              misra::libtooling_utils::GetFilename(decl, result.SourceManager),
              misra::libtooling_utils::GetLine(decl, result.SourceManager),
              results_list_);
      }
    }
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
}  // namespace rule_A8_5_2
}  // namespace autosar
