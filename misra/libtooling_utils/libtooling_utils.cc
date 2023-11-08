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

#include "libtooling_utils.h"

#include <clang/Tooling/Tooling.h>

#include "absl/strings/str_split.h"
#include "fstream"

using namespace std;
using namespace clang;
using namespace ast_matchers;
using namespace internal;

namespace misra {
namespace libtooling_utils {

// TODO:
// implement other features in https://pkg.go.dev/path/filepath#Clean

// remove ".." in file path or location
string CleanPath(string path) {
  std::vector<std::string> split_path = absl::StrSplit(path, "/");
  std::stack<string> folder_stack;

  for (auto it = split_path.begin(); it != split_path.end(); it++) {
    if (*it == "..") {
      if (folder_stack.empty()) {
        return "";
      }
      folder_stack.pop();
    } else if (*it != "" && *it != ".") {
      folder_stack.push(*it);
    }
  }

  string cleaned_path = "";
  while (!folder_stack.empty()) {
    cleaned_path = "/" + folder_stack.top() + cleaned_path;
    folder_stack.pop();
  }
  return cleaned_path;
}

string GenerateLoction(string file_name, int line_number, int colomn_number) {
  return file_name + ":" + to_string(line_number) + ":" +
         to_string(colomn_number);
}

string GetLocationFilename(SourceLocation loc, SourceManager* source_manager) {
  if (!loc.isValid()) {
    return "";
  }
  if (loc.isFileID()) {
    return CleanPath(
        tooling::getAbsolutePath(source_manager->getFilename(loc)));
  }
  SourceLocation eloc = source_manager->getExpansionLoc(loc);
  return GetLocationFilename(eloc, source_manager);
}

vector<string> GetCTUSourceFile(string source_file_path) {
  ifstream fp(source_file_path);
  vector<string> source_file_list;
  string line;
  while (getline(fp, line)) {
    int line_size = line.size() - 1;
    if (!line.empty() && line[line_size] == '\n') {
      line.erase(line_size);
    }
    source_file_list.emplace_back(line);
  }
  fp.close();
  return source_file_list;
}

string GetFilename(const Stmt* stmt, SourceManager* source_manager) {
  SourceLocation loc = stmt->getBeginLoc();
  return GetLocationFilename(loc, source_manager);
}

string GetFilename(const Decl* decl, SourceManager* source_manager) {
  SourceLocation loc = source_manager->getSpellingLoc(decl->getLocation());
  return GetLocationFilename(loc, source_manager);
}

int GetLocationLine(SourceLocation loc, SourceManager* source_manager) {
  if (!loc.isValid()) {
    return 0;
  }
  if (loc.isFileID()) {
    PresumedLoc ploc = source_manager->getPresumedLoc(loc);
    if (!ploc.isValid()) {
      return 0;
    }
    return ploc.getLine();
  }
  SourceLocation eloc = source_manager->getExpansionLoc(loc);
  return GetLocationLine(eloc, source_manager);
}

int GetLine(const Stmt* stmt, SourceManager* source_manager) {
  SourceLocation loc = stmt->getBeginLoc();
  return GetLocationLine(loc, source_manager);
}

int GetLine(const Decl* decl, SourceManager* source_manager) {
  SourceLocation loc = source_manager->getSpellingLoc(decl->getLocation());
  return GetLocationLine(loc, source_manager);
}

string GetRealLocation(SourceLocation loc, SourceManager* source_manager) {
  if (!loc.isValid()) {
    return "";
  }
  if (loc.isFileID()) {
    PresumedLoc ploc = source_manager->getPresumedLoc(loc);
    if (!ploc.isValid()) {
      return "";
    }
    return GenerateLoction(
        CleanPath(tooling::getAbsolutePath(source_manager->getFilename(loc))),
        ploc.getLine(), ploc.getColumn());
  }
  SourceLocation eloc = source_manager->getExpansionLoc(loc);
  return GetRealLocation(eloc, source_manager);
}

string GetRealFilename(SourceLocation loc, SourceManager* source_manager) {
  if (!loc.isValid()) {
    return "";
  }
  if (loc.isFileID()) {
    PresumedLoc ploc = source_manager->getPresumedLoc(loc);
    if (!ploc.isValid()) {
      return "";
    }
    return CleanPath(
        tooling::getAbsolutePath(source_manager->getFilename(loc)));
  }
  SourceLocation eloc = source_manager->getExpansionLoc(loc);
  return GetRealFilename(eloc, source_manager);
}

int GetRealLine(SourceLocation loc, SourceManager* source_manager) {
  if (!loc.isValid()) {
    return -1;
  }
  if (loc.isFileID()) {
    PresumedLoc ploc = source_manager->getPresumedLoc(loc);
    if (!ploc.isValid()) {
      return -1;
    }
    return ploc.getLine();
  }
  SourceLocation eloc = source_manager->getExpansionLoc(loc);
  return GetRealLine(eloc, source_manager);
}

string GetLocation(const Stmt* stmt, SourceManager* source_manager) {
  SourceLocation loc = stmt->getBeginLoc();
  return GetRealLocation(loc, source_manager);
}

bool IsInMacroExpansion(const Stmt* stmt, SourceManager* source_manager) {
  SourceLocation loc = stmt->getBeginLoc();
  if (!loc.isValid()) return false;
  return !loc.isFileID();
}

string GetLocation(const Decl* decl, SourceManager* source_manager) {
  SourceLocation loc = source_manager->getSpellingLoc(decl->getLocation());
  return GetRealLocation(loc, source_manager);
}

bool HasHeaderSuffix(const llvm::StringRef& filename) {
  return filename.endswith(".h") || filename.endswith(".hpp");
}

bool IsInSystemHeader(const Decl* decl, ASTContext* context) {
  FullSourceLoc location = context->getFullLoc(decl->getBeginLoc());
  if (location.isInvalid()) {
    return true;  // system predefiend types
  }
  if (location.isInSystemHeader()) {
    return true;
  }
  return false;
}

bool IsInSystemHeader(const Stmt* stmt, ASTContext* context) {
  FullSourceLoc location = context->getFullLoc(stmt->getBeginLoc());
  if (location.isInvalid()) {
    return true;  // system predefiend types
  }
  if (location.isInSystemHeader()) {
    return true;
  }
  return false;
}

EssentialTypeCategory GetEssentialTypeCategory(const Expr* expr,
                                               const ASTContext* context) {
  if (expr->getType()->isBooleanType()) {
    return kBoolean;
  }
  if (expr->getType()->isIntegerType()) {
    Expr::EvalResult aint;
    if (expr->EvaluateAsInt(aint, *context)) {
      if (aint.Val.getInt() == 1 || aint.Val.getInt() == 0) {
        return kBoolean;
      }
    }
  }
  if (expr->getType()->isCharType()) {
    return kCharater;
  }
  if (expr->getType()->isSignedIntegerType()) {
    return kSigned;
  }
  if (expr->getType()->isUnsignedIntegerType()) {
    return kUnsigned;
  }
  if (expr->getType()->isEnumeralType()) {
    return kEnum;
  }
  if (expr->getType()->isFloatingType()) {
    return kFloating;
  }
  return kUndefined;
}

void SplitArg(int* gflag_argc, int* libtooling_argc, int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-@@@") == 0) {
      *gflag_argc = i;
      *libtooling_argc = argc - i;
      argv[i] = argv[0];
      break;
    }
  }
}

