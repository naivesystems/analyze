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

#include "autosar/rule_A12_1_6/libtooling/checker.h"

#include <glog/logging.h>

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
      "Derived classes that do not need further explicit initialization and require all the constructors from the base class shall use inheriting constructors.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A12_1_6 {
namespace libtooling {
// 首先匹配所有的cxxRecordDecl，然后去遍历这个类的所有explict的构造函数，
// 首先判断类的构造函数是不是的实现是不是为空，如果为空，再去检查
// 构造函数的初始化列表中是否使用了基类的构造函数，如果使用了，
// 比较基类和派生类的构造函数每个参数是否分别初始化同一个变量，
// 如果都完全相同则说明这个构造函数应该直接被继承，记录需要被直接继承的
// 构造函数的数量，然后再去统计基类中有多少explict的构造函数，
// 如果两者数量相同，则说明这个类应该直接继承。

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        cxxRecordDecl(isClass(), hasAnyBase(hasType(cxxRecordDecl(isClass()))))
            .bind("class_decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXRecordDecl* class_decl =
        result.Nodes.getNodeAs<CXXRecordDecl>("class_decl");
    if (misra::libtooling_utils::IsInSystemHeader(class_decl, result.Context)) {
      return;
    }

    // 判断是否为多继承
    if (class_decl->getNumBases() != 1) {
      return;
    }

    int num_of_matched_ctors_by_class = 0;
    int num_of_base_ctors = 0;

    for (auto ctor_it = class_decl->method_begin();
         ctor_it != class_decl->method_end(); ++ctor_it) {
      auto ctor = dyn_cast<CXXConstructorDecl>(*ctor_it);
      if (ctor == nullptr) {
        continue;
      }

      // 去除隐式ctor
      if (!ctor->isExplicit()) {
        continue;
      }

      // 判断函数实现是否为空
      if (const Stmt* decl_body = ctor->getBody()) {
        if (const CompoundStmt* compound_stmt =
                dyn_cast<CompoundStmt>(decl_body)) {
          if (!compound_stmt->body_empty()) {
            continue;
          }
        }
      }

      for (CXXCtorInitializer* initializer : ctor->inits()) {
        if (!initializer->isWritten() ||
            !initializer
                 ->isBaseInitializer() /*判断当前initializer是否是基类的构造函数*/) {
          continue;
        }

        // 获取基类的构造函数节点
        const Expr* initializer_body = initializer->getInit();
        if (initializer_body == nullptr) {
          continue;
        }
        const CXXConstructExpr* base_ctor_expr =
            dyn_cast<CXXConstructExpr>(initializer_body);
        if (base_ctor_expr == nullptr) {
          continue;
        }

        // 判断初始化列表里是否只有基类的构造函数，
        // 派生类的构造函数参数是否都用于基类的构造函数。
        bool matched = true;
        unsigned int parm_index = 0;
        for (auto implicit_cast_expr : base_ctor_expr->children()) {
          if (parm_index >= ctor->getNumParams()) {
            matched = false;
            break;
          }
          // 获取派生类构造函数参数
          const ParmVarDecl* derive_parm = ctor->getParamDecl(parm_index);
          parm_index++;
          const DeclRefExpr* base_parm =
              dyn_cast<DeclRefExpr>(*(implicit_cast_expr->child_begin()));
          if (base_parm && base_parm->getDecl() != derive_parm) {
            matched = false;
            break;
          }
        }
        if (matched) {
          num_of_matched_ctors_by_class += 1;
        }
      }

      const CXXBaseSpecifier base_class = *(class_decl->bases().begin());
      QualType base_type = base_class.getType();
      const CXXRecordDecl* base_class_decl = base_type->getAsCXXRecordDecl();

      // 统计基类中有多少个explict构造函数
      for (const CXXConstructorDecl* ctor : base_class_decl->ctors()) {
        if (ctor->isExplicit()) {
          num_of_base_ctors++;
        }
      }

      // 数量相等，说明应该直接继承
      if (num_of_matched_ctors_by_class == num_of_base_ctors) {
        std::string path = misra::libtooling_utils::GetFilename(
            class_decl, result.SourceManager);
        int line_number =
            misra::libtooling_utils::GetLine(class_decl, result.SourceManager);
        ReportError(path, line_number, results_list_);
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
}  // namespace rule_A12_1_6
}  // namespace autosar