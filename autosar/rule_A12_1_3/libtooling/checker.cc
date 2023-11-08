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

#include "autosar/rule_A12_1_3/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

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
      "If all user-defined constructors of a class initialize data members with constant values that are the same across all constructors, then data members shall be initialized using NSDMI instrad.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A12_1_3 {
namespace libtooling {

typedef unordered_map<const FieldDecl*, vector<pair<const Expr*, string>>*>
    CheckMap;

string getValue(const ast_matchers::MatchFinder::MatchResult& result,
                const Expr* expr) {
  clang::SourceRange range =
      SourceRange(result.SourceManager->getSpellingLoc(expr->getBeginLoc()),
                  result.SourceManager->getSpellingLoc(expr->getEndLoc()));
  auto char_range = Lexer::makeFileCharRange(
      CharSourceRange::getTokenRange(range), *result.SourceManager,
      result.Context->getLangOpts());
  auto source = Lexer::getSourceText(char_range, *result.SourceManager,
                                     result.Context->getLangOpts());

  return source.str();
}

void addExprToCheckMapItem(
    CheckMap* checkMap, const Expr* expr, const FieldDecl* FD,
    const ast_matchers::MatchFinder::MatchResult& result) {
  if (!checkMap->count(FD)) {
    vector<pair<const Expr*, string>>* valueList =
        new vector<pair<const Expr*, string>>;
    (*checkMap)[FD] = valueList;
  }
  if ((*checkMap)[FD] != nullptr) {
    pair<const Expr*, string> p(expr, getValue(result, expr));
    (*checkMap)[FD]->push_back(p);
  }
}

void markAsInitByVariable(CheckMap* checkMap, const FieldDecl* FD) {
  if (checkMap->count(FD) && (*checkMap)[FD] != nullptr) {
    delete (*checkMap)[FD];
  }
  (*checkMap)[FD] = nullptr;
}

bool hasInitByVariable(CheckMap* checkMap, const FieldDecl* FD) {
  return checkMap->count(FD) && (*checkMap)[FD] == nullptr;
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        cxxRecordDecl(hasDefinition(), unless(isExpansionInSystemHeader()))
            .bind("record"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXRecordDecl* record =
        result.Nodes.getNodeAs<CXXRecordDecl>("record");
    CheckMap initMap;

    for (auto ctor : record->ctors()) {
      if (!ctor->isUserProvided()) {
        continue;
      }
      // check constructor initializer
      for (auto init : ctor->inits()) {
        const Expr* expr = init->getInit();
        const FieldDecl* FD = init->getMember();
        if (isa<CXXDefaultInitExpr>(expr) || !FD ||
            hasInitByVariable(&initMap, FD)) {
          continue;
        }
        if (expr->isConstantInitializer(*(result.Context), false)) {
          addExprToCheckMapItem(&initMap, expr, FD, result);
        } else {
          // if a data member is initialized with a variable, it should not
          // use NSDMI
          markAsInitByVariable(&initMap, FD);
          if (FD->hasInClassInitializer()) {
            std::string path =
                misra::libtooling_utils::GetFilename(FD, result.SourceManager);
            int line_number =
                misra::libtooling_utils::GetLine(FD, result.SourceManager);
            ReportError(path, line_number, results_list_);
          }
        }
      }
      // check expr in constructor
      if (ctor->hasBody()) {
        const Stmt* body = ctor->getBody();
        queue<const Stmt*> exprQueue;
        for (auto child : body->children()) {
          exprQueue.push(child);
        }
        while (!exprQueue.empty()) {
          const Stmt* stmt = exprQueue.front();
          exprQueue.pop();
          if (!stmt) {
            continue;
          }
          if (isa<BinaryOperator>(stmt)) {
            const BinaryOperator* biOp = cast<BinaryOperator>(stmt);
            if (biOp->getOpcode() != clang::BO_Assign) {
              continue;
            }
            const Expr* lhs = biOp->getLHS()->IgnoreParens();
            if (!isa<MemberExpr>(lhs)) {
              continue;
            }
            const ValueDecl* VD = cast<MemberExpr>(lhs)->getMemberDecl();
            if (!isa<FieldDecl>(VD)) {
              continue;
            }
            const FieldDecl* FD = cast<FieldDecl>(VD);
            if (hasInitByVariable(&initMap, FD)) {
              continue;
            }
            const Expr* rhs = biOp->getRHS();
            if (rhs->isConstantInitializer(*(result.Context), false)) {
              addExprToCheckMapItem(&initMap, rhs, FD, result);
            } else {
              // if a data member is initialized with a variable, it
              // should not use NSDMI
              markAsInitByVariable(&initMap, FD);
              if (FD->hasInClassInitializer()) {
                std::string path = misra::libtooling_utils::GetFilename(
                    FD, result.SourceManager);
                int line_number =
                    misra::libtooling_utils::GetLine(FD, result.SourceManager);
                ReportError(path, line_number, results_list_);
              }
            }
          } else {
            for (auto child : stmt->children()) {
              exprQueue.push(child);
            }
          }
        }
      }
    }

    for (auto memberInit : initMap) {
      if (memberInit.second != nullptr) {
        auto values = *(memberInit.second);
        bool hasDiffValue = false;
        for (int i = 1; i < values.size(); i++) {
          if (values[i].second != values[0].second) {
            hasDiffValue = true;
            if (memberInit.first->hasInClassInitializer()) {
              // this data member is initialized with different values, so it
              // should not use NSDMI
              std::string path = misra::libtooling_utils::GetFilename(
                  memberInit.first, result.SourceManager);
              int line_number = misra::libtooling_utils::GetLine(
                  memberInit.first, result.SourceManager);
              ReportError(path, line_number, results_list_);
              break;
            }
          }
        }
        if (!hasDiffValue) {
          // this data member is initialized with the same constant values, so
          // it should use NSDMI, report error on each constructor decl
          for (auto value : values) {
            std::string path = misra::libtooling_utils::GetFilename(
                value.first, result.SourceManager);
            int line_number = misra::libtooling_utils::GetLine(
                value.first, result.SourceManager);
            ReportError(path, line_number, results_list_);
          }
        }
        delete memberInit.second;
      }
    }
    // TODO:
    // 如果一个变量在不同的构造函数里用了不同的constant
    // value进行初始化，按规则就不应该用NSDMI。

    // 现在判断两次初始化用的constant value是否相同，用的方法是找constant
    // value的source code(string)，比较两个string是否一致。
    // 这里会有一点小问题，比如，用0和0.0分别初始化同一个变量，我们现在认为constant
    // value不相同，用1和const int i=1来初始化，我们也认为不相同。
    // 因为现在看的不是值，而是字面。
    // 因此，testcase里面留了一个bad5，目前是ExpectFailure.

    // 现在没有用值来比较的原因是：
    // （1）获取初始化表达式拿到的是Expr，如果要转成int/bool/nullptr/floating/...，需要用if-else一个一个判断这个expr是不是某一类literal，
    // 然后再调API取值，取出来的值是APInt,
    // APFloat这种特殊类型，因为可能涉及到跨类型的比较（比如0和0.0），需要补充一大堆重载，感觉代码会变得很繁杂；
    // （2）诸如 const int 这种有名字的常量，暂时没找到方法把它的值读出来
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
}  // namespace rule_A12_1_3
}  // namespace autosar
