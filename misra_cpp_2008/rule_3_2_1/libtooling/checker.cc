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

#include "misra_cpp_2008/rule_3_2_1/libtooling/checker.h"

#include <glog/logging.h>

#include <climits>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {
struct DeclInfo {
  string name;
  string fileline;
  string file;
  string mainfile;
  int line;

  DeclInfo(const DeclaratorDecl* decl, SourceManager* sm) {
    string name = decl->getQualifiedNameAsString();
    SourceLocation spelling_loc = sm->getSpellingLoc(decl->getLocation());
    string file =
        misra::libtooling_utils::GetLocationFilename(spelling_loc, sm);
    int line = misra::libtooling_utils::GetLocationLine(spelling_loc, sm);
    string fileline = absl::StrFormat("%s:%d", file, line);
    string mainfile =
        sm->getNonBuiltinFilenameForID(sm->getMainFileID())->str();

    this->fileline = fileline;
    this->file = file;
    this->mainfile = mainfile;
    this->name = name;
    this->line = line;
  }
};

struct VarDeclInfo {
  string type;
  string fileline;
  string file;
  string mainfile;

  VarDeclInfo(const clang::VarDecl* var_decl, DeclInfo& decl_info) {
    this->type = var_decl->getType().getAsString();
    this->file = decl_info.file;
    this->fileline = decl_info.fileline;
    this->mainfile = decl_info.mainfile;
  }
};

struct FuncDeclInfo {
  string returntype;
  string fileline;
  string file;
  string mainfile;
  std::vector<string> parameters;

  FuncDeclInfo(const clang::FunctionDecl* decl, DeclInfo& decl_info) {
    string returnstr = decl->getReturnType().getAsString();
    this->returntype = returnstr;
    this->fileline = decl_info.fileline;
    this->file = decl_info.file;
    this->mainfile = decl_info.mainfile;
    vector<string> params;
    for (int i = 0; i < decl->getNumParams(); ++i) {
      auto p = decl->getParamDecl(i);
      params.push_back(p->getType().getAsString());
    }
    this->parameters = params;
  }
};

// check if two array type are compatible
// first need to make sure we got two array types
// then check the type

// int arr[] will be compatible with any int array.
// int arr[5] will be compatible with int arr[5]
// int arr[6] will not be compatible with int arr[5]
bool isArrayCompatible(string type1, string type2) {
  size_t index1 = type1.find("[");
  size_t index2 = type2.find("[");
  if (index1 == string::npos || index2 == string::npos) {
    return false;
  }

  vector<string> words1 = {type1.substr(0, index1),
                           type1.substr(index1, type1.length())};
  vector<string> words2 = {type2.substr(0, index2),
                           type2.substr(index2, type2.length())};

  if (words1[0] != words2[0]) {
    return false;
  }
  if (words1[1] == "[]" || words2[1] == "[]") {
    return true;
  }
  if (words1[1] == words2[1]) {
    return true;
  }
  return false;
}

bool isIdenticalType(string t1, QualType qual_type) {
  if (qual_type->isArrayType()) {
    if (isArrayCompatible(t1, qual_type.getAsString())) {
      return true;
    }
    return false;
  }
  if (t1 == qual_type.getAsString()) {
    return true;
  }
  return false;
}

void ReportError(string loc, std::string other_loc, int line_number,
                 ResultsList* results_list) {
  std::string error_message = "一个对象或函数的所有声明的类型必须兼容";
  std::vector<std::string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, loc, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_3_2_1);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_3_2_1 {
namespace libtooling {
class VarCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(varDecl().bind("var"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    SourceManager* sm = result.SourceManager;
    const VarDecl* var_decl = result.Nodes.getNodeAs<VarDecl>("var");

    if (misra::libtooling_utils::IsInSystemHeader(var_decl, context)) {
      return;
    }
    if (!misra::libtooling_utils::isExternalDecl(var_decl)) {
      return;
    }

    DeclInfo decl_info = DeclInfo(var_decl, sm);

    auto find = name_info_.find(decl_info.name);
    if (find == name_info_.end()) {
      name_info_.emplace(
          make_pair(decl_info.name, VarDeclInfo(var_decl, decl_info)));
      return;
    }

    // if two decls in the same TU, skip
    if (find->second.mainfile == decl_info.mainfile) {
      return;
    }
    if (!isIdenticalType(find->second.type, var_decl->getType())) {
      ReportError(decl_info.file, find->second.file, decl_info.line,
                  results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<string, VarDeclInfo> name_info_;
};

class FuncCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionDecl().bind("func"), this);
  }

  // for a function decl, we need to make sure return type and param type are
  // identical we have 2 kinds of cases need to skip:
  // 1. if params list is identical, return type is also identical, skip
  // 2. if params list is not the same, skip
  // otherwise, report
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    SourceManager* sm = result.SourceManager;
    const FunctionDecl* func_decl =
        result.Nodes.getNodeAs<FunctionDecl>("func");

    // we only care about function declaration
    if (func_decl->hasBody()) {
      return;
    }
    if (misra::libtooling_utils::IsInSystemHeader(func_decl, context)) {
      return;
    }
    if (!misra::libtooling_utils::isExternalDecl(func_decl)) {
      return;
    }

    DeclInfo decl_info = DeclInfo(func_decl, sm);

    auto find = name_info_.find(decl_info.name);
    if (find == name_info_.end()) {
      name_info_.emplace(
          make_pair(decl_info.name, FuncDeclInfo(func_decl, decl_info)));
      return;
    }

    // if two decls in the same TU, skip
    if (find->second.mainfile == decl_info.mainfile) {
      return;
    }

    // params' type should be identical
    if (find->second.parameters.size() != func_decl->getNumParams()) {
      return;
    }

    auto find_params = find->second.parameters;
    for (int i = 0; i < func_decl->getNumParams(); ++i) {
      auto param = func_decl->getParamDecl(i);
      if (!isIdenticalType(find_params[i], param->getType())) {
        return;
      }
    }

    // return type check
    if (!isIdenticalType(find->second.returntype, func_decl->getReturnType())) {
      ReportError(decl_info.file, find->second.file, decl_info.line,
                  results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<string, FuncDeclInfo> name_info_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  var_callback_ = new VarCallback;
  var_callback_->Init(result_list, &finder_);
  func_callback_ = new FuncCallback;
  func_callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_3_2_1
}  // namespace misra_cpp_2008