bool isExternalDecl(const DeclaratorDecl* decl) {
  if (decl->getLinkageInternal() == NoLinkage ||
      decl->getLinkageInternal() == InternalLinkage ||
      decl->getLinkageInternal() == UniqueExternalLinkage) {
    return false;
  }
  return true;
}

string GetTokenFromSourceLoc(SourceManager* source_manager,
                             SourceLocation begin, SourceLocation end) {
  if (source_manager->getCharacterData(end) >
      source_manager->getCharacterData(begin)) {
    return std::string(source_manager->getCharacterData(begin),
                       source_manager->getCharacterData(end) -
                           source_manager->getCharacterData(begin) + 1);
  }
  // return an empty string if the input source locations are invalid
  return "";
}

bool isInHeader(const NamedDecl* decl, SourceManager* sourceManager) {
  std::string filename =
      misra::libtooling_utils::GetFilename(decl, sourceManager);
  unsigned long pos = filename.find_last_of(".");
  if (pos == string::npos) return false;
  std::string postfix = filename.substr(pos);
  return postfix == ".h" || postfix == ".hpp";
}

QualType* GetUnderlyingType(QualType* typeA, QualType* typeB,
                            ASTContext* context) {
  if (typeA->isNull() || typeB->isNull()) return nullptr;
  const QualType canonicalTypeA = typeA->getCanonicalType();
  const QualType canonicalTypeB = typeB->getCanonicalType();
  if (canonicalTypeA.isNull() || canonicalTypeB.isNull()) return nullptr;
  const Type* typePtrA = canonicalTypeA.getTypePtr();
  const Type* typePtrB = canonicalTypeB.getTypePtr();
  if ((typePtrA->isDependentType()) || (typePtrB->isDependentType()))
    return nullptr;

  uint64_t widthA = context->getTypeInfo(typePtrA).Width;
  uint64_t widthB = context->getTypeInfo(typePtrB).Width;

  // one of the types is floating, return the floating type
  if (typePtrA->isFloatingType() && !typePtrB->isFloatingType()) {
    return typeA;
  } else if (!typePtrA->isFloatingType() && typePtrB->isFloatingType()) {
    return typeB;
  }

  // types with inequal width, return the wider one
  if (widthA != widthB) {
    if (widthA > widthB) {
      return typeA;
    }
    return typeB;
  }

  // type with equal width
  if (widthA == widthB) {
    if (typePtrB->isUnsignedIntegerType()) {
      return typeB;
    }
    return typeA;
  }

  llvm_unreachable("unexpected types");
}

