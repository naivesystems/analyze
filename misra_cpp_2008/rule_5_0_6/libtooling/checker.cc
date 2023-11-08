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

#include "misra_cpp_2008/rule_5_0_6/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <climits>
#include <unordered_map>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;

#define TYPE_WIDTH(type) (sizeof(type) * CHAR_BIT)

void ReportError506(string error_message, string path, int line,
                    analyzer::proto::ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      misra::proto_util::AddResultToResultsList(results_list, path, line,
                                                error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_0_6);
}

std::unordered_map<std::string, int> type_size_{
    {"char", TYPE_WIDTH(char)},
    {"short", TYPE_WIDTH(short)},
    {"int", TYPE_WIDTH(int)},
    {"long", TYPE_WIDTH(long)},
    {"long long", TYPE_WIDTH(long long)},
    {"unsigned char", TYPE_WIDTH(unsigned char)},
    {"unsigned short", TYPE_WIDTH(unsigned short)},
    {"unsigned int", TYPE_WIDTH(unsigned int)},
    {"unsigned long", TYPE_WIDTH(unsigned long)},
    {"unsigned long long", TYPE_WIDTH(unsigned long long)},
    {"float", sizeof(float)},
    {"double", sizeof(double)},
    {"long double", sizeof(long double)}};

namespace misra_cpp_2008 {
namespace rule_5_0_6 {
namespace libtooling {

class IntegerCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        implicitCastExpr(
            hasType(isInteger()),
            hasSourceExpression(expr(hasType(isInteger())).bind("src_expr")),
            // The safety of Explicit conversions is guaranteed by the
            // programmer. Enumeration type  will choose the appropriate number
            // of bytes to hold their entire value.
            unless(
                anyOf(hasParent(castExpr()), hasAncestor(enumConstantDecl()))))
            .bind("cast"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    ImplicitCastExpr const* cast =
        result.Nodes.getNodeAs<ImplicitCastExpr>("cast");
    Expr const* expr = result.Nodes.getNodeAs<Expr>("src_expr");

    if (!cast || !expr) {
      return;
    }
    // skip the system header file
    if (misra::libtooling_utils::IsInSystemHeader(cast, result.Context)) {
      return;
    }

    QualType targetType =
        cast->getType().getDesugaredType(*context).getUnqualifiedType();
    const auto* BT = dyn_cast<BuiltinType>(targetType.getCanonicalType());
    if (BT && BT->getKind() == BuiltinType::UChar) {
      /*
       * we want to ignore unsigned char array declare. for example:
       *   unsigned char resource[] = {0x0, 0x1, 0xa};
       * The ast of single element is:
       *   ImplicitCastExpr 0x12a0e6e18 <line:3:3> 'const unsigned char'
       *<IntegralCast>
       *     `-IntegerLiteral 0x12a0e6d20 <col:3> 'int' 10
       * So it's hard to distinguish from simple assignment, which we want to
       * report. Now we just judge the start of expr is "0x" for convenience.
       **/
      auto char_range = Lexer::makeFileCharRange(
          CharSourceRange::getTokenRange(expr->getSourceRange()),
          *result.SourceManager, result.Context->getLangOpts());
      auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                         result.Context->getLangOpts());
      if (source.startswith_insensitive("0x")) {
        return;
      }
    }

    QualType exprType =
        expr->getType().getDesugaredType(*context).getUnqualifiedType();
    if (type_size_[exprType.getAsString()] >
        type_size_[targetType.getAsString()]) {
      string error_message = "隐式的整数转换不应使底层类型的大小变小";
      string path =
          misra::libtooling_utils::GetFilename(cast, result.SourceManager);
      int line = misra::libtooling_utils::GetLine(cast, result.SourceManager);
      ReportError506(error_message, path, line, results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class FloatCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        implicitCastExpr(
            hasType(realFloatingPointType()),
            hasSourceExpression(
                expr(hasType(realFloatingPointType())).bind("src_expr")),
            unless(hasParent(castExpr())))
            .bind("cast"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    ImplicitCastExpr const* cast =
        result.Nodes.getNodeAs<ImplicitCastExpr>("cast");
    Expr const* expr = result.Nodes.getNodeAs<Expr>("src_expr");

    if (!cast || !expr) {
      return;
    }
    // skip the system header file
    if (misra::libtooling_utils::IsInSystemHeader(cast, result.Context)) {
      return;
    }

    QualType targetType =
        cast->getType().getDesugaredType(*context).getUnqualifiedType();
    QualType exprType =
        expr->getType().getDesugaredType(*context).getUnqualifiedType();
    if (type_size_[exprType.getAsString()] >
        type_size_[targetType.getAsString()]) {
      string error_message = "隐式的浮点转换不应使底层类型的大小变小";
      string path =
          misra::libtooling_utils::GetFilename(cast, result.SourceManager);
      int line = misra::libtooling_utils::GetLine(cast, result.SourceManager);
      ReportError506(error_message, path, line, results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  integer_callback_ = new IntegerCallback;
  integer_callback_->Init(result_list, &finder_);
  float_callback_ = new FloatCallback;
  float_callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_5_0_6
}  // namespace misra_cpp_2008
