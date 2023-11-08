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

#include "autosar/rule_A4_5_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace autosar {
namespace rule_A4_5_1 {
namespace libtooling {

void ReportError(string path, int line_number,
                 analyzer::proto::ResultsList* results_list) {
  string error_message =
      "Expressions with type enum or enum class shall not be used as operands to built-in and overloaded operators other than the subscript operator [ ], the assignment operator =, the equality operators == and !=, the unary $ operator, and the relational operators <, <=, >, >=.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(binaryOperator().bind("bin_op"), this);
    finder->addMatcher(unaryOperator().bind("un_op"), this);
    finder->addMatcher(cxxOperatorCallExpr().bind("op_call"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const BinaryOperator* bin_op =
        result.Nodes.getNodeAs<BinaryOperator>("bin_op");
    if (bin_op &&
        !misra::libtooling_utils::IsInSystemHeader(bin_op, result.Context)) {
      auto lhs_type =
          bin_op->getLHS()->IgnoreImpCasts()->getType().getCanonicalType();
      auto rhs_type =
          bin_op->getRHS()->IgnoreImpCasts()->getType().getCanonicalType();
      if (lhs_type->isEnumeralType() || rhs_type->isEnumeralType()) {
        auto opcode = bin_op->getOpcode();
        if (opcode != BO_Assign && opcode != BO_EQ && opcode != BO_NE &&
            opcode != BO_LT && opcode != BO_GT && opcode != BO_LE &&
            opcode != BO_GE) {
          ReportError(
              misra::libtooling_utils::GetFilename(bin_op,
                                                   result.SourceManager),
              misra::libtooling_utils::GetLine(bin_op, result.SourceManager),
              results_list_);
        }
      }
    }

    const UnaryOperator* un_op = result.Nodes.getNodeAs<UnaryOperator>("un_op");
    if (un_op &&
        !misra::libtooling_utils::IsInSystemHeader(un_op, result.Context)) {
      auto operand_type =
          un_op->getSubExpr()->IgnoreImpCasts()->getType().getCanonicalType();
      if (operand_type->isEnumeralType()) {
        auto opcode = un_op->getOpcode();
        if (opcode != UO_AddrOf) {
          ReportError(
              misra::libtooling_utils::GetFilename(un_op, result.SourceManager),
              misra::libtooling_utils::GetLine(un_op, result.SourceManager),
              results_list_);
        }
      }
    }

    const CXXOperatorCallExpr* op_call =
        result.Nodes.getNodeAs<CXXOperatorCallExpr>("op_call");
    if (op_call &&
        !misra::libtooling_utils::IsInSystemHeader(op_call, result.Context)) {
      bool isError = false;
      auto opcode_kind = op_call->getOperator();
      std::string opcode_str =
          std::string(clang::getOperatorSpelling(opcode_kind));
      if (op_call->getNumArgs() == 2) {
        auto lhs_type = op_call->getArg(0)->getType().getCanonicalType();
        auto rhs_type = op_call->getArg(1)->getType().getCanonicalType();
        if (lhs_type->isEnumeralType() || rhs_type->isEnumeralType()) {
          if (opcode_str != "==" && opcode_str != "!=" && opcode_str != "<" &&
              opcode_str != "<=" && opcode_str != ">" && opcode_str != ">=" &&
              opcode_str != "=" && opcode_str != "[]") {
            isError = true;
          }
        }
      }
      if (op_call->getNumArgs() == 1) {
        auto operand_type = op_call->getArg(0)->getType().getCanonicalType();
        if (operand_type->isEnumeralType()) {
          if (opcode_str != "&") {
            isError = true;
          }
        }
      }
      if (isError) {
        ReportError(
            misra::libtooling_utils::GetFilename(op_call, result.SourceManager),
            misra::libtooling_utils::GetLine(op_call, result.SourceManager),
            results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_A4_5_1
}  // namespace autosar
