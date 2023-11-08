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

#include "misra/rule_1_1/checker.h"

namespace {

const string error_message =
    "[C2201][misra-c2012-1.1]: The program shall contain no violations of the standard C syntax and constraints, and shall not exceed the implementation's translation limits";

void ReportStructMemberError(int struct_member_limit, int struct_member_count,
                             string struct_name, string path, int line_number,
                             ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_STRUCT_MEMBER);
  pb_result->set_struct_member_limit(to_string(struct_member_limit));
  pb_result->set_struct_member_count(to_string(struct_member_count));
  pb_result->set_name(struct_name);
}

void ReportFunctionParmError(int function_parm_limit, int function_parm_count,
                             string func_name, string path, int line_number,
                             ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_FUNCTION_PARM);
  pb_result->set_function_parm_limit(to_string(function_parm_limit));
  pb_result->set_function_parm_count(to_string(function_parm_count));
  pb_result->set_name(func_name);
}

void ReportFunctionArgError(int function_arg_limit, int function_arg_count,
                            string call_expr, string path, int line_number,
                            ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_FUNCTION_ARG);
  pb_result->set_function_arg_limit(to_string(function_arg_limit));
  pb_result->set_function_arg_count(to_string(function_arg_count));
  pb_result->set_name(call_expr);
}

void ReportNestedBlockError(int nested_block_limit, int nested_block_count,
                            string path, int line_number,
                            ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_NESTED_BLOCK);
  pb_result->set_nested_block_limit(to_string(nested_block_limit));
  pb_result->set_nested_block_count(to_string(nested_block_count));
}

void ReportNestedRecordError(int nested_record_limit, int nested_record_count,
                             string record_name, string path, int line_number,
                             ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_NESTED_RECORD);
  pb_result->set_nested_record_limit(to_string(nested_record_limit));
  pb_result->set_nested_record_count(to_string(nested_record_count));
  pb_result->set_name(record_name);
}

void ReportNestedExprError(int nested_expr_limit, int nested_expr_count,
                           string paren_expr, string path, int line_number,
                           ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_NESTED_EXPR);
  pb_result->set_nested_expr_limit(to_string(nested_expr_limit));
  pb_result->set_nested_expr_count(to_string(nested_expr_count));
  pb_result->set_name(paren_expr);
}

void ReportSwitchCaseError(int switch_case_limit, int switch_case_count,
                           string switch_stmt, string path, int line_number,
                           ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_SWITCH_CASE);
  pb_result->set_switch_case_limit(to_string(switch_case_limit));
  pb_result->set_switch_case_count(to_string(switch_case_count));
  pb_result->set_name(switch_stmt);
}

void ReportEnumConstantError(int enum_constant_limit, int enum_constant_count,
                             string enum_name, string path, int line_number,
                             ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_ENUM_CONSTANT);
  pb_result->set_enum_constant_limit(to_string(enum_constant_limit));
  pb_result->set_enum_constant_count(to_string(enum_constant_count));
  pb_result->set_name(enum_name);
}

void ReportStringCharError(int string_char_limit, int string_char_count,
                           string this_str, string path, int line_number,
                           ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_STRING_CHAR);
  pb_result->set_string_char_limit(to_string(string_char_limit));
  pb_result->set_string_char_count(to_string(string_char_count));
  pb_result->set_name(this_str);
}

void ReportExternIDError(int extern_id_limit, int extern_id_count, string path,
                         int line_number, ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_EXTERN_ID);
  pb_result->set_extern_id_limit(to_string(extern_id_limit));
  pb_result->set_extern_id_count(to_string(extern_id_count));
}

void ReportMacroIDError(int macro_id_limit, int macro_id_count, string path,
                        int line_number, ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_MACRO_ID);
  pb_result->set_macro_id_limit(to_string(macro_id_limit));
  pb_result->set_macro_id_count(to_string(macro_id_count));
}

