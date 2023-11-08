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

#include "absl/strings/str_format.h"
#include "googlecpp/g1186/libtooling/checker.h"
#include "googlecpp/g1186/libtooling/lib.h"
#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"
#include "podman_image/bigmain/suffix_rule.h"

using namespace clang;
using namespace llvm;
using analyzer::proto::ResultsList;

extern cl::OptionCategory ns_libtooling_checker;
extern cl::opt<std::string> results_path;

namespace {

void ReportError(string path, int line_number, string param_name,
                 ResultsList* results_list) {
  string error_message = absl::StrFormat(
      "Non-optional input parameters should usually be values or const references\nName of the parameter: '%s'",
      param_name);
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1186 {
namespace libtooling {
FuncInfo2ParamInfos func_info_2_param_infos;
int g1186(int argc, char** argv) {
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
  tooling::CommonOptionsParser& op = ep.get();
  tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList());

  analyzer::proto::ResultsList all_results;

  googlecpp::g1186::libtooling::Checker checker;
  checker.Init(&all_results);
  int status = tool.run(
      tooling::newFrontendActionFactory(checker.GetMatchFinder()).get());
  LOG(INFO) << "libtooling status: " << status;
  for (auto const& it : func_info_2_param_infos) {
    FuncInfo const& func_info = it.first;
    ParamInfos const& param_infos = it.second;
    for (ParamInfo const& param_info : param_infos) {
      if (!param_info.is_output &&
          !param_info.is_value_or_const_reference) {  // Output
        ReportError(func_info.path, func_info.line_number, param_info.name,
                    &all_results);
      }
    }
  }

  if (misra::proto_util::GenerateProtoFile(all_results, results_path).ok()) {
    LOG(INFO) << "g1186 check done";
  }
  return 0;
}
}  // namespace libtooling
}  // namespace g1186
}  // namespace googlecpp

namespace {

podman_image::bigmain::SuffixRule _("googlecpp/g1186",
                                    googlecpp::g1186::libtooling::g1186);

}  // namespace
