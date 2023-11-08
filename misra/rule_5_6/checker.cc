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

#include "misra/rule_5_6/checker.h"

#include <glog/logging.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

/* TypedefInfo stores location and the location of its associated tag which has
 * the same name as typedef (in other cases, associated_tag_loc_ will be "") */
struct TypedefInfo {
  string loc_;
  string associated_tag_loc_;
};

/* For the following exmaple:
 * typedef struct a b;

 * The AST of a struct/enum in clang is like:
 * -TypedefDecl 0x730048 <col:1, col:18> col:18 b 'struct a':'a'
 *   -ElaboratedType 0x72fff0 'struct a' sugar
 *     -RecordType 0x72ffd0 'a'
 *       -CXXRecord 0x72ff40 'a'

 * For TypedefDecl t in the above exmaple,
 * t->getUnderlyingType() will get type pointer at ElaboratedType 0x72fff0
 * and
 * t->getUnderlyingType().getCanonicalType() will get type pointer at RecordType
 * 0x72ffd0
 *
 * use getFirstDecl to get the first declaration location to avoid false
 * positive in such case:
 *
 *    good1.c:
 *    struct a {int y;};
 *    typedef strcut a a;
 *
 * struct a has two different declaration locations, and by using getFirstDecl
 * we can get the same declaration location for these two declarations.
 */
string TypedefAssociatedTagLoc(const TypedefDecl* td,
                               SourceManager* source_manager) {
  QualType type = td->getUnderlyingType().getCanonicalType();
  Type::TypeClass type_class = type.getTypePtr()->getTypeClass();

  if ((type_class == Type::Record || type_class == Type::Enum) &&
      type->getAsTagDecl()->getNameAsString() == td->getNameAsString()) {
    TagDecl* first_tag_decl = type->getAsTagDecl()->getFirstDecl();
    return misra::libtooling_utils::GetLocation(first_tag_decl, source_manager);
  }
  return "";
}

void ReportTypedefNameNotUniqueError(string name, string loc, string other_loc,
                                     string path, int line_number,
                                     ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "[C1104][misra-c2012-5.6]: violation of misra-c2012-5.6\n"
      "Typedef: %s\n"
      "First typedef location: %s\n"
      "Duplicated typedef location: %s",
      name, loc, other_loc);
  vector<string> locations{loc, other_loc};
  analyzer::proto::Result* pb_result = AddMultipleLocationsResultToResultsList(
      results_list, path, line_number, error_message, locations);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_5_6);
  pb_result->set_name(name);
  pb_result->set_loc(loc);
  pb_result->set_other_loc(other_loc);
  LOG(INFO) << error_message;
}

}  // namespace

namespace misra {
namespace rule_5_6 {

class TypedefNameUniqueCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(namedDecl().bind("nd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const NamedDecl* nd = result.Nodes.getNodeAs<NamedDecl>("nd");
    ASTContext* context = result.Context;
    SourceManager* source_manager = result.SourceManager;
    string name = nd->getQualifiedNameAsString();
    string loc = libtooling_utils::GetLocation(nd, source_manager);
    string path = libtooling_utils::GetFilename(nd, source_manager);
    int line_number = libtooling_utils::GetLine(nd, source_manager);

    if (libtooling_utils::IsInSystemHeader(nd, context)) {
      return;
    }
    if (isa<TypedefDecl>(nd)) {
      CheckTypedef(name, loc, cast<TypedefDecl>(nd), context, source_manager,
                   path, line_number);
    }

    if (isa<DeclaratorDecl>(nd)) {
      CheckDeclaratorDecl(name, loc, path, line_number);
    }

    /* There is an exception:
     * The typedef name may be the same as the structure, union or enumeration
     * tag name associated with the typedef.
     */
    if (isa<TagDecl>(nd)) {
      const TagDecl* first_tag_decl = cast<TagDecl>(nd)->getFirstDecl();
      string first_loc =
          libtooling_utils::GetLocation(first_tag_decl, source_manager);
      CheckTagDecl(name, first_loc, path, line_number);
    }
  }

 private:
  std::unordered_map<std::string, TypedefInfo> name_typedefs_;
  std::unordered_map<std::string, std::string> declarator_name_locations_;
  std::unordered_map<std::string, std::string> tag_name_locations_;
  ResultsList* results_list_;

  void CheckTypedef(string name, string loc, const TypedefDecl* td,
                    ASTContext* context, SourceManager* source_manager,
                    string path, int line_number) {
    auto it_typedef = name_typedefs_.find(name);
    if (it_typedef != name_typedefs_.end() && loc != it_typedef->second.loc_) {
      ReportTypedefNameNotUniqueError(name, loc, it_typedef->second.loc_, path,
                                      line_number, results_list_);
      return;
    }
    /* System environment typedef such as __int128_t or __builtin_va_list may
     * violate this rule. They should be ignored */
    if (libtooling_utils::IsInSystemHeader(td, context)) {
      return;
    }
    TypedefInfo td_with_loc;
    td_with_loc.associated_tag_loc_ =
        TypedefAssociatedTagLoc(td, source_manager);
    td_with_loc.loc_ = loc;
    name_typedefs_.emplace(name, td_with_loc);
    auto it_declarator = declarator_name_locations_.find(name);
    if (it_declarator != declarator_name_locations_.end()) {
      ReportTypedefNameNotUniqueError(name, loc, it_declarator->second, path,
                                      line_number, results_list_);
    }
    auto it_tag = tag_name_locations_.find(name);
    if (it_tag != tag_name_locations_.end() &&
        td_with_loc.associated_tag_loc_ != it_tag->second) {
      ReportTypedefNameNotUniqueError(name, loc, it_tag->second, path,
                                      line_number, results_list_);
    }
  }

  void CheckDeclaratorDecl(string name, string loc, string path,
                           int line_number) {
    auto it_typedef = name_typedefs_.find(name);
    if (it_typedef != name_typedefs_.end()) {
      ReportTypedefNameNotUniqueError(name, it_typedef->second.loc_, loc, path,
                                      line_number, results_list_);
      return;
    }
    auto it_declarator = declarator_name_locations_.find(name);
    if (it_declarator == declarator_name_locations_.end()) {
      declarator_name_locations_.emplace(name, loc);
    }
  }

  void CheckTagDecl(string name, string loc, string path, int line_number) {
    auto it_typedef = name_typedefs_.find(name);
    if (it_typedef != name_typedefs_.end() &&
        it_typedef->second.associated_tag_loc_ != loc) {
      ReportTypedefNameNotUniqueError(name, it_typedef->second.loc_, loc, path,
                                      line_number, results_list_);
      return;
    }
    auto it_tag = tag_name_locations_.find(name);
    if (it_tag == tag_name_locations_.end()) {
      tag_name_locations_.emplace(name, loc);
    }
  }
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  typedef_name_unique_callback_ = new TypedefNameUniqueCallback;
  typedef_name_unique_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_5_6
}  // namespace misra