void ReportMacroParmError(int macro_parm_limit, int macro_parm_count,
                          string macro_id, string path, int line_number,
                          ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_MACRO_PARM);
  pb_result->set_macro_parm_limit(to_string(macro_parm_limit));
  pb_result->set_macro_parm_count(to_string(macro_parm_count));
  pb_result->set_name(macro_id);
}

void ReportMacroArgError(int macro_arg_limit, int macro_arg_count,
                         string macro_id, string path, int line_number,
                         ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_MACRO_ARG);
  pb_result->set_macro_arg_limit(to_string(macro_arg_limit));
  pb_result->set_macro_arg_count(to_string(macro_arg_count));
  pb_result->set_name(macro_id);
}

void ReportNestedIncludeError(int nested_include_limit,
                              int nested_include_count, string file_name,
                              string path, int line_number,
                              ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_NESTED_INCLUDE);
  pb_result->set_nested_include_limit(to_string(nested_include_limit));
  pb_result->set_nested_include_count(to_string(nested_include_count));
  pb_result->set_name(file_name);
}

void ReportInternalOrMacroIDCharError(int iom_id_char_limit,
                                      int iom_id_char_count,
                                      string significant_id, string path,
                                      int line_number,
                                      ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_IOM_ID_CHAR);
  pb_result->set_iom_id_char_limit(to_string(iom_id_char_limit));
  pb_result->set_iom_id_char_count(to_string(iom_id_char_count));
  pb_result->set_name(significant_id);
}

void ReportNestedCondIncluError(int nested_cond_inclu_limit,
                                int nested_cond_inclu_count, string path,
                                int line_number, ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_RULE_1_1_NESTED_COND_INCLU);
  pb_result->set_nested_cond_inclu_limit(to_string(nested_cond_inclu_limit));
  pb_result->set_nested_cond_inclu_count(to_string(nested_cond_inclu_count));
}

void ReportBlockIDError(int block_id_limit, int block_id_count, string path,
                        int line_number, ResultsList* results_list) {
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_1_1_BLOCK_ID);
  pb_result->set_block_id_limit(to_string(block_id_limit));
  pb_result->set_block_id_count(to_string(block_id_count));
}

}  // namespace

