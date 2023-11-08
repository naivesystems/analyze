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

#ifndef ANALYZER_MISRA_LIBTOOLING_UTILS_PATH_UTILS_H
#define ANALYZER_MISRA_LIBTOOLING_UTILS_PATH_UTILS_H

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Lex/Lexer.h>

#include <unordered_map>

using namespace std;
using namespace clang;
using namespace clang::ast_matchers;
using namespace internal;

namespace misra {
namespace libtooling_utils {

string CleanPath(string path);

string GetLocationFilename(SourceLocation loc, SourceManager* source_manager);

vector<string> GetCTUSourceFile(string source_file_path);

int GetLocationLine(SourceLocation loc, SourceManager* source_manager);

string GetFilename(const Stmt* stmt, SourceManager* source_manager);

string GetFilename(const Decl* decl, SourceManager* source_manager);

int GetLine(const Stmt* stmt, SourceManager* source_manager);

int GetLine(const Decl* decl, SourceManager* source_manager);

string GetRealLocation(SourceLocation loc, SourceManager* source_manager);

string GetRealFilename(SourceLocation loc, SourceManager* source_manager);

int GetRealLine(SourceLocation loc, SourceManager* source_manager);

string GetLocation(const Stmt* stmt, SourceManager* source_manager);

string GetLocation(const Decl* decl, SourceManager* source_manager);

bool HasHeaderSuffix(const llvm::StringRef& filename);

bool IsInSystemHeader(const Decl* decl, ASTContext* context);

bool IsInSystemHeader(const Stmt* stmt, ASTContext* context);

bool IsInMacroExpansion(const Stmt* stmt, SourceManager* source_manager);

bool isExternalDecl(const DeclaratorDecl* decl);

enum EssentialTypeCategory {
  kBoolean,
  kCharater,
  kSigned,
  kUnsigned,
  kEnum,
  kFloating,
  kUndefined,
};

EssentialTypeCategory GetEssentialTypeCategory(const Expr* expr,
                                               const ASTContext* context);

void SplitArg(int* gflag_argc, int* libtooling_argc, int argc, char** argv);

string GetTokenFromSourceLoc(SourceManager* source_manager,
                             SourceLocation begin, SourceLocation end);

bool isInHeader(const NamedDecl* decl, SourceManager* sourceManager);

void GetUnderlyingTypeOfExpr(const Expr* expr, ASTContext* context,
                             QualType& output_type);

string GetLibFDNameOfCallExpr(const CallExpr* call_expr, ASTContext* context);

string GetCalleeArgNameWithParentFD(const CallExpr* call_expr, int arg_cnt);

enum ForConditionVarFormat {
  NoCondFormat = 0,
  BinaryEqual = 1,    // =, !=
  BinaryCompare = 2,  // >=, <=, >, <
  AllCondFormat = 1 | 2
};

enum ForIncrementVarFormat {
  NoIncFormat = 0,
  BinaryInDecrease = 1,  // +=, -=
  BinaryOtherOpt = 2,    // /=, %=, /=, *=, =
  BinaryAssign = 1 | 2,  // +=, -=, /=, %=, *=, =
  UnaryInDecrease = 4,   // ++, --
  FunctionChange = 8,    // f(&x), f(*x), a.f()
  AllIncFormat = 1 | 2 | 4 | 8
};

const Matcher<Stmt> CreateComparasionMatcher(
    const ForConditionVarFormat comp_enum,
    const std::string bind_id = "cond_var");

const Matcher<Stmt> CreateAssignmentMatcher(
    const ForIncrementVarFormat assign_enum,
    const std::string find_id = "cond_var",
    const std::string decl_bind_id = "loop_counter",
    const std::string ref_bind_id = "loop_counter_ref");

const Matcher<ForStmt> CreateLoopCounterMatcher(ForConditionVarFormat cond_enum,
                                                ForIncrementVarFormat inc_enum);

const Matcher<NamedDecl> OperatorOverloading();
struct FuncInfo {
  unsigned int id;
  std::string path;
  int line_number;
  bool is_return_void;
  bool operator==(const FuncInfo& other) const { return id == other.id; }
  struct HashFunction {
    size_t operator()(const FuncInfo& func_info) const {
      return std::hash<unsigned int>()(func_info.id);
    }
  };
};
struct ParamInfo {
  std::string name;
  bool is_not_null;
  bool is_pointer_ty;
  bool can_be_output;
  bool is_output;
  bool is_arg;               // is the parameter used as an function argument
  unsigned int arg_func_id;  // function id of the callee
  unsigned int arg_pos;      // function argument position
  bool arg_checked;          // is the argument `is_output` checked in recursion
  uint64_t size_bits;        // the parameter size in bits
  bool is_reference;
};

using ParamInfos = std::vector<ParamInfo>;
using FuncInfo2ParamInfos =
    std::unordered_map<FuncInfo, ParamInfos, FuncInfo::HashFunction>;

void UpdateFuncInfo2ParamInfos(FuncInfo2ParamInfos& func_info_2_param_infos);
void AddFuncOutputParamMatchers(
    ast_matchers::MatchFinder* finder,
    ast_matchers::MatchFinder::MatchCallback* Action);
void FuncOutputParamCallback(
    const ast_matchers::MatchFinder::MatchResult& result,
    FuncInfo2ParamInfos& func_info_2_param_infos);

bool isDependent(const Expr* expr);

bool isForwardingReference(QualType qt, unsigned inner_depth = 0);

string getQualifiedName(const UnresolvedLookupExpr* ule);

const auto sideEffectInBinaryOpInMemberFunc = anyOf(
    hasLHS(declRefExpr(to(varDecl(hasStaticStorageDuration())))),
    allOf(hasLHS(memberExpr(hasObjectExpression(cxxThisExpr()))),
          hasRHS(expr(
              unless(memberExpr()), unless(implicitCastExpr(has(memberExpr()))),
              unless(callExpr(callee(functionDecl(matchesName("std::move"),
                                                  isExpansionInSystemHeader())),
                              hasArgument(0, memberExpr()))),
              unless(implicitCastExpr(has(
                  callExpr(callee(functionDecl(matchesName("std::move"),
                                               isExpansionInSystemHeader())),
                           hasArgument(0, memberExpr())))))))));

class ConstCallExprVisitor : public ConstStmtVisitor<ConstCallExprVisitor> {
 public:
  ConstCallExprVisitor(ASTContext* ctx) : ctx_(ctx){};
  void Visit(const Stmt* Node);
  void VisitCallExpr(const CallExpr* Call);
  bool ShouldReport(bool aggressive_mode);
  bool hasCallExpr = false;
  // `hasDirectCall` is marked when callee is not function pointer.
  bool hasDirectCall = false;
  // `hasPersistentSideEffects` is marked when calling fuctions that have PSE.
  bool hasPersistentSideEffects = false;

