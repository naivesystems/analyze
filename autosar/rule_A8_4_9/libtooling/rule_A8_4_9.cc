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
#include "autosar/rule_A8_4_9/libtooling/checker.h"
#include "autosar/rule_A8_4_9/libtooling/lib.h"
#include "misra/libtooling_utils/libtooling_utils.h"
#include "podman_image/bigmain/suffix_rule.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace misra::libtooling_utils;

extern cl::OptionCategory ns_libtooling_checker;
extern cl::opt<std::string> results_path;

namespace autosar {
namespace rule_A8_4_9 {
namespace libtooling {
FuncInfo2ParamInfos func_info_2_param_infos;

int rule_A8_4_9(int argc, char** argv) {
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
    errs() << expected_parser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = expected_parser.get();
  ClangTool tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  analyzer::proto::ResultsList all_results;
  autosar::rule_A8_4_9::libtooling::Checker checker;
  checker.Init(&all_results);
  int status =
      tool.run(newFrontendActionFactory(checker.GetMatchFinder()).get());
  LOG(INFO) << "libtooling status: " << status;
  UpdateFuncInfo2ParamInfos(func_info_2_param_infos);
  for (auto const& it : func_info_2_param_infos) {
    FuncInfo const& func_info = it.first;
    ParamInfos const& param_infos = it.second;
    for (ParamInfo const& param_info : param_infos) {
      if (param_info.can_be_output && !param_info.is_output &&
          !param_info.is_arg)
        ReportError(func_info.path, func_info.line_number, &all_results);
    }
  }
  if (misra::proto_util::GenerateProtoFile(all_results, results_path).ok()) {
    LOG(INFO) << "rule_A8 4 9 check done";
  }
  return 0;
}
}  // namespace libtooling
}  // namespace rule_A8_4_9
}  // namespace autosar

namespace {

podman_image::bigmain::SuffixRule _(
    "autosar/rule_A8_4_9", autosar::rule_A8_4_9::libtooling::rule_A8_4_9);
}  // namespace
