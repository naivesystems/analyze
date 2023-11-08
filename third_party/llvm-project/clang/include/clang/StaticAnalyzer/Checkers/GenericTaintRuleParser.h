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

//=== GenericTaintRuleParser.h - Parse taint propagation rules --*- C++ -*-===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Checkers/Taint.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "llvm/Support/YAMLTraits.h"

namespace yamlparser {
using namespace clang;
using namespace llvm;
using namespace ento;

using ArgIdxTy = int;
using ArgVecTy = llvm::SmallVector<ArgIdxTy, 2>;

/// Denotes the return value.
constexpr ArgIdxTy ReturnValueIndex{-1};

/// ArgSet is used to describe arguments relevant for taint detection or
/// taint application. A discrete set of argument indexes and a variadic
/// argument list signified by a starting index are supported.
struct ArgSet {
  ArgSet() = default;
  ArgSet(ArgVecTy &&DiscreteArgs, Optional<ArgIdxTy> VariadicIndex = None)
      : DiscreteArgs(std::move(DiscreteArgs)),
        VariadicIndex(std::move(VariadicIndex)) {}

  bool contains(ArgIdxTy ArgIdx) const {
    if (llvm::is_contained(DiscreteArgs, ArgIdx))
      return true;

    return VariadicIndex && ArgIdx >= *VariadicIndex;
  }

  bool isEmpty() const { return DiscreteArgs.empty() && !VariadicIndex; }

  ArgVecTy DiscreteArgs;
  Optional<ArgIdxTy> VariadicIndex;
};

/// A struct used to specify taint propagation rules for a function.
///
/// If any of the possible taint source arguments is tainted, all of the
/// destination arguments should also be tainted. If ReturnValueIndex is added
/// to the dst list, the return value will be tainted.
struct GenericTaintRule {
  /// Arguments which are taints sinks and should be checked, and a report
  /// should be emitted if taint reaches these.
  ArgSet SinkArgs;
  /// Arguments which should be sanitized on function return.
  ArgSet FilterArgs;
  /// Arguments which can participate in taint propagationa. If any of the
  /// arguments in PropSrcArgs is tainted, all arguments in  PropDstArgs should
  /// be tainted.
  ArgSet PropSrcArgs;
  ArgSet PropDstArgs;

  /// A message that explains why the call is sensitive to taint.
  Optional<StringRef> SinkMsg;

  GenericTaintRule() = default;

  GenericTaintRule(ArgSet &&Sink, ArgSet &&Filter, ArgSet &&Src, ArgSet &&Dst,
                   Optional<StringRef> SinkMsg = None)
      : SinkArgs(std::move(Sink)), FilterArgs(std::move(Filter)),
        PropSrcArgs(std::move(Src)), PropDstArgs(std::move(Dst)),
        SinkMsg(SinkMsg) {}

public:
  /// Make a rule that reports a warning if taint reaches any of \p FilterArgs
  /// arguments.
  static GenericTaintRule Sink(ArgSet &&SinkArgs,
                               Optional<StringRef> Msg = None) {
    return {std::move(SinkArgs), {}, {}, {}, Msg};
  }

  /// Make a rule that sanitizes all FilterArgs arguments.
  static GenericTaintRule Filter(ArgSet &&FilterArgs) {
    return {{}, std::move(FilterArgs), {}, {}};
  }

  /// Make a rule that unconditionally taints all Args.
  /// If Func is provided, it must also return true for taint to propagate.
  static GenericTaintRule Source(ArgSet &&SourceArgs) {
    return {{}, {}, {}, std::move(SourceArgs)};
  }

  /// Make a rule that taints all PropDstArgs if any of PropSrcArgs is tainted.
  static GenericTaintRule Prop(ArgSet &&SrcArgs, ArgSet &&DstArgs) {
    return {{}, {}, std::move(SrcArgs), std::move(DstArgs)};
  }

  /// Make a rule that taints all PropDstArgs if any of PropSrcArgs is tainted.
  static GenericTaintRule SinkProp(ArgSet &&SinkArgs, ArgSet &&SrcArgs,
                                   ArgSet &&DstArgs,
                                   Optional<StringRef> Msg = None) {
    return {
        std::move(SinkArgs), {}, std::move(SrcArgs), std::move(DstArgs), Msg};
  }
};

/// Used to parse the configuration file.
struct TaintConfiguration {
  using NameScopeArgs = std::tuple<std::string, std::string, ArgVecTy>;
  enum class VariadicType { None, Src, Dst };

