/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_2_10_1/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, string previous_loc, string loc,
                 ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-2.10.1]: 不同的标识符不应有近似的字形";
  std::vector<std::string> locations{previous_loc, loc};
  AddMultipleLocationsResultToResultsList(results_list, path, line_number,
                                          error_message, locations);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_2_10_1 {
namespace libtooling {

class AmbiguousIdentifier {
 public:
  string id_;
  string origin_;
  string location_;
  AmbiguousIdentifier(string input, string location) {
    origin_ = input;
    location_ = location;
    id_ = input;
    id_.erase(std::remove(id_.begin(), id_.end(), '_'), id_.end());
  }

  bool isSameName(const AmbiguousIdentifier& rhs) const {
    return origin_ == rhs.origin_;
  }

  string getLocation() const { return location_; }

  bool operator==(const AmbiguousIdentifier& rhs) const {
    size_t rhs_idx = 0;
    size_t lhs_idx = 0;

    for (; rhs_idx < id_.size() && rhs_idx < rhs.id_.size();
         rhs_idx++, lhs_idx++) {
      const char c1 = id_[lhs_idx];
      const char c2 = rhs.id_[rhs_idx];

      if (tolower(c1) == tolower(c2)) {
        continue;
      } else if ((c1 == '0' && c2 == 'O') || (c1 == 'O' && c2 == '0')) {
        continue;
      } else if ((c1 == 'I' && c2 == 'l') || (c1 == 'l' && c2 == 'I')) {
        continue;
      } else if ((c1 == '1' && c2 == 'I') || (c1 == 'I' && c2 == '1')) {
        continue;
      } else if ((c1 == 'l' && c2 == '1') || (c1 == '1' && c2 == 'l')) {
        continue;
      } else if ((c1 == 'Z' && c2 == '2') || (c1 == '2' && c2 == 'Z')) {
        continue;
      } else if ((c1 == '5' && c2 == 'S') || (c1 == 'S' && c2 == '5')) {
        continue;
      } else if ((c1 == 'n' && c2 == 'h') || (c1 == 'h' && c2 == 'n')) {
        continue;
      } else if ((c1 == 'B' && c2 == '8') || (c1 == '8' && c2 == 'B')) {
        continue;
      } else if ((c1 == 'm' || c2 == 'm') &&
                 ((id_.substr(lhs_idx, 2) == "rn" && lhs_idx++) ||
                  ((rhs.id_.substr(rhs_idx, 2) == "rn") && rhs_idx++))) {
        continue;
      }
      return false;
    }

    if (lhs_idx < id_.size() || rhs_idx < rhs.id_.size()) {
      return false;
    }
    return true;
  }

  bool operator!=(const AmbiguousIdentifier& rhs) const {
    return !(*this == rhs);
  }

  bool operator<(const AmbiguousIdentifier& rhs) const {
    if (*this != rhs) {
      return id_ < rhs.id_;
    } else {
      return false;
    }
  }

  bool operator>(const AmbiguousIdentifier& rhs) const {
    return (*this != rhs) && !(*this < rhs);
  }
};

set<AmbiguousIdentifier> name_decl_;

void CheckNameCallback::Init(analyzer::proto::ResultsList* results_list,
                             ast_matchers::MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(namedDecl().bind("name"), this);
}

void CheckNameCallback::run(
    const ast_matchers::MatchFinder::MatchResult& result) {
  const NamedDecl* named_ = result.Nodes.getNodeAs<NamedDecl>("name");

  if (misra::libtooling_utils::IsInSystemHeader(named_, result.Context)) {
    return;
  }

  string name_loc =
      misra::libtooling_utils::GetLocation(named_, result.SourceManager);
  AmbiguousIdentifier name(named_->getQualifiedNameAsString(), name_loc);

  auto previous = name_decl_.find(name);
  if (previous == name_decl_.end()) {
    name_decl_.insert(name);
    return;
  }
  if (previous->isSameName(name)) {
    // ignore same name, we just report on ambiguous case.
    return;
  }
  ReportError(
      misra::libtooling_utils::GetFilename(named_, result.SourceManager),
      misra::libtooling_utils::GetLine(named_, result.SourceManager),
      previous->getLocation(), name_loc, results_list_);
  return;
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckNameCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_2_10_1
}  // namespace misra_cpp_2008