namespace misra {
namespace rule_1_1 {

class StructMemberCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int struct_member_limit, ResultsList* results_list,
            MatchFinder* finder) {
    struct_member_limit_ = struct_member_limit;
    results_list_ = results_list;
    finder->addMatcher(
        recordDecl(unless(isExpansionInSystemHeader())).bind("rd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>("rd");
    int struct_member_count = 0;
    for (auto it = rd->field_begin(); it != rd->field_end(); ++it) {
      struct_member_count += 1;
    }
    if (struct_member_count > struct_member_limit_) {
      ReportStructMemberError(
          struct_member_limit_, struct_member_count,
          rd->getQualifiedNameAsString(),
          libtooling_utils::GetFilename(rd, result.SourceManager),
          libtooling_utils::GetLine(rd, result.SourceManager), results_list_);
    }
  }

 private:
  int struct_member_limit_;
  ResultsList* results_list_;
};

class FunctionParmCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int function_parm_limit, ResultsList* results_list,
            MatchFinder* finder) {
    function_parm_limit_ = function_parm_limit;
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("fd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    int function_parm_count = fd->getNumParams();
    if (function_parm_count > function_parm_limit_) {
      ReportFunctionParmError(
          function_parm_limit_, function_parm_count,
          fd->getQualifiedNameAsString(),
          libtooling_utils::GetFilename(fd, result.SourceManager),
          libtooling_utils::GetLine(fd, result.SourceManager), results_list_);
    }
  }

 private:
  int function_parm_limit_;
  ResultsList* results_list_;
};

class FunctionArgCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int function_arg_limit, ResultsList* results_list,
            MatchFinder* finder) {
    function_arg_limit_ = function_arg_limit;
    results_list_ = results_list;
    finder->addMatcher(callExpr(unless(isExpansionInSystemHeader())).bind("ce"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CallExpr* ce = result.Nodes.getNodeAs<CallExpr>("ce");
    int function_arg_count = ce->getNumArgs();
    if (function_arg_count > function_arg_limit_) {
      ReportFunctionArgError(
          function_arg_limit_, function_arg_count,
          misra::libtooling_utils::GetTokenFromSourceLoc(
              result.SourceManager, ce->getBeginLoc(), ce->getEndLoc()),
          libtooling_utils::GetFilename(ce, result.SourceManager),
          libtooling_utils::GetLine(ce, result.SourceManager), results_list_);
    }
  }

 private:
  int function_arg_limit_;
  ResultsList* results_list_;
};

class NestedRecordCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int nested_record_limit, ResultsList* results_list,
            MatchFinder* finder) {
    nested_record_limit_ = nested_record_limit;
    results_list_ = results_list;
    finder->addMatcher(
        recordDecl(unless(isExpansionInSystemHeader())).bind("rd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>("rd");
    if (dyn_cast_or_null<RecordDecl>(rd->getLexicalParent())) return;
    depth_, max_depth_ = 0;
    CheckDepth(rd);
    if (max_depth_ > nested_record_limit_)
      ReportNestedRecordError(
          nested_record_limit_, max_depth_, rd->getQualifiedNameAsString(),
          libtooling_utils::GetFilename(rd, result.SourceManager),
          libtooling_utils::GetLine(rd, result.SourceManager), results_list_);
  }

 private:
  int nested_record_limit_;
  unsigned depth_ = 0;
  unsigned max_depth_ = 0;
  ResultsList* results_list_;

  void CheckDepth(const RecordDecl* rd) {
    if (!rd) return;
    depth_++;
    max_depth_ = max<unsigned>(depth_, max_depth_);
    for (const Decl* decl : rd->decls()) {
      if (const RecordDecl* rd = dyn_cast_or_null<RecordDecl>(decl))
        CheckDepth(rd);
    }
    depth_--;
  }
};

class NestedExprCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int nested_expr_limit, ResultsList* results_list,
            MatchFinder* finder) {
    nested_expr_limit_ = nested_expr_limit;
    results_list_ = results_list;
    finder->addMatcher(
        parenExpr(unless(isExpansionInSystemHeader())).bind("pe"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const ParenExpr* pe = result.Nodes.getNodeAs<ParenExpr>("pe");
    int depth = 0;
    const ParenExpr* current = pe;
    while (current) {
      depth++;
      if (depth > nested_expr_limit_) {
        ReportNestedExprError(
            nested_expr_limit_, depth,
            misra::libtooling_utils::GetTokenFromSourceLoc(
                result.SourceManager, pe->getBeginLoc(), pe->getEndLoc()),
            libtooling_utils::GetFilename(pe, result.SourceManager),
            libtooling_utils::GetLine(pe, result.SourceManager), results_list_);
        return;
      }
      current = dyn_cast_or_null<ParenExpr>(current->getSubExpr());
    }
  }

 private:
  int nested_expr_limit_;
  ResultsList* results_list_;
};

class SwitchCaseCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int switch_case_limit, ResultsList* results_list,
            MatchFinder* finder) {
    switch_case_limit_ = switch_case_limit;
    results_list_ = results_list;
    finder->addMatcher(
        switchStmt(unless(isExpansionInSystemHeader())).bind("ss"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const SwitchStmt* ss = result.Nodes.getNodeAs<SwitchStmt>("ss");
    int case_count = 0;
    const SwitchCase* sc = ss->getSwitchCaseList();
    while (sc) {
      case_count++;
      if (case_count > switch_case_limit_) {
        ReportSwitchCaseError(
            switch_case_limit_, case_count,
            misra::libtooling_utils::GetTokenFromSourceLoc(
                result.SourceManager, ss->getBeginLoc(), ss->getEndLoc()),
            libtooling_utils::GetFilename(ss, result.SourceManager),
            libtooling_utils::GetLine(ss, result.SourceManager), results_list_);
        return;
      }
      sc = sc->getNextSwitchCase();
    }
  }

 private:
  int switch_case_limit_;
  ResultsList* results_list_;
};

class EnumConstantCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int enum_constant_limit, ResultsList* results_list,
            MatchFinder* finder) {
    enum_constant_limit_ = enum_constant_limit;
    results_list_ = results_list;
    finder->addMatcher(enumDecl(unless(isExpansionInSystemHeader())).bind("ed"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const EnumDecl* ed = result.Nodes.getNodeAs<EnumDecl>("ed");
    int enum_constant_count = 0;
    for (auto enumerator : ed->enumerators()) {
      enum_constant_count++;
    }
    if (enum_constant_count > enum_constant_limit_) {
      ReportEnumConstantError(
          enum_constant_limit_, enum_constant_count,
          ed->getQualifiedNameAsString(),
          libtooling_utils::GetFilename(ed, result.SourceManager),
          libtooling_utils::GetLine(ed, result.SourceManager), results_list_);
    }
  }

 private:
  int enum_constant_limit_;
  ResultsList* results_list_;
};

class StringCharCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int string_char_limit, ResultsList* results_list,
            MatchFinder* finder) {
    string_char_limit_ = string_char_limit;
    results_list_ = results_list;
    finder->addMatcher(
        stringLiteral(unless(isExpansionInSystemHeader())).bind("sl"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const StringLiteral* sl = result.Nodes.getNodeAs<StringLiteral>("sl");
    int string_char_count = sl->getLength();
    if (string_char_count > string_char_limit_) {
      ReportStringCharError(
          string_char_limit_, string_char_count, sl->getString().str(),
          libtooling_utils::GetFilename(sl, result.SourceManager),
          libtooling_utils::GetLine(sl, result.SourceManager), results_list_);
    }
  }

 private:
  int string_char_limit_;
  ResultsList* results_list_;
};

class ExternIDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int extern_id_limit, ResultsList* results_list,
            MatchFinder* finder) {
    extern_id_limit_ = extern_id_limit;
    results_list_ = results_list;
    finder->addMatcher(
        translationUnitDecl(unless(isExpansionInSystemHeader())).bind("tud"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const TranslationUnitDecl* tud =
        result.Nodes.getNodeAs<TranslationUnitDecl>("tud");
    misra::libtooling_utils::ASTVisitor visitor;
    visitor.TraverseDecl(const_cast<TranslationUnitDecl*>(tud));
    int extern_id_count = 0;
    for (const VarDecl* vd : visitor.getVarDecls()) {
      if (vd->hasExternalFormalLinkage()) extern_id_count++;
    }
    for (const FunctionDecl* fd : visitor.getFuncDecls()) {
      if (fd->hasExternalFormalLinkage()) extern_id_count++;
    }
    if (extern_id_count > extern_id_limit_) {
      ReportExternIDError(
          extern_id_count, extern_id_limit_,
          libtooling_utils::GetFilename(tud, result.SourceManager),
          libtooling_utils::GetLine(tud, result.SourceManager), results_list_);
    }
  }

 private:
  int extern_id_limit_;
  ResultsList* results_list_;
};

struct Loc {
  string file;
  int line;
};

std::map<string, std::stack<Loc>> nested_blocks;

class NestedBlockCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int nested_block_limit, ResultsList* results_list,
            MatchFinder* finder) {
    nested_block_limit_ = nested_block_limit;
    results_list_ = results_list;
    finder->addMatcher(
        compoundStmt(unless(isExpansionInSystemHeader()),
                     forEachDescendant(
                         compoundStmt(unless(hasDescendant(compoundStmt())))
                             .bind("innermost_cstmt")))
            .bind("cstmt"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CompoundStmt* cstmt = result.Nodes.getNodeAs<CompoundStmt>("cstmt");
    const CompoundStmt* innermost_cstmt =
        result.Nodes.getNodeAs<CompoundStmt>("innermost_cstmt");
    if (cstmt && innermost_cstmt) {
      // the loc is in a format as file:line:column, which is unique for a block
      string loc =
          innermost_cstmt->getLBracLoc().printToString(*result.SourceManager);
      nested_blocks[loc].push(
          Loc{libtooling_utils::GetFilename(cstmt, result.SourceManager),
              libtooling_utils::GetLine(cstmt, result.SourceManager)});
    }
  }

  void Report() {
    for (auto it = nested_blocks.begin(); it != nested_blocks.end(); ++it) {
      std::stack<Loc> block_stack = it->second;
      // use '<': the block_stack does not store the innermost block
      if (block_stack.size() < nested_block_limit_) {
        continue;
      }
      // 1: the innermost block
      int nested_block_count = block_stack.size() + 1;
      int count = 1;
      while (!block_stack.empty()) {
        count++;
        Loc loc = block_stack.top();
        block_stack.pop();
        if (count > nested_block_limit_) {
          ReportNestedBlockError(nested_block_limit_, nested_block_count,
                                 loc.file, loc.line, results_list_);
        }
      }
    }
  }

 private:
  int nested_block_limit_;
  ResultsList* results_list_;
};

map<string, Identifier> internal_or_macro_significant_ids{};

class InternIDCharCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int iom_id_char_limit, ResultsList* results_list,
            MatchFinder* finder) {
    iom_id_char_limit_ = iom_id_char_limit;
    results_list_ = results_list;
    finder->addMatcher(
        namedDecl(unless(isExpansionInSystemHeader())).bind("nd"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const NamedDecl* nd = result.Nodes.getNodeAs<NamedDecl>("nd");
    string decl_name = nd->getNameAsString();
    string significant_decl_name =
        decl_name.substr(0, min<int>(decl_name.length(), iom_id_char_limit_));
    if (nd->getFormalLinkage() == Linkage::InternalLinkage) {
      if (internal_or_macro_significant_ids.find(significant_decl_name) !=
          internal_or_macro_significant_ids.end()) {
        ReportInternalOrMacroIDCharError(
            iom_id_char_limit_, decl_name.length(), significant_decl_name,
            libtooling_utils::GetFilename(nd, result.SourceManager),
            libtooling_utils::GetLine(nd, result.SourceManager), results_list_);
      } else {
        internal_or_macro_significant_ids[significant_decl_name] =
            Identifier{decl_name, nd->getLocation()};
      }
    }
  }

 private:
  int iom_id_char_limit_;
  ResultsList* results_list_;
};

struct Info {
  string file;
  int line;
  int count;
};

std::map<string, Info> block_ids;

class BlockIDCallback : public MatchFinder::MatchCallback {
 public:
  void Init(int block_id_limit, ResultsList* results_list,
            MatchFinder* finder) {
    block_id_limit_ = block_id_limit;
    results_list_ = results_list;
    finder->addMatcher(compoundStmt(unless(isExpansionInSystemHeader()),
                                    forEachDescendant(varDecl().bind("vd")))
                           .bind("cstmt"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CompoundStmt* cstmt = result.Nodes.getNodeAs<CompoundStmt>("cstmt");
    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>("vd");
    if (cstmt && vd) {
      // the loc is in a format as file:line:column, which is unique for a
      // block
      string loc = cstmt->getLBracLoc().printToString(*result.SourceManager);
      if (block_ids.find(loc) == block_ids.end()) {
        block_ids[loc] =
            Info{libtooling_utils::GetFilename(cstmt, result.SourceManager),
                 libtooling_utils::GetLine(cstmt, result.SourceManager), 0};
      }
      // record the depth of nesting
      block_ids[loc].count++;
    }
  }

  void Report() {
    for (auto it = block_ids.begin(); it != block_ids.end(); ++it) {
      if (it->second.count > block_id_limit_) {
        ReportBlockIDError(block_id_limit_, it->second.count, it->second.file,
                           it->second.line, results_list_);
      }
    }
  }

 private:
  int block_id_limit_;
  ResultsList* results_list_;
};

void PPCheck::MacroExpands(const Token& MacroNameTok, const MacroDefinition& MD,
                           SourceRange Range, const MacroArgs* Args) {
  unsigned arg_count = Args ? Args->getNumMacroArguments() : 0;
  SourceLocation sl = MacroNameTok.getLocation();
  if (arg_count > limits_->macro_arg_limit &&
      !source_manager_->isInSystemHeader(sl))
    ReportMacroArgError(
        limits_->macro_arg_limit, arg_count, MacroNameTok.getName(),
        libtooling_utils::GetLocationFilename(sl, source_manager_),
        libtooling_utils::GetLocationLine(sl, source_manager_), results_list_);
}

void PPCheck::LexedFileChanged(FileID FID, LexedFileChangeReason Reason,
                               SrcMgr::CharacteristicKind FileType,
                               FileID PrevFID, SourceLocation Loc) {
  if (source_manager_->isInSystemHeader(Loc)) return;

  if (Reason == LexedFileChangeReason::EnterFile) {
    include_depth_++;
    include_max_depth_ = max<int>(include_depth_, include_max_depth_);
  } else {
    include_depth_--;
    if (include_depth_ == 0) {
      const FileEntry* fe = source_manager_->getFileEntryForID(FID);
      if (include_max_depth_ > limits_->nested_include_limit && fe)
        ReportNestedIncludeError(
            limits_->nested_include_limit, include_max_depth_,
            fe->getName().str(),
            libtooling_utils::GetLocationFilename(Loc, source_manager_),
            libtooling_utils::GetLocationLine(Loc, source_manager_),
            results_list_);
      include_max_depth_ = 0;
    }
  }
}

void PPCheck::If(SourceLocation Loc, SourceRange ConditionRange,
                 ConditionValueKind ConditionValue) {
  if (source_manager_->isInSystemHeader(Loc)) return;
  cond_inclu_depth_++;
  cond_inclu_max_depth_ =
      max<unsigned>(cond_inclu_depth_, cond_inclu_max_depth_);
}

void PPCheck::Ifdef(SourceLocation Loc, const Token& MacroNameTok,
                    const MacroDefinition& MD) {
  if (source_manager_->isInSystemHeader(Loc)) return;
  cond_inclu_depth_++;
  cond_inclu_max_depth_ =
      max<unsigned>(cond_inclu_depth_, cond_inclu_max_depth_);
}

void PPCheck::Ifndef(SourceLocation Loc, const Token& MacroNameTok,
                     const MacroDefinition& MD) {
  if (source_manager_->isInSystemHeader(Loc)) return;
  cond_inclu_depth_++;
  cond_inclu_max_depth_ =
      max<unsigned>(cond_inclu_depth_, cond_inclu_max_depth_);
}

void PPCheck::Endif(SourceLocation Loc, SourceLocation IfLoc) {
  if (source_manager_->isInSystemHeader(Loc)) return;
  cond_inclu_depth_--;
  if (cond_inclu_depth_ == 0) {
    if (cond_inclu_max_depth_ > limits_->nested_cond_inclu_limit)
      ReportNestedCondIncluError(
          limits_->nested_cond_inclu_limit, cond_inclu_max_depth_,
          libtooling_utils::GetLocationFilename(IfLoc, source_manager_),
          libtooling_utils::GetLocationLine(IfLoc, source_manager_),
          results_list_);
    cond_inclu_max_depth_ = 0;
  }
}

void ASTChecker::Init(LimitList* limits, ResultsList* results_list) {
  results_list_ = results_list;
  struct_member_callback_ = new StructMemberCallback;
  struct_member_callback_->Init(limits->struct_member_limit, results_list_,
                                &finder_);
  function_parm_callback_ = new FunctionParmCallback;
  function_parm_callback_->Init(limits->function_parm_limit, results_list_,
                                &finder_);
  function_arg_callback_ = new FunctionArgCallback;
  function_arg_callback_->Init(limits->function_arg_limit, results_list_,
                               &finder_);
  nested_record_callback_ = new NestedRecordCallback;
  nested_record_callback_->Init(limits->nested_record_limit, results_list_,
                                &finder_);
  nested_expr_callback_ = new NestedExprCallback;
  nested_expr_callback_->Init(limits->nested_expr_limit, results_list_,
                              &finder_);
  nested_block_callback_ = new NestedBlockCallback;
  nested_block_callback_->Init(limits->nested_block_limit, results_list_,
                               &finder_);
  block_id_callback_ = new BlockIDCallback;
  block_id_callback_->Init(limits->block_id_limit, results_list_, &finder_);
  switch_case_callback_ = new SwitchCaseCallback;
  switch_case_callback_->Init(limits->switch_case_limit, results_list_,
                              &finder_);
  enum_constant_callback_ = new EnumConstantCallback;
  enum_constant_callback_->Init(limits->enum_constant_limit, results_list_,
                                &finder_);
  string_char_callback_ = new StringCharCallback;
  string_char_callback_->Init(limits->string_char_limit, results_list_,
                              &finder_);
  extern_id_callback_ = new ExternIDCallback;
  extern_id_callback_->Init(limits->extern_id_limit, results_list_, &finder_);
  intern_id_char_callback_ = new InternIDCharCallback;
  intern_id_char_callback_->Init(limits->iom_id_char_limit, results_list_,
                                 &finder_);
}

void ASTChecker::Report() {
  nested_block_callback_->Report();
  block_id_callback_->Report();
}

void PreprocessConsumer::HandleTranslationUnit(ASTContext& context) {
  Preprocessor& pp = compiler_.getPreprocessor();
  SourceManager& sm = context.getSourceManager();
  const TranslationUnitDecl* tud = context.getTranslationUnitDecl();
  set<string> macro_ids{};
  for (const auto& macro : pp.macros()) {
    const MacroInfo* info = pp.getMacroInfo(macro.getFirst());
    if (!info) continue;
    SourceLocation sl = info->getDefinitionLoc();
    unsigned macro_parm_count = info->getNumParams();
    if (sl.isValid() && !sm.isInSystemHeader(sl) && !sm.isInSystemMacro(sl) &&
        !info->isBuiltinMacro() && sm.isInMainFile(sl)) {
      string macro_id = macro.first->getName().str();
      macro_ids.insert(macro_id);
      internal_or_macro_significant_ids.emplace(
          macro_id.substr(
              0, min<int>(macro_id.length(), limits_->iom_id_char_limit)),
          Identifier{macro_id, sl});
      if (macro_parm_count > limits_->macro_parm_limit)
        ReportMacroParmError(
            limits_->macro_parm_limit, macro_parm_count, macro_id,
            libtooling_utils::GetLocationFilename(sl, &sm),
            libtooling_utils::GetLocationLine(sl, &sm), results_list_);
    }
  }
  if (macro_ids.size() > limits_->macro_id_limit)
    ReportMacroIDError(limits_->macro_id_limit, macro_ids.size(),
                       libtooling_utils::GetFilename(tud, &sm),
                       libtooling_utils::GetLine(tud, &sm), results_list_);
}

bool PreprocessAction::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<PPCheck> callback(
      new PPCheck(&ci.getSourceManager(), limits_, results_list_));
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(callback));
  return true;
}

}  // namespace rule_1_1
}  // namespace misra
