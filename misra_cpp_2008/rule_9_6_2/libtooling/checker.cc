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

#include "misra_cpp_2008/rule_9_6_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_9_6_2 {
namespace libtooling {
class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(fieldDecl(isBitField()).bind("decl"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Decl* s = result.Nodes.getNodeAs<Decl>("decl");

    if (misra::libtooling_utils::IsInSystemHeader(s, result.Context)) {
      return;
    }

    // use getCharacterData to convert declaration to string
    SourceLocation start_loc = s->getBeginLoc();
    const char* ch = result.SourceManager->getCharacterData(start_loc);
    std::string str(ch);

    string types[] = {"signed char",    "unsigned char", "signed short",
                      "unsigned short", "signed int",    "unsigned int",
                      "unsigned long",  "signed long",   "bool",
                      "uint8_t",        "uint16_t",      "uint32_t",
                      "uint64_t",       "int8_t",        "int16_t",
                      "int32_t",        "int64_t"};

    for (int i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
      if (str.find(types[i]) != std::string::npos) {
        return;
      }
    }
    string error_message = "位域应为 bool 类型或显式无符号或有符号整数类型";
    string path = misra::libtooling_utils::GetFilename(s, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(s, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_9_6_2);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_9_6_2
}  // namespace misra_cpp_2008