  struct Common {
    std::string Name;
    std::string Scope;
  };

  struct Sink : Common {
    ArgVecTy SinkArgs;
  };

  struct Filter : Common {
    ArgVecTy FilterArgs;
  };

  struct Propagation : Common {
    ArgVecTy SrcArgs;
    ArgVecTy DstArgs;
    VariadicType VarType;
    ArgIdxTy VarIndex;
  };

  std::vector<Propagation> Propagations;
  std::vector<Filter> Filters;
  std::vector<Sink> Sinks;

  TaintConfiguration() = default;
  TaintConfiguration(const TaintConfiguration &) = default;
  TaintConfiguration(TaintConfiguration &&) = default;
  TaintConfiguration &operator=(const TaintConfiguration &) = default;
  TaintConfiguration &operator=(TaintConfiguration &&) = default;
};

struct GenericTaintRuleParser {
  GenericTaintRuleParser(CheckerManager &Mgr) : Mgr(Mgr) {}
  /// Container type used to gather call identification objects grouped into
  /// pairs with their corresponding taint rules. It is temporary as it is used
  /// to finally initialize RuleLookupTy, which is considered to be immutable.
  using RulesContTy = std::vector<std::pair<CallDescription, GenericTaintRule>>;
  template <typename Checker>
  RulesContTy parseConfiguration(const std::string &Option,
                                 TaintConfiguration &&Config) const;

private:
  using NamePartsTy = llvm::SmallVector<SmallString<32>, 2>;

  /// Validate part of the configuration, which contains a list of argument
  /// indexes.
  template <typename Checker>
  void validateArgVector(const std::string &Option, const ArgVecTy &Args) const;

  template <typename Config> static NamePartsTy parseNameParts(const Config &C);

  // Takes the config and creates a CallDescription for it and associates a Rule
  // with that.
  template <typename Config>
  static void consumeRulesFromConfig(const Config &C, GenericTaintRule &&Rule,
                                     RulesContTy &Rules);

  template <typename Checker>
  void parseConfig(const std::string &Option, TaintConfiguration::Sink &&P,
                   RulesContTy &Rules) const;
  template <typename Checker>
  void parseConfig(const std::string &Option, TaintConfiguration::Filter &&P,
                   RulesContTy &Rules) const;
  template <typename Checker>
  void parseConfig(const std::string &Option,
                   TaintConfiguration::Propagation &&P,
                   RulesContTy &Rules) const;

  CheckerManager &Mgr;
};

// implementation
template <typename Checker>
void GenericTaintRuleParser::validateArgVector(const std::string &Option,
                                               const ArgVecTy &Args) const {
  for (ArgIdxTy Arg : Args) {
    if (Arg < ReturnValueIndex) {
      Mgr.reportInvalidCheckerOptionValue(
          Mgr.getChecker<Checker>(), Option,
          "an argument number for propagation rules greater or equal to -1");
    }
  }
}

template <typename Config>
GenericTaintRuleParser::NamePartsTy
GenericTaintRuleParser::parseNameParts(const Config &C) {
  NamePartsTy NameParts;
  if (!C.Scope.empty()) {
    // If the Scope argument contains multiple "::" parts, those are considered
    // namespace identifiers.
    llvm::SmallVector<StringRef, 2> NSParts;
    StringRef{C.Scope}.split(NSParts, "::", /*MaxSplit*/ -1,
                             /*KeepEmpty*/ false);
    NameParts.append(NSParts.begin(), NSParts.end());
  }
  NameParts.emplace_back(C.Name);
  return NameParts;
}

template <typename Config>
void GenericTaintRuleParser::consumeRulesFromConfig(const Config &C,
                                                    GenericTaintRule &&Rule,
                                                    RulesContTy &Rules) {
  NamePartsTy NameParts = parseNameParts(C);
  llvm::SmallVector<const char *, 2> CallDescParts{NameParts.size()};
  llvm::transform(NameParts, CallDescParts.begin(),
                  [](SmallString<32> &S) { return S.c_str(); });
  Rules.emplace_back(CallDescription(CallDescParts), std::move(Rule));
}

template <typename Checker>
void GenericTaintRuleParser::parseConfig(const std::string &Option,
                                         TaintConfiguration::Sink &&S,
                                         RulesContTy &Rules) const {
  validateArgVector<Checker>(Option, S.SinkArgs);
  consumeRulesFromConfig(S, GenericTaintRule::Sink(std::move(S.SinkArgs)),
                         Rules);
}

template <typename Checker>
void GenericTaintRuleParser::parseConfig(const std::string &Option,
                                         TaintConfiguration::Filter &&S,
                                         RulesContTy &Rules) const {
  validateArgVector<Checker>(Option, S.FilterArgs);
  consumeRulesFromConfig(S, GenericTaintRule::Filter(std::move(S.FilterArgs)),
                         Rules);
}

template <typename Checker>
void GenericTaintRuleParser::parseConfig(const std::string &Option,
                                         TaintConfiguration::Propagation &&P,
                                         RulesContTy &Rules) const {
  validateArgVector<Checker>(Option, P.SrcArgs);
  validateArgVector<Checker>(Option, P.DstArgs);
  bool IsSrcVariadic = P.VarType == TaintConfiguration::VariadicType::Src;
  bool IsDstVariadic = P.VarType == TaintConfiguration::VariadicType::Dst;
  Optional<ArgIdxTy> JustVarIndex = P.VarIndex;

  ArgSet SrcDesc(std::move(P.SrcArgs), IsSrcVariadic ? JustVarIndex : None);
  ArgSet DstDesc(std::move(P.DstArgs), IsDstVariadic ? JustVarIndex : None);

  consumeRulesFromConfig(
      P, GenericTaintRule::Prop(std::move(SrcDesc), std::move(DstDesc)), Rules);
}

template <typename Checker>
GenericTaintRuleParser::RulesContTy
GenericTaintRuleParser::parseConfiguration(const std::string &Option,
                                           TaintConfiguration &&Config) const {

  RulesContTy Rules;

  for (auto &F : Config.Filters)
    parseConfig<Checker>(Option, std::move(F), Rules);

  for (auto &S : Config.Sinks)
    parseConfig<Checker>(Option, std::move(S), Rules);

  for (auto &P : Config.Propagations)
    parseConfig<Checker>(Option, std::move(P), Rules);

  return Rules;
}
} // namespace yamlparser

