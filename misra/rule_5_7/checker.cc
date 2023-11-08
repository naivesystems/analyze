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

#include "misra/rule_5_7/checker.h"

#include <glog/logging.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;
using llvm::dyn_cast;
using llvm::isa;

namespace {

enum class TagType { STRUCT = 0, ENUM = 1, UNION = 2, CLASS = 3 };

struct TagInfo {
  std::string tag_name;
  std::string begin_loc;
  TagType type;
};

// for variable, we will get type and tag name info from tag type varibale
// adn only keep name and begin_loc from other variable decls.
struct VarInfo {
  std::string name;
  std::string begin_loc;
  TagType type;
  std::string tag_name;
};

void FillTagTypeAndTagNameToTagTypeVarInfo(VarInfo* var_info,
                                           const clang::VarDecl* var_decl) {
  const clang::QualType vd_type = var_decl->getType();
  const Type* type = vd_type.getTypePtr();
  if (vd_type->isEnumeralType() || vd_type->isStructureType() ||
      vd_type->isUnionType() || vd_type->isClassType()) {
    if (const ElaboratedType* elaborated_type =
            dyn_cast<ElaboratedType>(type)) {
      var_info->tag_name = elaborated_type->getAsTagDecl()->getNameAsString();
      if (elaborated_type->getKeyword() == clang::ETK_Struct ||
          vd_type->isStructureType()) {
        var_info->type = TagType::STRUCT;
      } else if (elaborated_type->getKeyword() == clang::ETK_Union ||
                 vd_type->isUnionType()) {
        var_info->type = TagType::UNION;
      } else if (elaborated_type->getKeyword() == clang::ETK_Enum ||
                 vd_type->isEnumeralType()) {
        var_info->type = TagType::ENUM;
      } else if (elaborated_type->getKeyword() == clang::ETK_Class ||
                 vd_type->isClassType()) {
        var_info->type = TagType::CLASS;
      }
    }
  }
}

struct VarInfo* MakeTagTypeVarInfo(clang::ASTContext* context,
                                   const clang::VarDecl* var_decl) {
  struct VarInfo* var_info = new VarInfo;
  var_info->name = var_decl->getNameAsString();
  var_info->begin_loc = misra::libtooling_utils::GetLocation(
      var_decl, &var_decl->getASTContext().getSourceManager());
  // if the variable decl is not tag type,
  // the tag name and type will leave as
  // empty
  FillTagTypeAndTagNameToTagTypeVarInfo(var_info, var_decl);
  return var_info;
}

struct TagInfo* MakeTagInfo(clang::ASTContext* context,
                            const clang::TagDecl* tag_decl) {
  struct TagInfo* tag_info = new TagInfo;
  tag_info->tag_name = tag_decl->getNameAsString();
  tag_info->begin_loc = misra::libtooling_utils::GetLocation(
      tag_decl, &tag_decl->getASTContext().getSourceManager());

  if (tag_decl->isEnum()) {
    tag_info->type = TagType::ENUM;
  } else if (tag_decl->isStruct()) {
    tag_info->type = TagType::STRUCT;
  } else if (tag_decl->isUnion()) {
    tag_info->type = TagType::UNION;
  } else if (tag_decl->isClass()) {
    tag_info->type = TagType::CLASS;
  }
  return tag_info;
}

void ReportError(std::string tag_name, std::string loc, std::string other_loc,
                 std::string path, int line_number, ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C1103][misra-c2012-5.7]: error tag name is not unique\n"
      "Duplicated tag name: %s\n"
      "First identifier location: %s\n"
      "Duplicated identifier location: %s",
      tag_name, loc, other_loc);
  std::vector<std::string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_5_7_DUPLICATE);
  pb_result->set_tag_name(tag_name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

void ReportForInvalid(const std::string loc, std::string path, int line_number,
                      ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "[C1103][misra-c2012-5.7]: error tag name is not unique\n"
      "Invalid declaration: at %s",
      loc);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_5_7_INVALID);
  pb_result->set_loc(loc);
  LOG(INFO) << error_message;
}

bool TypedefNameReusedInTagDecl(const clang::TypedefDecl* td) {
  const QualType type = td->getUnderlyingType().getCanonicalType();
  Type::TypeClass type_class = type.getTypePtr()->getTypeClass();
  if ((type_class == Type::Record || type_class == Type::Enum) &&
      type->getAsTagDecl()->getNameAsString() == td->getNameAsString()) {
    return true;
  }
  return false;
}

}  // namespace

namespace misra {
namespace rule_5_7 {

class CheckTagDeclCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(namedDecl().bind("named_decl"), this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const clang::NamedDecl* decl =
        result.Nodes.getNodeAs<clang::NamedDecl>("named_decl");
    clang::ASTContext* context = result.Context;
    // only for cpp code
    // cpp will have an implict decl for recordDecl
    // it will duplicate with the origin one
    // need to skip it
    if (decl->isImplicit()) {
      return;
    }
    // skip self duplicated declaration
    if (decl->getPreviousDecl()) {
      return;
    }
    // Skip using declaration.
    if (decl->getIdentifierNamespace() ==
        clang::Decl::IdentifierNamespace::IDNS_Using) {
      return;
    }
    // skip the decl in system header
    auto location = context->getFullLoc(decl->getBeginLoc());
    if (location.isInvalid() || location.isInSystemHeader()) {
      return;
    }
    std::string path =
        libtooling_utils::GetFilename(decl, result.SourceManager);
    int line_number = libtooling_utils::GetLine(decl, result.SourceManager);

    if (!isa<clang::VarDecl>(decl) && !isa<clang::TagDecl>(decl)) {
      CheckOtherDecls(decl, context, path, line_number);
    }

    if (auto tag_decl_cast = dyn_cast<clang::TagDecl>(decl)) {
      CheckTagDeclOK(tag_decl_cast, context, path, line_number);
    } else if (auto var_decl_cast = dyn_cast<clang::VarDecl>(decl)) {
      CheckVarDeclOK(var_decl_cast, context, path, line_number);
    }
  }

