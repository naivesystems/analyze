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

#include "misra_cpp_2008/rule_8_0_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/Lex/Lexer.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_8_0_1 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(declStmt(hasDescendant(varDecl())).bind("stmt"), this);
    finder->addMatcher(
        varDecl(hasDeclContext(anyOf(translationUnitDecl(), namespaceDecl())))
            .bind("decl"),
        this);
    finder->addMatcher(fieldDecl().bind("decl"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const Decl* decl = result.Nodes.getNodeAs<Decl>("decl");
    const DeclStmt* stmt = result.Nodes.getNodeAs<DeclStmt>("stmt");

    string path = " ";
    int line = -1;
    int vardecl_num = 0;

    if (decl != nullptr) {
      if (misra::libtooling_utils::IsInSystemHeader(decl, result.Context)) {
        return;
      }

      SourceLocation start_loc = decl->getBeginLoc();
      const char* ch = result.SourceManager->getCharacterData(start_loc);
      std::string str(ch);

      for (int i = 0; i < str.size(); i++) {
        if (str[i] == '\'' || str[i] == ';' || str[i] == '\"') {
          str = str.substr(0, i);
          break;
        }
      }

      int left_curly_braces = 0;
      bool non_single_init = false;
      for (auto c : str) {
        if (c == '{') {
          left_curly_braces++;
        } else if (c == '}') {
          left_curly_braces--;
        } else if (c == ',' && left_curly_braces == 0) {
          non_single_init = true;
          break;
        }
      }
      if (!non_single_init) {
        return;
      }

      path = misra::libtooling_utils::GetFilename(decl, result.SourceManager);
      line = misra::libtooling_utils::GetLine(decl, result.SourceManager);

    } else if (stmt != nullptr) {
      if (misra::libtooling_utils::IsInSystemHeader(stmt, result.Context)) {
        return;
      }

      // check the number of varDecl descendants of the declStmt
      for (auto it = stmt->decl_begin(); it != stmt->decl_end(); it++) {
        if (isa<VarDecl>(*it)) {
          vardecl_num++;
        }
      }
      if (vardecl_num <= 1) {
        return;
      }

      path = misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
      line = misra::libtooling_utils::GetLine(stmt, result.SourceManager);
    }

    string error_message =
        "初始化声明符列表必须由一个初始化声明符组成；成员声明符列表必须由一个成员声明符组成";
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_8_0_1);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_8_0_1
}  // namespace misra_cpp_2008