void GetUnderlyingTypeOfExpr(const Expr* expr, ASTContext* context,
                             QualType& output_type);

bool isLiteral(const Expr* expr) {
  return isa<IntegerLiteral>(expr) || isa<FloatingLiteral>(expr);
}

void GetUnderlyingTypeOfBinExpr(const BinaryOperator* binOp,
                                ASTContext* context, QualType& output_type) {
  const Expr* lhs = binOp->getLHS()->IgnoreParenImpCasts();
  const Expr* rhs = binOp->getRHS()->IgnoreParenImpCasts();

  QualType lhs_type;
  GetUnderlyingTypeOfExpr(lhs, context, lhs_type);
  QualType rhs_type;
  GetUnderlyingTypeOfExpr(rhs, context, rhs_type);

  if (isLiteral(lhs) && !isLiteral(rhs)) {
    lhs_type = rhs_type;
  } else if (!isLiteral(lhs) && isLiteral(rhs)) {
    rhs_type = lhs_type;
  }
  QualType* tmp_type = GetUnderlyingType(&lhs_type, &rhs_type, context);
  if (tmp_type != nullptr)
    output_type = *GetUnderlyingType(&lhs_type, &rhs_type, context);
}

void GetUnderlyingTypeOfUnaryExpr(const UnaryOperator* unaryOp,
                                  ASTContext* context, QualType& output_type) {
  GetUnderlyingTypeOfExpr(unaryOp->getSubExpr()->IgnoreParenImpCasts(), context,
                          output_type);
}

void GetUnderlyingTypeOfExpr(const Expr* expr, ASTContext* context,
                             QualType& output_type) {
  expr = expr->IgnoreParenImpCasts();
  if (isa<BinaryOperator>(expr)) {
    GetUnderlyingTypeOfBinExpr(dyn_cast<BinaryOperator>(expr), context,
                               output_type);
    return;
  } else if (isa<UnaryOperator>(expr)) {
    GetUnderlyingTypeOfUnaryExpr(dyn_cast<UnaryOperator>(expr), context,
                                 output_type);
    return;
  }
  output_type = expr->getType();
}

string GetLibFDNameOfCallExpr(const CallExpr* call_expr, ASTContext* context) {
  const FunctionDecl* callee_fd = call_expr->getDirectCallee();
  if (callee_fd == nullptr) {
    return "";
  }
  // The function should not be user defined.
  if (!IsInSystemHeader(callee_fd, context)) {
    return "";
  }
  return callee_fd->getNameAsString();
}