  void CheckVarDeclOK(const clang::VarDecl* var_decl_cast,
                      clang::ASTContext* context, std::string path,
                      int line_number) {
    const VarInfo* var_decl = MakeTagTypeVarInfo(context, var_decl_cast);
    // keep var_decl for comparing that tag name is unique
    other_decls_[var_decl->name] = var_decl->begin_loc;

    if (IsValDeclNameAndTypeInValid(var_decl)) {
      auto decl1 = tag_decls_.find(var_decl->tag_name);
      if (decl1 != tag_decls_.end()) {
        // variable tag type is different with the tag type defined before
        ReportError(decl1->second->tag_name, decl1->second->begin_loc,
                    var_decl->begin_loc, path, line_number, results_list_);
      } else if ((decl1 = tag_decls_.find(var_decl->name)) !=
                 tag_decls_.end()) {
        // predefined tag decl's name is same as the varible's name
        ReportError(decl1->second->tag_name, decl1->second->begin_loc,
                    var_decl->begin_loc, path, line_number, results_list_);
      }
    }
  }

  void CheckTagDeclOK(const clang::TagDecl* tag_decl_cast,
                      clang::ASTContext* context, std::string path,
                      int line_number) {
    // if we define two same struct in the same file,
    // the second defined struct is invalid struct definition on AST.
    if (tag_decl_cast->isInvalidDecl()) {
      std::string begin_loc = libtooling_utils::GetLocation(
          tag_decl_cast, &context->getSourceManager());
      ReportForInvalid(begin_loc, path, line_number, results_list_);
      return;
    }
    const TagInfo* tag_decl;
    tag_decl = MakeTagInfo(context, tag_decl_cast->getFirstDecl());
    // skip anonymous type
    if (tag_decl->tag_name.empty()) {
      return;
    }
    // make sure tag name is unique in all namespace
    // i.e. function name or variable name can not
    // be the same as tag name except template specialization
    auto find = other_decls_.find(tag_decl->tag_name);
    if (find != other_decls_.end() && find->second != tag_decl->begin_loc) {
      ReportError(find->first, find->second, tag_decl->begin_loc, path,
                  line_number, results_list_);
    }
    if (CheckIfTagNameUnique(tag_decl)) {
      tag_decls_[tag_decl->tag_name] = tag_decl;
    } else {
      auto previous_tag_decl = tag_decls_[tag_decl->tag_name];
      ReportError(previous_tag_decl->tag_name, previous_tag_decl->begin_loc,
                  tag_decl->begin_loc, path, line_number, results_list_);
    }
  }

  void CheckOtherDecls(const NamedDecl* decl, clang::ASTContext* context,
                       std::string path, int line_number) {
    // make sure that tag name can be reused for the same typedef
    if (auto type_def = dyn_cast<clang::TypedefDecl>(decl)) {
      if (TypedefNameReusedInTagDecl(type_def)) {
        return;
      }
    }
    if (isa<CXXConstructorDecl>(decl)) {
      return;
    }
    auto name = decl->getNameAsString();
    std::string begin_loc = libtooling_utils::GetLocation(
        decl, &decl->getASTContext().getSourceManager());
    other_decls_[name] = begin_loc;
    // make sure tag name is unique in all namespace
    // i.e. function name can not be the same as tag name
    auto prev_decl = tag_decls_.find(name);
    if (prev_decl != tag_decls_.end()) {
      ReportError(prev_decl->second->tag_name, prev_decl->second->begin_loc,
                  begin_loc, path, line_number, results_list_);
    }
    return;
  }

  bool CheckIfTagNameUnique(const TagInfo* info_container) {
    auto decl = tag_decls_.find(info_container->tag_name);
    if (decl == tag_decls_.end()) {
      return true;
    }
    // tag name should be unique
    // we need to make sure the same tag name are from the same file
    // and have the same struct/union/enum type
    return ((decl->second->begin_loc == info_container->begin_loc &&
             decl->second->type == info_container->type));
  }

  bool IsValDeclNameAndTypeInValid(const VarInfo* info_container) {
    auto name = tag_decls_.find(info_container->name);
    if (name != tag_decls_.end()) {
      return true;
    }

    auto decl = tag_decls_.find(info_container->tag_name);
    if (decl == tag_decls_.end()) {
      return true;
    }
    // if we have a struct stag
    // the variable type should also be struct stag
    return (decl->second->type != info_container->type);
  }

 private:
  std::unordered_map<std::string, const TagInfo*> tag_decls_;
  // first is name, second is begin loc
  std::unordered_map<std::string, std::string> other_decls_;
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new CheckTagDeclCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_5_7
}  // namespace misra
