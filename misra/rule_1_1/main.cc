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
#include <llvm/Support/JSON.h>

#include "misra/rule_1_1/checker.h"
#include "misra/rule_1_1/lib.h"
#include "podman_image/bigmain/suffix_rule.h"

using namespace clang::tooling;
using namespace llvm;

extern cl::OptionCategory ns_libtooling_checker;
static cl::extrahelp common_help(CommonOptionsParser::HelpMessage);
extern cl::opt<std::string> results_path;
extern cl::opt<int> struct_member_limit;
extern cl::opt<int> function_parm_limit;
extern cl::opt<int> function_arg_limit;
extern cl::opt<int> nested_record_limit;
extern cl::opt<int> nested_expr_limit;
extern cl::opt<int> switch_case_limit;
extern cl::opt<int> enum_constant_limit;
extern cl::opt<int> string_char_limit;
extern cl::opt<int> extern_id_limit;
extern cl::opt<int> macro_id_limit;
extern cl::opt<int> macro_parm_limit;
extern cl::opt<int> macro_arg_limit;
extern cl::opt<int> nested_block_limit;
extern cl::opt<int> nested_include_limit;
extern cl::opt<int> iom_id_char_limit;
extern cl::opt<int> nested_cond_inclu_limit;
extern cl::opt<int> block_id_limit;

namespace misra {
namespace rule_1_1 {
int rule_1_1(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::AllowCommandLineReparsing();
  int gflag_argc = argc;
  int libtooling_argc = argc;
  misra::libtooling_utils::SplitArg(&gflag_argc, &libtooling_argc, argc, argv);
  const char** const_argv = const_cast<const char**>(argv);
  gflags::ParseCommandLineFlags(&gflag_argc, &argv, false);

  auto expected_parser = CommonOptionsParser::create(
      libtooling_argc, &const_argv[argc - libtooling_argc],
      ns_libtooling_checker);
  if (!expected_parser) {
    llvm::errs() << expected_parser.takeError();
    return 1;
  }
  CommonOptionsParser& options_parser = expected_parser.get();
  ClangTool tool(options_parser.getCompilations(),
                 options_parser.getSourcePathList());
  analyzer::proto::ResultsList all_results;
  // running ASTChecker
  misra::rule_1_1::ASTChecker ast_checker;
  LimitList limits{
      struct_member_limit,     function_parm_limit,  function_arg_limit,
      nested_record_limit,     nested_expr_limit,    switch_case_limit,
      enum_constant_limit,     string_char_limit,    extern_id_limit,
      macro_id_limit,          macro_parm_limit,     macro_arg_limit,
      nested_block_limit,      nested_include_limit, iom_id_char_limit,
      nested_cond_inclu_limit, block_id_limit};

  // running PreprocessChecker
  misra::rule_1_1::PreprocessChecker preprocess_checker{&all_results, &limits};
  int status = tool.run(&preprocess_checker);
  LOG(INFO) << "libtooling status (PreprocessChecker): " << status << endl;

  // running ASTChecker
  ast_checker.Init(&limits, &all_results);
  status =
      tool.run(newFrontendActionFactory(ast_checker.GetMatchFinder()).get());
  ast_checker.Report();
  LOG(INFO) << "libtooling status (ASTChecker): " << status << endl;

  if (misra::proto_util::GenerateProtoFile(all_results, results_path).ok()) {
    LOG(INFO) << "rule 1.1 check done";
  }
  return 0;
}
}  // namespace rule_1_1
}  // namespace misra

namespace {

podman_image::bigmain::SuffixRule _("misra/rule_1_1",
                                    misra::rule_1_1::rule_1_1);

}  // namespace