// The return name is formatted as 'parent_fd_name:decl_name'.
string GetCalleeArgNameWithParentFD(const CallExpr* call_expr, int arg_cnt) {
  const Decl* arg_decl =
      call_expr->getArg(arg_cnt)->getReferencedDeclOfCallee();
  if (!arg_decl) return "";
  string decl_name = dyn_cast<NamedDecl>(arg_decl)->getNameAsString();
  const DeclContext* parent = arg_decl->getParentFunctionOrMethod();
  if (parent) {
    if (isa<FunctionDecl>(parent)) {
      decl_name =
          cast<FunctionDecl>(parent)->getNameAsString() + ":" + decl_name;
    }
  }
  return decl_name;
}

/**
 * @brief create comparasion operation matcher
 * @param comp_enum choose which comparasion format will be used
 * @param bind_id   which string will be binded on comparison variable
 * @return comparasion operation matcher
 * @note following are expressions considered
 * 1. binary operator: a in `a < 10`
 * 2. overloaded binary operator: interator
 **/
const Matcher<Stmt> CreateComparasionMatcher(
    const ForConditionVarFormat comp_enum, const std::string bind_id) {
  // choose condition format for variables
  std::vector<StringRef> opt_name;
  if (comp_enum == AllCondFormat) {
    opt_name = {">=", ">", "<=", "<", "==", "!="};
  } else {
    if (comp_enum & BinaryEqual) {
      opt_name = {"==", "!="};
    }
    if (comp_enum & BinaryCompare) {
      opt_name = {">=", ">", "<=", "<"};
    }
  }

  // condition part (comparison)
  // match the declarations of condition variables and bind them to "cond_var"
  Matcher<Expr> cond_var =
      ignoringParenCasts(declRefExpr(to(varDecl().bind(bind_id))));

  // match variables in both operand of binary operator
  // e.g. a in `a == 10`, `10 <= a`; a, b in `a > b`
  auto both_cond_opd = eachOf(hasLHS(cond_var), hasRHS(cond_var));

  // match variables in comparison binary operator
  Matcher<Stmt> cond_bin_opt_matcher =
      binaryOperator(hasAnyOperatorName(opt_name), both_cond_opd);

  // match variables in comparison overloaded binary operator
  // e.g. iterator
  Matcher<Stmt> cond_cxx_opt_matcher =
      cxxOperatorCallExpr(hasAnyOperatorName(opt_name), both_cond_opd);

  // combine every condition matcher
  return eachOf(findAll(cond_bin_opt_matcher), findAll(cond_cxx_opt_matcher));
}

/**
 * @brief create assignment operation matcher
 * @param assign_enum  choose which assignment format will be used
 * @param find_id      which string will be found to match
 * @param decl_bind_id which string will be binded on comparison variable
 *declaration
 * @param ref_bind_id  which string will be binded on comparison expression
 * @return assignment operation matcher
 * @note following are expressions considered
 * 1. binary operator: a in `a += 10`
 * 2. unary operator: a in `a++`
 * 3. overloaded binary and unary operator: iterator
 * 4. function call: x in `f(&x)`, `f(*x)`, `f(**x)`
 * 5. non-const member function call: a in `a.f(10)`
 **/
