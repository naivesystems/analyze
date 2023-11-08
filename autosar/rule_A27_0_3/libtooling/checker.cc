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

#include "autosar/rule_A27_0_3/libtooling/checker.h"

#include <glog/logging.h>

#include <queue>

#include "absl/strings/str_format.h"
#include "clang/AST/QualTypeNames.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;
using namespace clang;

using analyzer::proto::ResultsList;
using std::string;

struct StreamInfo {
  string type;  // in/out/pos
  int line_number;
  string path;
};

struct Compare {
  bool operator()(StreamInfo const& left, StreamInfo const& right) {
    if (left.path != right.path) {
      return left.path > right.path;
    }
    return left.line_number > right.line_number;
  }
};

typedef std::priority_queue<StreamInfo, std::vector<StreamInfo>, Compare>
    stream_queue;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Alternate input and output operations on a file stream shall not be used without an intervening flush or position call.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A27_0_3 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> fstream =
        declRefExpr(
            unless(isExpansionInSystemHeader()),
            to(varDecl(hasType(asString("std::fstream"))).bind("fstream")));
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> ptr_FILE =
        declRefExpr(
            unless(isExpansionInSystemHeader()),
            to(varDecl(hasType(asString("std::FILE *"))).bind("ptr_FILE")));
    // match stream f as output, f >> str or str << f
    finder->addMatcher(
        stmt(anyOf(cxxOperatorCallExpr(
                       hasOverloadedOperatorName(">>"),
                       hasLHS(anyOf(fstream, hasDescendant(fstream)))),
                   cxxOperatorCallExpr(
                       hasOverloadedOperatorName("<<"),
                       hasRHS(anyOf(fstream, hasDescendant(fstream)))),
                   callExpr(callee(functionDecl(
                                anyOf(hasName("fputc"), hasName("fputs"),
                                      hasName("fputwc"), hasName("fputs"),
                                      hasName("fwrite")))),
                            hasDescendant(ptr_FILE))))
            .bind("ostream"),
        this);
    // match stream f as input, f << str or str >> f
    finder->addMatcher(
        stmt(anyOf(cxxOperatorCallExpr(
                       hasOverloadedOperatorName("<<"),
                       hasLHS(anyOf(fstream, hasDescendant(fstream)))),
                   cxxOperatorCallExpr(
                       hasOverloadedOperatorName(">>"),
                       hasRHS(anyOf(fstream, hasDescendant(fstream)))),
                   callExpr(callee(functionDecl(
                                anyOf(hasName("fgetc"), hasName("fgets"),
                                      hasName("fgetwc"), hasName("fgetws"),
                                      hasName("fread")))),
                            hasDescendant(ptr_FILE))))
            .bind("istream"),
        this);
    // match position function which can be used between input and output
    // operation like f.seekg() or f.flush()
    finder->addMatcher(
        stmt(anyOf(cxxMemberCallExpr(callee(functionDecl(anyOf(
                                         hasName("seekg"), hasName("flush")))),
                                     hasDescendant(fstream)),
                   callExpr(callee(functionDecl(
                                anyOf(hasName("fseek"), hasName("fflush"),
                                      hasName("fsetpos"), hasName("rewind")))),
                            hasDescendant(ptr_FILE))))
            .bind("position"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const VarDecl* fstream = result.Nodes.getNodeAs<VarDecl>("fstream");
    const VarDecl* ptr_FILE = result.Nodes.getNodeAs<VarDecl>("ptr_FILE");
    int64_t id;
    if (fstream != nullptr) {
      id = fstream->getID();
    } else {
      id = ptr_FILE->getID();
    }
    if (stream_map_.find(id) == stream_map_.end()) {
      stream_map_.emplace(id, stream_queue());
    }
    const Stmt* ostream = result.Nodes.getNodeAs<Stmt>("ostream");
    if (ostream != nullptr) {
      std::string path =
          misra::libtooling_utils::GetFilename(ostream, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(ostream, result.SourceManager);
      StreamInfo info = {"out", line_number, path};
      stream_map_[id].push(info);
    }
    const Stmt* istream = result.Nodes.getNodeAs<Stmt>("istream");
    if (istream != nullptr) {
      std::string path =
          misra::libtooling_utils::GetFilename(istream, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(istream, result.SourceManager);
      StreamInfo info = {"in", line_number, path};
      stream_map_[id].push(info);
    }
    const Stmt* position = result.Nodes.getNodeAs<Stmt>("position");
    if (position != nullptr) {
      std::string path =
          misra::libtooling_utils::GetFilename(position, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(position, result.SourceManager);
      StreamInfo info = {"pos", line_number, path};
      stream_map_[id].push(info);
    }
  }

  // If two neighbor elements in the same file are respectively output and
  // input, it will be reported.
  void Report() {
    for (auto it_map = stream_map_.begin(); it_map != stream_map_.end();
         ++it_map) {
      while (it_map->second.size() > 1) {
        StreamInfo last = it_map->second.top();
        it_map->second.pop();
        StreamInfo now = it_map->second.top();
        if (last.path != now.path) {
          continue;
        }
        if ((last.type == "in" && now.type == "out") ||
            ((last.type == "out" && now.type == "in"))) {
          ReportError(now.path, now.line_number, results_list_);
        }
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
  std::unordered_map<int64_t, stream_queue> stream_map_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

void Checker::Report() { callback_->Report(); }

}  // namespace libtooling
}  // namespace rule_A27_0_3
}  // namespace autosar
