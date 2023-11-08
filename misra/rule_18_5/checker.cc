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

#include "misra/rule_18_5/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra {
namespace rule_18_5 {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        varDecl(unless(isExpansionInSystemHeader())).bind("var_decl"), this);
    finder->addMatcher(
        fieldDecl(unless(isExpansionInSystemHeader())).bind("field_decl"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("func_decl"),
        this);
  }

  bool is_type_good(const Type* type, unsigned remaining_pointer = 2) {
    if (!type) return true;
    auto func_proto_type = type->getAs<FunctionProtoType>();
    if (func_proto_type)
      return is_type_good(func_proto_type->getReturnType().getTypePtrOrNull());
    if (!type->isPointerType()) return true;
    if (remaining_pointer == 0) return false;
    --remaining_pointer;
    return is_type_good(type->getUnqualifiedDesugaredType()
                            ->getPointeeOrArrayElementType()
                            ->getUnqualifiedDesugaredType(),
                        remaining_pointer);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    Decl* decl;
    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("var_decl");
    const FieldDecl* field_decl =
        result.Nodes.getNodeAs<FieldDecl>("field_decl");
    const FunctionDecl* func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func_decl");
    string declStr;
    bool should_skip = false;
    if (var_decl) {
      decl = const_cast<Decl*>(dyn_cast<Decl>(var_decl));
      should_skip = is_type_good(var_decl->getType().getTypePtrOrNull());
    } else if (field_decl) {
      decl = const_cast<Decl*>(dyn_cast<Decl>(field_decl));
      should_skip = is_type_good(field_decl->getType().getTypePtrOrNull());
    } else if (func_decl) {
      decl = const_cast<Decl*>(dyn_cast<Decl>(func_decl));
      should_skip = is_type_good(func_decl->getReturnType().getTypePtrOrNull());
    }
    if (should_skip) return;
    string error_message =
        "[C1304][misra-c2012-18.5]: Declarations should contain no more than "
        "two levels of pointer nesting";
    string path =
        misra::libtooling_utils::GetFilename(decl, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(decl, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_18_5);
    LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                                 line);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}

}  // namespace rule_18_5
}  // namespace misra