const Matcher<Stmt> CreateAssignmentMatcher(
    const ForIncrementVarFormat assign_enum, const std::string find_id,
    const std::string decl_bind_id, const std::string ref_bind_id) {
  Matcher<BinaryOperator> binary_opt = anything();
  Matcher<CXXOperatorCallExpr> cxx_binary_opt = anything();
  if ((assign_enum & BinaryInDecrease) && (assign_enum & BinaryOtherOpt)) {
    binary_opt = anything();
    cxx_binary_opt = anything();
  } else if (assign_enum & BinaryInDecrease) {
    binary_opt = hasAnyOperatorName("+=", "-=");
    cxx_binary_opt = hasAnyOperatorName("+=", "-=");
  } else if (assign_enum & BinaryOtherOpt) {
    binary_opt = unless(hasAnyOperatorName("+=", "-="));
    cxx_binary_opt = unless(hasAnyOperatorName("+=", "-="));
  }

  // increment part (assignment)
  // match the declaration of increment variables which have been found in
  // condition part ("cond_var"), and bind it to "loop_counter"
  Matcher<Expr> inc_var = ignoringParenImpCasts(
      declRefExpr(to(varDecl(equalsBoundNode(find_id)).bind(decl_bind_id)))
          .bind(ref_bind_id));

  // match variables in assignment binary operator
  // e.g. i in `i += 1`, `i -= 1`, `i = j + 1`
  Matcher<Stmt> inc_bin_opt_matcher =
      binaryOperator(isAssignmentOperator(), hasLHS(inc_var), binary_opt);

  // match variables in assignment overloaded binary operator
  // e.g. iterator
  Matcher<Stmt> inc_bin_cxx_opt_matcher = cxxOperatorCallExpr(
      isAssignmentOperator(), hasLHS(inc_var), cxx_binary_opt);

  // match variables in assignment unary operator
  // e.g. i in `++i`, `i++`, `i--`
  Matcher<Stmt> inc_una_opt_matcher =
      unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(inc_var));

  // match variables in assignment overloaded unary operator
  // e.g. iterator
  Matcher<Stmt> inc_una_cxx_opt_matcher = cxxOperatorCallExpr(
      hasAnyOperatorName("++", "--"), hasUnaryOperand(inc_var));

  // match reference and pointer variables in function call
  // e.g. x in `f(&x)`, `f(*x)`, `f(**x)`
  Matcher<Stmt> inc_call_exp_matcher = allOf(
      callExpr(forEachArgumentWithParamType(
          anyOf(inc_var, unaryOperator(hasAnyOperatorName("&"),
                                       hasUnaryOperand(inc_var))),
          qualType(anyOf(isAnyPointer(), hasCanonicalType(referenceType()))))),
      unless(cxxOperatorCallExpr()));

  // match object itself in non-const member function call
  // e.g. a in `a.f(x)`
  Matcher<Stmt> inc_cxx_mem_call_matcher = cxxMemberCallExpr(
      on(inc_var), hasDeclaration(cxxMethodDecl(unless(isConst()))));

  // choose increment format for variables
  if (!(assign_enum & BinaryAssign)) {
    inc_bin_opt_matcher = unless(anything());
    inc_bin_cxx_opt_matcher = unless(anything());
  }
  if (!(assign_enum & UnaryInDecrease)) {
    inc_una_opt_matcher = unless(anything());
    inc_una_cxx_opt_matcher = unless(anything());
  }
  if (!(assign_enum & FunctionChange)) {
    inc_call_exp_matcher = unless(anything());
    inc_cxx_mem_call_matcher = unless(anything());
  }

  // combine every increment matcher
  return eachOf(findAll(inc_bin_opt_matcher), findAll(inc_bin_cxx_opt_matcher),
                findAll(inc_una_opt_matcher), findAll(inc_una_cxx_opt_matcher),
                findAll(inc_call_exp_matcher),
                findAll(inc_cxx_mem_call_matcher));
}

/**
 * @brief create condition and increment matcher to find loop-counter
 * @param  cond_enum choose which format in condition part will be used
 * @param  inc_enum  choose which format in increment part will be used
 * @return loop-counter matcher
 **/
const Matcher<ForStmt> CreateLoopCounterMatcher(
    ForConditionVarFormat cond_enum, ForIncrementVarFormat inc_enum) {
  auto cond_matcher = CreateComparasionMatcher(cond_enum);
  auto inc_matcher = CreateAssignmentMatcher(inc_enum);

  return allOf(hasCondition(cond_matcher), hasIncrement(inc_matcher));
}

const Matcher<NamedDecl> OperatorOverloading() {
  return functionDecl(hasAnyName(
      "operator+", "operator-", "operator*", "operator/", "operator%",
      "operator^", "operator&", "operator|", "operator~", "operator!",
      "operator=", "operator<", "operator>",
      "operator+=", "operator-=", "operator*=", "operator/=", "operator%=",
      "operator^=", "operator&=", "operator|=", "operator<<", "operator>>",
      "operator<<=", "operator>>=", "operator==", "operator!=", "operator<=",
      "operator>=", "operator&&", "operator||", "operator++", "operator--",
      "operator,", "operator->*", "operator->", "operator()", "operator[]"));
}

