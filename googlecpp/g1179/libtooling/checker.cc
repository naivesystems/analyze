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

#include "googlecpp/g1179/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

auto param_type =
    hasParameter(0, anyOf(hasType(references(cxxRecordDecl().bind("record"))),
                          hasType(pointsTo(cxxRecordDecl().bind("record"))),
                          hasType(cxxRecordDecl().bind("record"))));

struct OpLoc {
  const char* operator_name;
  int line;
  string path;
  bool operator==(const OpLoc& oploc) const {
    return (this->path == oploc.path && this->line == oploc.line &&
            this->operator_name == oploc.operator_name);
  }
};

struct OpLocHasher {
  size_t operator()(const OpLoc& oploc) const {
    return std::hash<std::string>()(oploc.operator_name);
  }
};
void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "If you define an operator, also define any related operators that make sense, and make sure they are defined consistently";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
void ReportGenral(
    unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>>& op_map,
    int upper, analyzer::proto::ResultsList* results_list_) {
  for (auto& op : op_map) {
    if (op.second.size() != 0 && op.second.size() != upper) {
      for (auto& ele : op.second) {
        ReportError(ele.path, ele.line, results_list_);
      }
    }
  }
}

void FillInMap(int64_t record_id, const FunctionDecl* fd, SourceManager* sm,
               unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>>& m) {
  OpLoc oploc;
  oploc.operator_name = getOperatorSpelling(fd->getOverloadedOperator());
  oploc.line = misra::libtooling_utils::GetLine(fd, sm);
  oploc.path = misra::libtooling_utils::GetFilename(fd, sm);
  m[record_id].insert(oploc);
}
// for "+", "+=
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> binary_arith_add_map_;
vector<string> add{"+", "+="};
// for "-", "-=
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> binary_arith_sub_map_;
vector<string> sub{"-", "-="};
// for "*", "*=
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>>
    binary_arith_mult_map_;
vector<string> mult{"*", "*="};
// for "/", "/=
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> binary_arith_div_map_;
vector<string> div_ops{"/", "/="};
// for "%", "%=
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> binary_arith_mod_map_;
vector<string> mod{"%", "%="};
// for "<", ">", "<=", ">="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> comparsion_cmp_map_;
vector<string> cmp{"<", ">", "<=", ">="};
// for "==", "!="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> comparsion_eq_map_;
vector<string> eq{"==", "!="};
// for "[]"
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> array_subcript_map_;
vector<string> array_subcript{"[]", "[]"};
// for "&", "&="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> bitwise_and_map_;
vector<string> bitwise_and{"&", "&="};
// for "|", "|="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> bitwise_or_map_;
vector<string> bitwise_or{"|", "|="};
// for "^", "^="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> bitwise_xor_map_;
vector<string> bitwise_xor{"^", "^="};
// for "<<", "<<="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> bitwise_lshift_map_;
vector<string> lshift{"<<", "<<="};
// for ">>", ">>="
unordered_map<int64_t, unordered_set<OpLoc, OpLocHasher>> bitwise_rshift_map_;
vector<string> rshift{">>", ">>="};
}  // namespace

