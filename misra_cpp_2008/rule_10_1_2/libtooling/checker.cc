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

#include "misra_cpp_2008/rule_10_1_2/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using std::unordered_map;
using std::unordered_set;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

typedef unordered_set<const CXXRecordDecl*> ClassDeclSet;
typedef unordered_map<const CXXRecordDecl*, ClassDeclSet> InheritMap;

struct ClassInfo {
  string path;
  int line;
  bool operator<(const ClassInfo& oth) const {
    return path + to_string(line) < oth.path + to_string(oth.line);
  }
};

// used to exclude visited base class nodes
// so we don't need to start from these nodes again
ClassDeclSet non_visited_class_set;

// used to identify reported classes
set<ClassInfo> reported_class_set;

// used to save information of all the classes
unordered_map<const CXXRecordDecl*, ClassInfo> info_map;

// used to save invalid virtual inheritance
// key: derived class, value: base class set
// it works through blacklist, meaning the map includes all the classes at first
InheritMap invalid_v_inherit_map;

// get the base class in the form of `CXXRecordDecl` through `CXXBaseSpecifier`
inline const CXXRecordDecl* GetBaseClass(
    const CXXBaseSpecifier& base_specifier) {
  return base_specifier.getType()->getAsCXXRecordDecl();
}

void ReportError(std::string filepath, int line_number,
                 ResultsList* results_list) {
  std::string error_message = absl::StrFormat(
      "只有在用于菱形层次结构（diamond hierarchy）时，才能声明一个基类为虚拟基类");
  analyzer::proto::Result* pb_result = AddResultToResultsList(
      results_list, filepath, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_10_1_2);
  pb_result->set_filename(filepath);
}

/**
 * @brief find every virtual inheritance chain from the certain derived class
 * @param derived_class              the final node of the inheritance tree,
 *                                   from which find virtual inheritance chain
 * @param cur_reverse_v_inherit_map  save the direct inheritance chain
 *                                   key: base class, value: derived class set
 * @note
 * 1. label visited node as true
 * 2. find virtual inheritance chains and save to `cur_reverse_v_inherit_map`
 * 3. recurse all of the base class nodes
 **/
void FindAllVChains(const CXXRecordDecl* derived_class,
                    InheritMap& cur_reverse_v_inherit_map) {
  // the current class is visited, so erase it from `non_visited_class_set`,
  // if the class has been erased before,
  // we do not need to traverse the inheritance tree again
  if (0 == non_visited_class_set.erase(derived_class)) {
    return;
  }
  for (auto& base_specifier : derived_class->bases()) {
    auto base_class = GetBaseClass(base_specifier);
    // find virtual inheritance chains and save to `cur_reverse_v_inherit_map`
    if (base_specifier.isVirtual()) {
      cur_reverse_v_inherit_map[base_class].insert(derived_class);
    }
    // recurse all of the base class nodes
    FindAllVChains(base_class, cur_reverse_v_inherit_map);
  }
}

/**
 * @brief find valid virtual chains and erase them from `invalid_v_inherit_map`
 * @param cur_reverse_v_inherit_map  virtual inheritance in the current tree,
 *                                   used to identify whether the chain is valid
 * @note
 * 1. traverse the `cur_reverse_v_inherit_map`
 * 2. if derived class of the current base class is less than one, the chain is
 *    invalid, so continue to the next loop
 * 3. if not, these inheritance chains are valid, so erase them
 **/
void EraseValidVInheritsFromMap(InheritMap& cur_reverse_v_inherit_map) {
  for (auto& cur_v_inherit : cur_reverse_v_inherit_map) {
    auto v_base = cur_v_inherit.first;
    auto v_derived_set = cur_v_inherit.second;
    // if derived class of the current base class is less than one, the chain is
    // invalid, so continue to the next loop
    if (v_derived_set.size() <= 1) {
      continue;
    }
    // if not, these inheritance chains are valid, so erase them
    for (auto& derived : v_derived_set) {
      invalid_v_inherit_map[derived].erase(v_base);
    }
  }
}

/**
 * @brief match invalid virtual inheritance and report error
 * @param result_list record the error report
 * @note
 * 1. traverse the `invalid_v_inherit_map`
 * 2. if there exists virtual base class of the current derived class in the
 *    `invalid_v_inherit`, the chain is invalid and report error
 **/
void MatchInvalidVChains(ResultsList* results_list) {
  for (auto& invalid_v_inherit : invalid_v_inherit_map) {
    auto v_derived = invalid_v_inherit.first;
    auto v_base_set = invalid_v_inherit.second;
    // if there exists virtual base class of the current derived class in the
    // `invalid_v_inherit`, the chain is invalid and report error
    if (v_base_set.size() > 0) {
      ClassInfo info{.path = info_map[v_derived].path,
                     .line = info_map[v_derived].line};
      if (reported_class_set.find(info) != reported_class_set.end()) {
        continue;
      }
      reported_class_set.insert(info);
      ReportError(info.path, info.line, results_list);
    }
  }
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_10_1_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxRecordDecl(hasAnyBase(isVirtual())).bind("class_decl"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto class_decl = result.Nodes.getNodeAs<CXXRecordDecl>("class_decl");
    // add information of the certain class declaration to info_map
    info_map[class_decl] =
        ClassInfo{.path = misra::libtooling_utils::GetFilename(
                      class_decl, result.SourceManager),
                  .line = misra::libtooling_utils::GetLine(
                      class_decl, result.SourceManager)};

    // save all the classes
    non_visited_class_set.insert(class_decl);
    for (auto& base_specifier : class_decl->bases()) {
      // add the relationship of direct virtual inheritance to `v_inherit_map`
      if (base_specifier.isVirtual()) {
        invalid_v_inherit_map[class_decl].insert(GetBaseClass(base_specifier));
      }
    }
  }

  /**
   * @brief check invalid virtual inheritance at the end of code and report
   *error
   * @note
   * 1. traverse the `non_visited_class_set`
   * 2. find all the virtual inheritance chains in the tree and erase visited
   *    nodes in `non_visited_class_set`
   * 3. find valid virtual chains and erase them from `invalid_v_inherit_map`
   * 4. match invalid virtual inheritance, note every error and report it
   **/
  void onEndOfTranslationUnit() override {
    // traverse the `non_visited_class_set`
    InheritMap cur_reverse_v_inherit_map;
    while (!non_visited_class_set.empty()) {
      auto class_decl = *non_visited_class_set.begin();
      cur_reverse_v_inherit_map.clear();
      // find all the virtual inheritance chains in the tree
      FindAllVChains(class_decl, cur_reverse_v_inherit_map);
      // find valid virtual chains and erase them from `invalid_v_inherit_map`
      EraseValidVInheritsFromMap(cur_reverse_v_inherit_map);
    }
    // match invalid virtual inheritance and report error
    MatchInvalidVChains(results_list_);

    non_visited_class_set.clear();
    info_map.clear();
    invalid_v_inherit_map.clear();
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_10_1_2
}  // namespace misra_cpp_2008