bool GetParamInfoIsOutput(FuncInfo2ParamInfos func_info_2_param_infos,
                          ParamInfo& param_info) {
  if (!param_info.is_not_null || param_info.is_output ||
      param_info.arg_checked || !param_info.is_arg)
    return param_info.is_output;
  param_info.arg_checked = true;  // avoid inf recursion
  auto const& find_by_function_id =
      [&param_info](const FuncInfo2ParamInfos::value_type& kv_pair) {
        return kv_pair.first.id == param_info.arg_func_id;
      };
  auto const find_result =
      find_if(func_info_2_param_infos.begin(), func_info_2_param_infos.end(),
              find_by_function_id);
  if (find_result == func_info_2_param_infos.end()) return false;
  return GetParamInfoIsOutput(func_info_2_param_infos,
                              find_result->second[param_info.arg_pos]);
}

void UpdateFuncInfo2ParamInfos(FuncInfo2ParamInfos& func_info_2_param_infos) {
  for (auto& it : func_info_2_param_infos)
    for (ParamInfo& param_info : it.second)
      param_info.is_output =
          GetParamInfoIsOutput(func_info_2_param_infos, param_info);
}

void AddFuncOutputParamMatchers(
    ast_matchers::MatchFinder* finder,
    ast_matchers::MatchFinder::MatchCallback* action) {
  finder->addMatcher(
      functionDecl(unless(isExpansionInSystemHeader())).bind("func"), action);
  finder->addMatcher(
      binaryOperator(
          unless(isExpansionInSystemHeader()), isAssignmentOperator(),
          hasLHS(anyOf(unaryOperator(hasOperatorName("*"),
                                     hasUnaryOperand(hasDescendant(
                                         declRefExpr().bind("binary_lhs"))))
                           .bind("deref_op"),
                       declRefExpr().bind("binary_lhs"))),
          hasAncestor(functionDecl().bind("func"))),
      action);
  finder->addMatcher(
      unaryOperator(unless(isExpansionInSystemHeader()),
                    anyOf(hasOperatorName("++"), hasOperatorName("--")),
                    hasDescendant(declRefExpr().bind("unary_lhs")),
                    hasAncestor(functionDecl().bind("func"))),
      action);
  finder->addMatcher(callExpr(unless(isExpansionInSystemHeader()),
                              unless(cxxOperatorCallExpr(hasOperatorName("="))),
                              unless(cxxMemberCallExpr()),
                              hasAnyArgument(declRefExpr().bind("arg")),
                              hasAncestor(functionDecl().bind("func")))
                         .bind("call"),
                     action);
  finder->addMatcher(
      cxxMemberCallExpr(
          unless(isExpansionInSystemHeader()),
          unless(has(memberExpr(member(
              anyOf(hasName("begin"),
                    hasName("size"),    // add names of methods that will not
                    hasName("length"),  // modifiy object caller
                    hasName("end")))))),
          has(memberExpr(has(declRefExpr().bind("object")))),
          hasAncestor(functionDecl().bind("func")))
          .bind("member_call"),
      action);
}

