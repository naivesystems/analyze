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

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <glog/logging.h>
#include <llvm/Support/CommandLine.h>

#include "absl/strings/match.h"
#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"
#include "misra_cpp_2008/rule_3_2_1/libtooling/checker.h"
#include "misra_cpp_2008/rule_3_2_1/libtooling/lib.h"
#include "podman_image/bigmain/suffix_rule.h"

using namespace clang;
using namespace llvm;

extern cl::OptionCategory ns_libtooling_checker;
extern cl::opt<std::string> results_path;

namespace misra_cpp_2008 {
namespace rule_3_2_1 {
namespace libtooling {
int rule_3_2_1(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::AllowCommandLineReparsing();
  int gflag_argc = argc;
  int libtooling_argc = argc;
  misra::libtooling_utils::SplitArg(&gflag_argc, &libtooling_argc, argc, argv);
  const char** const_argv = const_cast<const char**>(argv);
  gflags::ParseCommandLineFlags(&gflag_argc, &argv, false);

  auto ep = tooling::CommonOptionsParser::create(
      libtooling_argc, &const_argv[argc - libtooling_argc],
      ns_libtooling_checker);
  if (!ep) {
    llvm::errs() << ep.takeError();
    return 1;
  }
  tooling::CommonOptionsParser& options_parser = ep.get();
  vector<string> path_list = options_parser.getSourcePathList();
  if (path_list.size() != 1) {
    llvm::errs() << "The number of filepath is not equal to 1";
    return 1;
  }
  tooling::ClangTool tool(
      options_parser.getCompilations(),
      misra::libtooling_utils::GetCTUSourceFile(path_list[0]));

  analyzer::proto::ResultsList all_results;
  misra_cpp_2008::rule_3_2_1::libtooling::Checker checker;
  checker.Init(&all_results);
  int status = tool.run(
      tooling::newFrontendActionFactory(checker.GetMatchFinder()).get());
  LOG(INFO) << "libtooling status: " << status;
  if (misra::proto_util::GenerateProtoFile(all_results, results_path).ok()) {
    LOG(INFO) << "rule 3.2.1 check done";
  }
  return 0;
}
}  // namespace libtooling
}  // namespace rule_3_2_1
}  // namespace misra_cpp_2008

namespace {

podman_image::bigmain::SuffixRule _(
    "misra_cpp_2008/rule_3_2_1",
    misra_cpp_2008::rule_3_2_1::libtooling::rule_3_2_1);

}  // namespace
