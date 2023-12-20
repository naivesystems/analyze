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

#include "misra_cpp_2008/rule_5_2_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-5.2.2]: 一个指向虚基类（virtual base class）的指针只能通过dynamic_cast的方式被转换为一个指向派生类（derived class）的指针";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_2_2 {
namespace libtooling {

void CheckClassCastCallback::Init(ResultsList* results_list,
                                  MatchFinder* finder) {
  results_list_ = results_list;
  auto matcher = explicitCastExpr(
      unless(isExpansionInSystemHeader()), unless(cxxDynamicCastExpr()),
      hasSourceExpression(hasType(pointerType(
          pointee(recordType(hasDeclaration(cxxRecordDecl(hasDefinition())))
                      .bind("source_type"))))),
      hasDestinationType(
          pointerType(pointee(recordType(hasDeclaration(cxxRecordDecl(
              hasDefinition(), hasDirectBase(cxxBaseSpecifier(
                                   isVirtual(), hasType(type(equalsBoundNode(
                                                    "source_type"))))))))))));
  finder->addMatcher(matcher.bind("cast"), this);
}

void CheckClassCastCallback::run(const MatchFinder::MatchResult& result) {
  if (const auto* explicit_ =
          result.Nodes.getNodeAs<ExplicitCastExpr>("cast")) {
    ReportError(
        misra::libtooling_utils::GetFilename(explicit_, result.SourceManager),
        misra::libtooling_utils::GetLine(explicit_, result.SourceManager),
        results_list_);
  }
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckClassCastCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_2_2
}  // namespace misra_cpp_2008
