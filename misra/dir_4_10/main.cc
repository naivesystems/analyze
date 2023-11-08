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
#include "misra/dir_4_10/checker.h"
#include "misra/dir_4_10/lib.h"
#include "misra/libtooling_utils/libtooling_utils.h"
#include "podman_image/bigmain/suffix_rule.h"

using namespace clang::tooling;
using namespace llvm;

extern cl::OptionCategory ns_libtooling_checker;
static cl::extrahelp common_help(CommonOptionsParser::HelpMessage);
extern cl::opt<std::string> results_path;

namespace misra {
namespace dir_4_10 {
int dir_4_10(int argc, char** argv) {
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
  vector<string> path_list = options_parser.getSourcePathList();
  if (path_list.size() != 1) {
    llvm::errs() << "The number of filepath is not equal to 1";
    return 1;
  }
  ClangTool tool(options_parser.getCompilations(),
                 misra::libtooling_utils::GetCTUSourceFile(path_list[0]));
  analyzer::proto::ResultsList all_results;

  misra::dir_4_10::Checker checker;
  checker.Init(&all_results);
  int status = tool.run(&checker);
  LOG(INFO) << "libtooling status: " << status;
  if (misra::proto_util::GenerateProtoFile(all_results, results_path).ok()) {
    LOG(INFO) << "dir 4.10 check done";
  }
  return 0;
}
}  // namespace dir_4_10
}  // namespace misra

namespace {

podman_image::bigmain::SuffixRule _("misra/dir_4_10",
                                    misra::dir_4_10::dir_4_10);

}  // namespace