 private:
  ASTContext* ctx_;
  void Visit_(const Stmt* Node, bool* HasCallExprChild);
};

class ASTVisitor : public RecursiveASTVisitor<ASTVisitor> {
 public:
  bool VisitCXXMemberCallExpr(const CXXMemberCallExpr* call) {
    memberCalls.emplace_back(call);  // Store the node
    return true;                     // Continue traversal
  }
  bool VisitCXXOperatorCallExpr(const CXXOperatorCallExpr* op) {
    operatorCalls.emplace_back(op);  // Store the node
    return true;                     // Continue traversal
  }
  bool VisitCXXConstructExpr(const CXXConstructExpr* cce) {
    constructExprs.emplace_back(cce);  // Store the node
    return true;                       // Continue traversal
  }
  bool VisitIfStmt(const IfStmt* is) {
    ifStmts.emplace_back(is);  // Store the node
    return true;               // Continue traversal
  }
  bool VisitCXXDependentScopeMemberExpr(
      const CXXDependentScopeMemberExpr* cdsme) {
    dependentMemberExprs.emplace_back(cdsme);  // Store the node
    return true;                               // Continue traversal
  }
  bool VisitVarDecl(const VarDecl* vd) {
    varDecls.emplace_back(vd);  // Store the node
    return true;                // Continue traversal
  }
  bool VisitBinaryOperator(const BinaryOperator* bo) {
    binaryOps.emplace_back(bo);  // Store the node
    return true;                 // Continue traversal
  }
  bool VisitFunctionDecl(const FunctionDecl* fd) {
    funcDecls.emplace_back(fd);  // Store the node
    return true;                 // Continue traversal
  }
  const vector<const CXXMemberCallExpr*>& getMemberCalls() const {
    return memberCalls;
  }
  const vector<const CXXOperatorCallExpr*>& getOperatorCalls() const {
    return operatorCalls;
  }
  const vector<const CXXConstructExpr*>& getConstructExprs() const {
    return constructExprs;
  }
  const vector<const IfStmt*>& getIfStmts() const { return ifStmts; }
  const vector<const CXXDependentScopeMemberExpr*>& getDependentMemberExprs()
      const {
    return dependentMemberExprs;
  }
  const vector<const VarDecl*>& getVarDecls() const { return varDecls; }
  const vector<const BinaryOperator*>& getBinaryOps() const {
    return binaryOps;
  }
  const vector<const FunctionDecl*>& getFuncDecls() const { return funcDecls; }

 private:
  vector<const CXXMemberCallExpr*> memberCalls{};
  vector<const CXXOperatorCallExpr*> operatorCalls{};
  vector<const CXXConstructExpr*> constructExprs{};
  vector<const IfStmt*> ifStmts{};
  vector<const CXXDependentScopeMemberExpr*> dependentMemberExprs{};
  vector<const VarDecl*> varDecls{};
  vector<const BinaryOperator*> binaryOps{};
  vector<const FunctionDecl*> funcDecls{};
};

string GetExprName(const Expr* expr, SourceManager* sm, ASTContext* context);

}  // namespace libtooling_utils
}  // namespace misra

#endif