/// YAML serialization mapping.
LLVM_YAML_IS_SEQUENCE_VECTOR(yamlparser::TaintConfiguration::Sink)
LLVM_YAML_IS_SEQUENCE_VECTOR(yamlparser::TaintConfiguration::Filter)
LLVM_YAML_IS_SEQUENCE_VECTOR(yamlparser::TaintConfiguration::Propagation)

namespace llvm {
namespace yaml {
using yamlparser::TaintConfiguration;

template <> struct MappingTraits<TaintConfiguration> {
  static void mapping(IO &IO, TaintConfiguration &Config) {
    IO.mapOptional("Propagations", Config.Propagations);
    IO.mapOptional("Filters", Config.Filters);
    IO.mapOptional("Sinks", Config.Sinks);
  }
};

template <> struct MappingTraits<TaintConfiguration::Sink> {
  static void mapping(IO &IO, TaintConfiguration::Sink &Sink) {
    IO.mapRequired("Name", Sink.Name);
    IO.mapOptional("Scope", Sink.Scope);
    IO.mapRequired("Args", Sink.SinkArgs);
  }
};

template <> struct MappingTraits<TaintConfiguration::Filter> {
  static void mapping(IO &IO, TaintConfiguration::Filter &Filter) {
    IO.mapRequired("Name", Filter.Name);
    IO.mapOptional("Scope", Filter.Scope);
    IO.mapRequired("Args", Filter.FilterArgs);
  }
};

template <> struct MappingTraits<TaintConfiguration::Propagation> {
  static void mapping(IO &IO, TaintConfiguration::Propagation &Propagation) {
    IO.mapRequired("Name", Propagation.Name);
    IO.mapOptional("Scope", Propagation.Scope);
    IO.mapOptional("SrcArgs", Propagation.SrcArgs);
    IO.mapOptional("DstArgs", Propagation.DstArgs);
    IO.mapOptional("VariadicType", Propagation.VarType);
    IO.mapOptional("VariadicIndex", Propagation.VarIndex);
  }
};

template <> struct ScalarEnumerationTraits<TaintConfiguration::VariadicType> {
  static void enumeration(IO &IO, TaintConfiguration::VariadicType &Value) {
    IO.enumCase(Value, "None", TaintConfiguration::VariadicType::None);
    IO.enumCase(Value, "Src", TaintConfiguration::VariadicType::Src);
    IO.enumCase(Value, "Dst", TaintConfiguration::VariadicType::Dst);
  }
};
} // namespace yaml
} // namespace llvm
