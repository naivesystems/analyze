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

#ifndef ANALYZER_MISRA_RULE_5_7_CHECKER_H_
#define ANALYZER_MISRA_RULE_5_7_CHECKER_H_

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "misra/proto_util.h"

using MatchFinder = clang::ast_matchers::MatchFinder;

namespace misra {
namespace rule_5_7 {

class CheckTagDeclCallback;
/**
 * From [misra-c2012-5.7]:
 * A tag name shall be a unique identifier which infers that other name (i.e.
 * function name or variable name) can not be the same as the tag name declared
 * before.
 *
 * I keeped two maps: one is for storing the declared struct/union/enum tag name
 * and location named tag_decls_, another one is to store other declaration's
 * name and location called other_decls_.
 *
 * Branch 1:
 * When I catch a decl, I will validate if it is a varDecl or tagDecl, if not
 * it's name and location will be stored in other_decls. Then I will try to find
 * that if current decl's name is in tag_decls_ or not; if find it, report
 * error.
 * One exception is that if this decl is a typedef decl which typedefs the
 * tagDecl we stored before and the typedef name is same as the tag name, this
 * is
 * allowed.
 * i.e. struct stag {}; typedef struct stag stag;
 *
 * The procedure is like:
 * if decl is not varDecl or tagDecl:
 *   if decl is typedefDecl:
 *      check if decl's tag name is reused as typedef's name
 *      if yes:
 *          return
 *
 *   other_decls_[name of decl] = location of decl
 *   if the name of decl is in tag_decls_:
 *      reportError
 *
 *
 * Branch 2:
 * When we catch a decl which is a tagDecl. Skip invalid and anonymous TagDecl
 * first.
 * If the decl has been declared before, we need to find the
 * first declaration of it,
 * if this is the first delcaration, just keep this decl to use later.
 * Then make a TagInfo struct to store some info that we need: tag name,
 * tag type(union/struct/enum), location. Once We have this struct, we will try
 * to find that if current tagDecl's tag_name in other_decls_ or tag_decls_; if
 * find it, report error; otherwise store it in tag_decls_.
 *
 * The procedure is like:
 * if decl is tagDecl:
 *    skip invalid and anonymous decl
 *
 *    if decl->getFirstDecl() is not null:
 *        we will keep the info of it's previous declaration
 *    else:
 *        keep decl's tag name, locaton and type
 *   check if the tag name is unique in both other_decls_ and tag_decls_
 *   if unique:
 *      add the decl in tag_decls_
 *   else:
 *      reportError
 *
 *
 * Branch 3:
 * When we catch a decl which is a varDecl. We will try to extract some useful
 * infomation from it:
 * 1. we will need variable name and location
 * 2. if this variable type is (union/struct/enum), we will get the tag type and
 * tag name
 * 3. fill those info into ElaboratedVarInfo struct
 *
 * Store the variable name and location into other_decls_ for duplicating
 * checking in Branch 2
 *
 * Then check if variable name is duplicated with tag name
 * in tag_decls_; if yes, report error.
 *
 * Check if this variable's tag name in
 * tag_decls_, if yes, check if the type of this varDecl is same as the tagDecl
 * which has same tag name if no, report error.
 *
 * The procedure is like:
 * if decl is varDecl:
 *    keep name and location
 *    if decl's type is union/struct/enum:
 *         get tag name and type
 *    add this decl to other_decls_ for duplicating check in branch 2
 *
 *    make sure
 *    1. the name of the variable decl is not duplicate with tag declaration's
 *    2. if this variable is union/struct/enum type
 *       the tag name of this variable's type is declared before and the tag
 *       type must consistent with that declaration
 *     if not:
 *        reportError
 *
 *
 **/

class Checker {
 public:
  void Init(analyzer::proto::ResultsList* results_list);
  MatchFinder* GetMatchFinder() { return &finder_; }

 private:
  MatchFinder finder_;
  CheckTagDeclCallback* callback_;
  analyzer::proto::ResultsList* results_list_;
};

}  // namespace rule_5_7
}  // namespace misra

#endif  // MISRA_RULE_5_7_CHECKER_H_