void FuncOutputParamCallback(
    const ast_matchers::MatchFinder::MatchResult& result,
    FuncInfo2ParamInfos& func_info_2_param_infos) {
  FunctionDecl const* const func = result.Nodes.getNodeAs<FunctionDecl>("func");
  DeclRefExpr const* const binary_lhs =
      result.Nodes.getNodeAs<DeclRefExpr>("binary_lhs");
  UnaryOperator const* const deref_op =
      result.Nodes.getNodeAs<UnaryOperator>("deref_op");
  DeclRefExpr const* const unary_lhs =
      result.Nodes.getNodeAs<DeclRefExpr>("unary_lhs");
  DeclRefExpr const* const arg = result.Nodes.getNodeAs<DeclRefExpr>("arg");
  CallExpr const* const call = result.Nodes.getNodeAs<CallExpr>("call");
  DeclRefExpr const* const object =
      result.Nodes.getNodeAs<DeclRefExpr>("object");
  CXXMemberCallExpr const* const member_call =
      result.Nodes.getNodeAs<CXXMemberCallExpr>("member_call");
  auto* SM = result.SourceManager;
  if (!func || func->isImplicit()) return;
  FuncInfo func_info = {
      .id = func->getNameInfo().getLoc().getHashValue(),
      .path = misra::libtooling_utils::GetFilename(func, SM),
      .line_number = misra::libtooling_utils::GetLine(func, SM),
      .is_return_void = func->getReturnType()->isVoidType(),
  };
  vector<ParamInfo>& param_infos = func_info_2_param_infos[func_info];
  if (param_infos.size() == 0 && func->getNumParams() > func->isVariadic()) {
    for (unsigned int i = 0; i < func->getNumParams() - func->isVariadic();
         ++i) {
      ParmVarDecl const* const param = func->getParamDecl(i);
      QualType const& type = param->getType();
      if (type.isNull()) {
        param_infos.push_back({
            .name = param->getNameAsString(),
            .is_not_null = false,
        });
        continue;
      }
      auto non_ref_type =
          type->isReferenceType() ? type.getNonReferenceType() : type;
      // for `getTypeInfo(const Type *T)`, T should not be dependent.
      if (non_ref_type->isDependentType()) return;
      // see `const ASTRecordLayout& ASTContext::getASTRecordLayout(const
      // RecordDecl *D) const`, add the following check to avoid assertion
      // failure
      if (non_ref_type->isRecordType()) {
        auto RD = non_ref_type->getAsRecordDecl();
        RD = RD->getDefinition();
        if (!RD) return;
        if (RD->isInvalidDecl()) return;
        if (!RD->isCompleteDefinition()) return;
      }
      param_infos.push_back({
          .name = param->getNameAsString(),
          .is_not_null = true,
          .is_pointer_ty = type->isPointerType(),
          .can_be_output =
              (type->isReferenceType() && !type.isConstQualified()) ||
              type->isPointerType(),
          .is_output = false,
          .is_arg = false,
          .arg_func_id = 0,
          .arg_pos = 0,
          .arg_checked = false,
          .size_bits = param->getASTContext().getTypeInfo(non_ref_type).Width,
          .is_reference = type->isReferenceType(),
      });
    }
  }
  if (!binary_lhs && !unary_lhs && !arg && !object) return;
  DeclRefExpr const* const param = [&]() {
    if (binary_lhs) return binary_lhs;
    if (unary_lhs) return unary_lhs;
    if (arg) return arg;
    return object;
  }();
  string const& param_name = param->getNameInfo().getName().getAsString();
  auto const& is_param_info = [&param_name](ParamInfo const& param_info) {
    return param_info.name == param_name;
  };
  vector<ParamInfo>::iterator const& find_result =
      std::find_if(param_infos.begin(), param_infos.end(), is_param_info);
  if (find_result != param_infos.end() && find_result->can_be_output) {
    if (binary_lhs) {
      if (!deref_op) {  // reference
        find_result->is_output = !find_result->is_pointer_ty;
      } else {  // pointer
        find_result->is_output = find_result->is_pointer_ty;
      }
    } else if (unary_lhs) {
      find_result->is_output = true;
    } else if (arg) {
      find_result->is_arg = true;
      auto callee = call->getDirectCallee();
      if (!callee) return;
      find_result->arg_func_id = callee->getNameInfo().getLoc().getHashValue();
      unsigned int arg_pos = 0;
      for (const auto& call_arg : call->arguments()) {
        if (call_arg == arg) break;
        ++arg_pos;
      }
      find_result->arg_pos = arg_pos;
    } else if (object) {
      find_result->is_output = !member_call->getMethodDecl()->isConst();
    }
  }
}

bool isDependent(const Expr* expr) {
  return expr->isTypeDependent() || expr->isValueDependent() ||
         expr->isInstantiationDependent();
}

bool isForwardingReference(QualType qt, unsigned inner_depth) {
  //   A forwarding reference is an rvalue reference to a cv-unqualified
  //   template parameter that does not represent a template parameter of a
  //   class template.
  qt = qt.getNonPackExpansionType();
  if (const RValueReferenceType* parm_ref = qt->getAs<RValueReferenceType>()) {
    if (parm_ref->getPointeeType().getQualifiers()) return false;
    const TemplateTypeParmType* type_parm =
        parm_ref->getPointeeType()->getAs<TemplateTypeParmType>();
    return type_parm && type_parm->getDepth() == inner_depth;
  }
  return false;
}

string getQualifiedName(const UnresolvedLookupExpr* ule) {
  string qualifiedName{};
  if (ule) {
    if (NestedNameSpecifier* nns = ule->getQualifier()) {
      llvm::raw_string_ostream os(qualifiedName);
      nns->print(os, PrintingPolicy(LangOptions()));
    }
    qualifiedName += ule->getName().getAsString();
  }
  return qualifiedName;
}

