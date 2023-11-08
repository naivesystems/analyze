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

#include "misra/rule_15_7/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace rule_15_7 {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(ifStmt(hasElse(ifStmt().bind("elseif"))), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto Report = [this](const string& filename, unsigned line,
                         const string& error_message) {
      analyzer::proto::Result* pb_result =
          AddResultToResultsList(results_list_, filename, line, error_message);
      pb_result->set_error_kind(
          analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_15_7);
    };

    const IfStmt* elseif = result.Nodes.getNodeAs<IfStmt>("elseif");
    const Stmt* _else = elseif->getElse();
    SourceLocation loc = elseif->getEndLoc();
    if (_else) {
      if (const CompoundStmt* cmd = dyn_cast<CompoundStmt>(_else)) {
        loc = cmd->getBeginLoc();
        if (!cmd->body_empty()) return;
      } else {
        return;
      }
    }

    SourceManager* source_manager = result.SourceManager;
    clang::FullSourceLoc location =
        result.Context->getFullLoc(elseif->getBeginLoc());
    if (location.isInvalid() || location.isInSystemHeader()) {
      return;
    }

    // spell: macro definition, expansion: macro expansion
    // SpellingLoc == ExpansionLoc if there is no macro expansion
    if (source_manager->getSpellingLoc(loc) !=
        source_manager->getExpansionLoc(loc)) {
      string expansionFilename =
          libtooling_utils::GetFilename(elseif, source_manager);
      string spellingFilename = libtooling_utils::GetRealFilename(
          source_manager->getSpellingLoc(loc), source_manager);

      unsigned spellingLine = source_manager->getSpellingLineNumber(loc);
      unsigned expansionLine = source_manager->getExpansionLineNumber(loc);
      // report error at macro definition
      // The error message is the definition line number for further use
      Report(expansionFilename, expansionLine,
             spellingFilename + ":" + to_string(spellingLine));
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_15_7
}  // namespace misra
