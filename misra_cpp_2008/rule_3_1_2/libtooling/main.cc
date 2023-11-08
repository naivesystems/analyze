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

#include "absl/strings/match.h"
#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"
#include "misra_cpp_2008/rule_3_1_2/libtooling/checker.h"
#include "misra_cpp_2008/rule_3_1_2/libtooling/lib.h"
#include "podman_image/bigmain/suffix_rule.h"

using namespace clang::tooling;
using namespace llvm;

extern cl::OptionCategory ns_libtooling_checker;
extern cl::opt<std::string> results_path;

namespace misra_cpp_2008 {
namespace rule_3_1_2 {
namespace libtooling {
int rule_3_1_2(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::AllowCommandLineReparsing();
  int gflag_argc = argc;
  int libtooling_argc = argc;
  misra::libtooling_utils::SplitArg(&gflag_argc, &libtooling_argc, argc, argv);
  const char** const_argv = const_cast<const char**>(argv);
  auto expected_parser = CommonOptionsParser::create(
      libtooling_argc, &const_argv[argc - libtooling_argc],
      ns_libtooling_checker);
  gflags::ParseCommandLineFlags(&gflag_argc, &argv, false);
  if (!expected_parser) {
    llvm::errs() << expected_parser.takeError();
    return 1;
  }
  CommonOptionsParser& options_parser = expected_parser.get();
  ClangTool tool(options_parser.getCompilations(),
                 options_parser.getSourcePathList());
  misra_cpp_2008::rule_3_1_2::libtooling::Checker checker;
  analyzer::proto::ResultsList all_results;

  checker.Init(&all_results);
  int status =
      tool.run(newFrontendActionFactory(checker.GetMatchFinder()).get());
  LOG(INFO) << "libtooling status: " << status;
  if (misra::proto_util::GenerateProtoFile(all_results, results_path).ok()) {
    LOG(INFO) << "rule 3.1.2 check done";
  }
  return 0;
}
}  // namespace libtooling
}  // namespace rule_3_1_2
}  // namespace misra_cpp_2008

namespace {

podman_image::bigmain::SuffixRule _(
    "misra_cpp_2008/rule_3_1_2",
    misra_cpp_2008::rule_3_1_2::libtooling::rule_3_1_2);

}  // namespace