bool isPersistentSideEffect(const VarDecl* vd, const DeclRefExpr* dre) {
  return vd && (vd->isFileVarDecl() || (dyn_cast<ParmVarDecl>(dre->getDecl()) &&
                                        (vd->getType()->isPointerType() ||
                                         vd->getType()->isReferenceType())));
}

void ConstCallExprVisitor::Visit(const Stmt* Node) {
  bool dummy = false;
  Visit_(Node, &dummy);
}

void ConstCallExprVisitor::Visit_(const Stmt* Node, bool* HasCallExprChild) {
  if (hasPersistentSideEffects) {
    return;
  }
  // Call appropriate visit method based on the node type
  ConstStmtVisitor<ConstCallExprVisitor>::Visit(Node);
  // Traverse child statements
  bool hasCallExprChild = false;
  if (dyn_cast<CallExpr>(Node)) {
    hasCallExprChild = true;
  }
  const Expr* expr = dyn_cast<Expr>(Node);
  if (!expr || !expr->HasSideEffects(*ctx_)) {
    return;
  }
  for (const auto* Child : Node->children()) {
    if (Child) {
      bool isCallExpr = false;
      Visit_(Child, &isCallExpr);
      hasCallExprChild = hasCallExprChild || isCallExpr;
    }
  }
  if (!hasCallExprChild) {
    hasPersistentSideEffects = true;
  }
  *HasCallExprChild = hasCallExprChild;
}

void ConstCallExprVisitor::VisitCallExpr(const CallExpr* Call) {
  hasCallExpr = true;
  if (!Call->getDirectCallee()) {
    return;
  }
  const FunctionDecl* Callee = Call->getDirectCallee()->getDefinition();
  if (!Callee) return;

  hasDirectCall = true;
  libtooling_utils::ASTVisitor visitor;
  visitor.TraverseDecl(const_cast<FunctionDecl*>(Callee));
  for (const VarDecl* vd : visitor.getVarDecls()) {
    if (vd->getStorageClass() == SC_Static) {
      hasPersistentSideEffects = true;
      return;
    }
  }
  for (const BinaryOperator* bo : visitor.getBinaryOps()) {
    if (bo->isAssignmentOp()) {
      if (const DeclRefExpr* lhs = dyn_cast<DeclRefExpr>(bo->getLHS())) {
        const VarDecl* lhs_var = dyn_cast<VarDecl>(lhs->getDecl());
        if (isPersistentSideEffect(lhs_var, lhs)) {
          hasPersistentSideEffects = true;
          return;
        }
      } else if (dyn_cast<MemberExpr>(bo->getLHS())) {
        // _class_member = 1;
        hasPersistentSideEffects = true;
        return;
      } else if (const UnaryOperator* lhs =
                     dyn_cast<UnaryOperator>(bo->getLHS()->IgnoreParens())) {
        // (*parameter) = 1;
        const DeclRefExpr* defered_expr =
            dyn_cast<DeclRefExpr>(lhs->getSubExpr()->IgnoreImpCasts());
        const VarDecl* defered_expr_var =
            dyn_cast<VarDecl>(defered_expr->getDecl());
        if (isPersistentSideEffect(defered_expr_var, defered_expr)) {
          hasPersistentSideEffects = true;
          return;
        }
      }
    }
  }
}

string GetExprName(const Expr* expr, SourceManager* sm, ASTContext* context) {
  CharSourceRange char_range = Lexer::makeFileCharRange(
      CharSourceRange::getTokenRange(expr->getSourceRange()), *sm,
      context->getLangOpts());
  string name =
      Lexer::getSourceText(char_range, *sm, context->getLangOpts()).str();
  return name;
}

// If aggressive_mode is false, only report when the function is sure to have
// persistent side effects.
// If aggressive_mode is true, only not report when the function is sure not to
// have persistent side effects.
bool ConstCallExprVisitor::ShouldReport(bool aggressive_mode) {
  if (hasPersistentSideEffects) {
    return true;
  }
  if (!aggressive_mode) {
    return hasCallExpr && hasDirectCall && hasPersistentSideEffects;
  }
  return !(hasCallExpr && hasDirectCall && !hasPersistentSideEffects);
}

}  // namespace libtooling_utils
}  // namespace misra