namespace googlecpp {
namespace g1179 {
namespace libtooling {

class BitwiseCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // we divide the operators into the following parts, based on
    // https://en.cppreference.com/w/cpp/language/operators
    auto bitwise_arith_set1 = hasAnyOverloadedOperatorName("&", "&=");
    auto bitwise_arith_set2 = hasAnyOverloadedOperatorName("|", "|=");
    auto bitwise_arith_set3 = hasAnyOverloadedOperatorName("^", "^=");
    auto bitwise_arith_set4 = hasAnyOverloadedOperatorName("<<", "<<=");
    auto bitwise_arith_set5 = hasAnyOverloadedOperatorName(">>", ">>=");
    // match the operator overload that defined in a class
    finder->addMatcher(
        cxxRecordDecl(
            unless(isImplicit()), unless(isExpansionInSystemHeader()),
            eachOf(
                forEachDescendant(
                    functionDecl(bitwise_arith_set1, isDefinition())
                        .bind("b1")),
                forEachDescendant(
                    functionDecl(bitwise_arith_set2, isDefinition())
                        .bind("b2")),
                forEachDescendant(
                    functionDecl(bitwise_arith_set3, isDefinition())
                        .bind("b3")),
                forEachDescendant(
                    functionDecl(
                        bitwise_arith_set4,
                        unless(anyOf(
                            // To skip
                            // std::ostream& operator<<(std::ostream& os, const
                            // T& obj)
                            hasParameter(0, hasType(asString("std::istream"))),
                            hasParameter(0,
                                         hasType(asString("std::ostream"))))))
                        .bind("b4")),
                forEachDescendant(
                    functionDecl(
                        bitwise_arith_set5,
                        unless(anyOf(
                            // To skip
                            // std::ostream& operator<<(std::ostream& os, const
                            // T& obj)
                            hasParameter(0, hasType(asString("std::istream"))),
                            hasParameter(0,
                                         hasType(asString("std::ostream"))))))
                        .bind("b5"))))
            .bind("record"),
        this);
    // match a operator overload that defined outside of a class
    // so we need to find which class related to this operator via the
    // paramaters
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), bitwise_arith_set1,
                     isDefinition(), param_type)
            .bind("b1"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), bitwise_arith_set2,
                     isDefinition(), param_type)
            .bind("b2"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), bitwise_arith_set3,
                     isDefinition(), param_type)
            .bind("b3"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), bitwise_arith_set4,
                     isDefinition(), param_type)
            .bind("b4"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), bitwise_arith_set5,
                     isDefinition(), param_type)
            .bind("b5"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;
    auto* bitwise_fd1 = result.Nodes.getNodeAs<FunctionDecl>("b1");
    auto* bitwise_fd2 = result.Nodes.getNodeAs<FunctionDecl>("b2");
    auto* bitwise_fd3 = result.Nodes.getNodeAs<FunctionDecl>("b3");
    auto* bitwise_fd4 = result.Nodes.getNodeAs<FunctionDecl>("b4");
    auto* bitwise_fd5 = result.Nodes.getNodeAs<FunctionDecl>("b5");
    auto* record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    int64_t id = record->getID();

    if (bitwise_fd1) {
      FillInMap(id, bitwise_fd1, source_manager, bitwise_and_map_);
    }
    if (bitwise_fd2) {
      FillInMap(id, bitwise_fd2, source_manager, bitwise_or_map_);
    }
    if (bitwise_fd3) {
      FillInMap(id, bitwise_fd3, source_manager, bitwise_xor_map_);
    }
    if (bitwise_fd4) {
      FillInMap(id, bitwise_fd4, source_manager, bitwise_lshift_map_);
    }
    if (bitwise_fd5) {
      FillInMap(id, bitwise_fd5, source_manager, bitwise_rshift_map_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class ArraySubcriptCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // we divide the operators into the following parts, based on
    // https://en.cppreference.com/w/cpp/language/operators

    // For array subscrpts, we need to define two
    // one is for write, another is for read
    auto array_subcript_set = hasOverloadedOperatorName("[]");

    // Subscript operators generally come in pairs
    finder->addMatcher(
        cxxRecordDecl(
            unless(isExpansionInSystemHeader()), unless(isImplicit()),
            forEachDescendant(
                cxxMethodDecl(array_subcript_set, isDefinition()).bind("as")))
            .bind("record"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;
    auto* as_fd = result.Nodes.getNodeAs<CXXMethodDecl>("as");
    auto* record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    int64_t id = record->getID();
    FillInMap(id, as_fd, source_manager, array_subcript_map_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class CompareCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // we divide the operators into the following parts, based on
    // https://en.cppreference.com/w/cpp/language/operators
    auto comparsions_set1 = hasAnyOverloadedOperatorName("<", ">", "<=", ">=");
    auto comparsions_set2 = hasAnyOverloadedOperatorName("==", "!=");

    finder->addMatcher(
        cxxRecordDecl(
            unless(isImplicit()), unless(isExpansionInSystemHeader()),
            eachOf(
                forEachDescendant(
                    functionDecl(comparsions_set1, isDefinition()).bind("c1")),
                forEachDescendant(
                    functionDecl(comparsions_set2, isDefinition()).bind("c2"))))
            .bind("record"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), comparsions_set1,
                     isDefinition(), param_type)
            .bind("c1"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), comparsions_set2,
                     isDefinition(), param_type)
            .bind("c2"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;
    auto* compare_fd1 = result.Nodes.getNodeAs<FunctionDecl>("c1");
    auto* compare_fd2 = result.Nodes.getNodeAs<FunctionDecl>("c2");
    result.Nodes.getNodeAs<FunctionDecl>("incrdecr_fd");
    auto* record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    int64_t id = record->getID();

    if (compare_fd1) {
      FillInMap(id, compare_fd1, source_manager, comparsion_cmp_map_);
    }
    if (compare_fd2) {
      FillInMap(id, compare_fd2, source_manager, comparsion_eq_map_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class BinopArithCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // we divide the operators into the following parts, based on
    // https://en.cppreference.com/w/cpp/language/operators
    auto binary_arith_set1 = hasAnyOverloadedOperatorName("+", "+=");
    auto binary_arith_set2 = hasAnyOverloadedOperatorName("-", "-=");
    auto binary_arith_set3 = hasAnyOverloadedOperatorName("*", "*=");
    auto binary_arith_set4 = hasAnyOverloadedOperatorName("/", "/=");
    auto binary_arith_set5 = hasAnyOverloadedOperatorName("%", "%=");
    finder->addMatcher(
        cxxRecordDecl(unless(isImplicit()), unless(isExpansionInSystemHeader()),
                      eachOf(forEachDescendant(
                                 functionDecl(binary_arith_set1, isDefinition())
                                     .bind("binary_fd1")),
                             forEachDescendant(
                                 functionDecl(binary_arith_set2, isDefinition())
                                     .bind("binary_fd2")),
                             forEachDescendant(
                                 functionDecl(binary_arith_set3, isDefinition())
                                     .bind("binary_fd3")),
                             forEachDescendant(
                                 functionDecl(binary_arith_set4, isDefinition())
                                     .bind("binary_fd4")),
                             forEachDescendant(
                                 functionDecl(binary_arith_set5, isDefinition())
                                     .bind("binary_fd5"))))
            .bind("record"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), binary_arith_set1,
                     isDefinition(), param_type)
            .bind("binary_fd1"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), binary_arith_set2,
                     isDefinition(), param_type)
            .bind("binary_fd2"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), binary_arith_set3,
                     isDefinition(), param_type)
            .bind("binary_fd3"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), binary_arith_set4,
                     isDefinition(), param_type)
            .bind("binary_fd4"),
        this);
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader()), binary_arith_set5,
                     isDefinition(), param_type)
            .bind("binary_fd5"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;
    auto* binary_fd1 = result.Nodes.getNodeAs<FunctionDecl>("binary_fd1");
    auto* binary_fd2 = result.Nodes.getNodeAs<FunctionDecl>("binary_fd2");
    auto* binary_fd3 = result.Nodes.getNodeAs<FunctionDecl>("binary_fd3");
    auto* binary_fd4 = result.Nodes.getNodeAs<FunctionDecl>("binary_fd4");
    auto* binary_fd5 = result.Nodes.getNodeAs<FunctionDecl>("binary_fd5");
    auto* record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
    int64_t id = record->getID();

    if (binary_fd1) {
      // skip unary operator +a
      if (binary_fd1->getNumParams() == 0) {
        return;
      }
      FillInMap(id, binary_fd1, source_manager, binary_arith_add_map_);
    }
    if (binary_fd2) {
      // skip unary operator -a
      if (binary_fd2->getNumParams() == 0) {
        return;
      }
      FillInMap(id, binary_fd2, source_manager, binary_arith_sub_map_);
    }
    if (binary_fd3) {
      FillInMap(id, binary_fd3, source_manager, binary_arith_mult_map_);
    }
    if (binary_fd4) {
      FillInMap(id, binary_fd4, source_manager, binary_arith_div_map_);
    }
    if (binary_fd5) {
      FillInMap(id, binary_fd5, source_manager, binary_arith_mod_map_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Run() {
  ReportGenral(binary_arith_add_map_, add.size(), results_list_);
  ReportGenral(binary_arith_sub_map_, sub.size(), results_list_);
  ReportGenral(binary_arith_mult_map_, mult.size(), results_list_);
  ReportGenral(binary_arith_div_map_, div_ops.size(), results_list_);
  ReportGenral(binary_arith_mod_map_, mod.size(), results_list_);
  ReportGenral(comparsion_cmp_map_, cmp.size(), results_list_);
  ReportGenral(comparsion_eq_map_, eq.size(), results_list_);
  ReportGenral(array_subcript_map_, array_subcript.max_size(), results_list_);
  ReportGenral(bitwise_and_map_, bitwise_and.size(), results_list_);
  ReportGenral(bitwise_or_map_, bitwise_or.size(), results_list_);
  ReportGenral(bitwise_xor_map_, bitwise_xor.size(), results_list_);
  ReportGenral(bitwise_lshift_map_, lshift.size(), results_list_);
  ReportGenral(bitwise_rshift_map_, rshift.size(), results_list_);
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  binop_callback_ = new BinopArithCallback;
  binop_callback_->Init(results_list_, &finder_);
  cmp_callback_ = new CompareCallback;
  cmp_callback_->Init(results_list_, &finder_);
  arr_callback_ = new ArraySubcriptCallback;
  arr_callback_->Init(results_list_, &finder_);
  bitwise_callback_ = new BitwiseCallback;
  bitwise_callback_->Init(results_list_, &finder_);
}
}  // namespace libtooling
}  // namespace g1179
}  // namespace googlecpp
