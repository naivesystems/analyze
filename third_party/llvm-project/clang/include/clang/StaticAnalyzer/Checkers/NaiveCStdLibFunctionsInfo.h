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

//=== NaiveCStdLibFunctionsInfo.h -------------------------------*- C++ -*-===//
//
// This header provides common information on C standard library functions
// lists.
//
// Functions in stdio.h and string.h are supported currently.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"

using namespace clang;
using namespace ento;
using namespace std::placeholders;

namespace naive {

using ArgIdxTy = int;
using ArgVecTy = llvm::SmallVector<ArgIdxTy, 2>;
constexpr ArgIdxTy ReturnValueIndex{-1};

class ArgSet {
public:
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

private:
  ArgVecTy DiscreteArgs;
  Optional<ArgIdxTy> VariadicIndex;
};

const CallDescriptionSet PathFunctions = {{"realpath", 2},
                                          {"canonicalize_file_name", 1}};

const CallDescriptionMap<std::pair<ArgSet, ArgSet>> FuncArgsMayReadOrWrite = {
    {{CDF_MaybeBuiltin, "atof", 1}, {/*R*/ {{0}}, /*W*/ {}}},
    {{CDF_MaybeBuiltin, "atoi", 1}, {/*R*/ {{0}}, /*W*/ {}}},
    {{CDF_MaybeBuiltin, "atol", 1}, {/*R*/ {{0}}, /*W*/ {}}},
    {{CDF_MaybeBuiltin, "clearerr", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fclose", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fdopen", 2}, {/*R*/ {{1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "feof", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "ferror", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fflush", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fgetc", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fgetpos", 2}, {/*R*/ {{0}}, /*W*/ {{0, 1}}}},
    {{CDF_MaybeBuiltin, "fgets", 3}, {/*R*/ {{2}}, /*W*/ {{0, 2}}}},
    {{CDF_MaybeBuiltin, "fileno", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fopen", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "fprintf"}, {/*R*/ {{0, 1}, 2}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fputc", 2}, {/*R*/ {{1}}, /*W*/ {{1}}}},
    {{CDF_MaybeBuiltin, "fputs", 2}, {/*R*/ {{0, 1}}, /*W*/ {{1}}}},
    {{CDF_MaybeBuiltin, "fread", 4}, {/*R*/ {{3}}, /*W*/ {{0, 3}}}},
    {{CDF_MaybeBuiltin, "free", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "freopen", 3}, {/*R*/ {{0, 1, 2}}, /*W*/ {{2}}}},
    {{CDF_MaybeBuiltin, "fscanf"}, {/*R*/ {{0, 1}}, /*W*/ {{0}, 2}}},
    {{CDF_MaybeBuiltin, "fseek", 3}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fsetpos", 2}, {/*R*/ {{0, 1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "ftell", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "fwrite", 4}, {/*R*/ {{0, 3}}, /*W*/ {{3}}}},
    {{CDF_MaybeBuiltin, "getc", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "getchar", 0}, {/*R*/ {{}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "gets", 1}, {/*R*/ {{}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "memchr", 3}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "memcmp", 3}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "memcpy", 3}, {/*R*/ {{1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "memmove", 3}, {/*R*/ {{1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "memset", 3}, {/*R*/ {{}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "perror", 1}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "printf"}, {/*R*/ {{0}, 1}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "putc", 2}, {/*R*/ {{1}}, /*W*/ {{1}}}},
    {{CDF_MaybeBuiltin, "putchar", 1}, {/*R*/ {{}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "puts", 1}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "qsort", 4}, {/*R*/ {{0, 3}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "read", 3}, {/*R*/ {}, /*W*/ {{1}}}}, //*
    {{CDF_MaybeBuiltin, "realloc", 2}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "remove", 1}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "rename", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "rewind", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "scanf"}, {/*R*/ {{0}}, /*W*/ {{}, 1}}},
    {{CDF_MaybeBuiltin, "setbuf", 2}, {/*R*/ {{0}}, /*W*/ {{0, 1}}}},
    {{CDF_MaybeBuiltin, "setvbuf", 4}, {/*R*/ {{0}}, /*W*/ {{0, 1}}}},
    {{CDF_MaybeBuiltin, "signal", 2}, {/*R*/ {{1}}, /*W*/ {}}},
    {{CDF_MaybeBuiltin, "snprintf"}, {/*R*/ {{2}, 3}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "sprintf"}, {/*R*/ {{1}, 2}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "sscanf"}, {/*R*/ {{0, 1}}, /*W*/ {{}, 2}}},
    {{CDF_MaybeBuiltin, "strcat", 2}, {/*R*/ {{0, 1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "strchr", 2}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strcmp", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strcoll", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strcpy", 2}, {/*R*/ {{1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "strcspn", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strerror", 1}, {/*R*/ {{}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strlen", 1}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strncat", 3}, {/*R*/ {{0, 1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "strncmp", 3}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strncpy", 3}, {/*R*/ {{1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "strnlen", 2}, {/*R*/ {{0}}, /*W*/ {}}}, //*
    {{CDF_MaybeBuiltin, "strpbrk", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strrchr", 2}, {/*R*/ {{0}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strspn", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strstr", 2}, {/*R*/ {{0, 1}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "strtok_r", 3}, {/*R*/ {{0, 1, 2}}, /*W*/ {{0, 2}}}},
    {{CDF_MaybeBuiltin, "strtok", 2}, {/*R*/ {{0, 1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "strxfrm", 3}, {/*R*/ {{1}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "time", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "time64", 1}, {/*R*/ {{0}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "tmpfile", 0}, {/*R*/ {{}}, /*W*/ {{}}}},
    {{CDF_MaybeBuiltin, "tmpnam", 1}, {/*R*/ {{}}, /*W*/ {{0}}}},
    {{CDF_MaybeBuiltin, "ungetc", 2}, {/*R*/ {{0, 1}}, /*W*/ {{1}}}},
    {{CDF_MaybeBuiltin, "vfprintf", 3}, {/*R*/ {{0, 1, 2}}, /*W*/ {{0, 2}}}},
    {{CDF_MaybeBuiltin, "vfscanf", 3}, {/*R*/ {{0, 1, 2}}, /*W*/ {{0, 2}}}},
    {{CDF_MaybeBuiltin, "vfwscanf", 3}, {/*R*/ {{0, 1, 2}}, /*W*/ {{0, 2}}}},
    {{CDF_MaybeBuiltin, "vprintf", 2}, {/*R*/ {{0, 1}}, /*W*/ {{1}}}},
    {{CDF_MaybeBuiltin, "vscanf", 2}, {/*R*/ {{0, 1}}, /*W*/ {{1}}}},
    {{CDF_MaybeBuiltin, "vsnprintf", 4}, {/*R*/ {{2, 3}}, /*W*/ {{0, 3}}}},
    {{CDF_MaybeBuiltin, "vsprintf", 3}, {/*R*/ {{1, 2}}, /*W*/ {{0, 2}}}},
    {{CDF_MaybeBuiltin, "vsscanf", 3}, {/*R*/ {{0, 1, 2}}, /*W*/ {{2}}}},
    {{CDF_MaybeBuiltin, "write", 3}, {/*R*/ {{1}}, /*W*/ {}}}, //*
    // TODO: Support more functions
    // Functions marked with * is not in standard library
};

const CallDescriptionSet ReturnValueNeedCheckFunctions = {
    {CDF_MaybeBuiltin, "atof", 1},
    {CDF_MaybeBuiltin, "atoi", 1},
    {CDF_MaybeBuiltin, "atol", 1},
    {CDF_MaybeBuiltin, "malloc", 1},
    {CDF_MaybeBuiltin, "fopen", 2},
    {CDF_MaybeBuiltin, "fclose", 1},
    {CDF_MaybeBuiltin, "fdopen", 1},
    {CDF_MaybeBuiltin, "feof", 1},
    {CDF_MaybeBuiltin, "fflush", 1},
    {CDF_MaybeBuiltin, "fgetc", 1},
    {CDF_MaybeBuiltin, "fgetpos", 2},
    {CDF_MaybeBuiltin, "fgets", 3},
    {CDF_MaybeBuiltin, "fileno", 1},
    {CDF_MaybeBuiltin, "fputs", 2},
    {CDF_MaybeBuiltin, "fread", 4},
    {CDF_MaybeBuiltin, "freopen", 3},
    {CDF_MaybeBuiltin, "fseek", 3},
    {CDF_MaybeBuiltin, "fsetpos", 2},
    {CDF_MaybeBuiltin, "ftell", 1},
    {CDF_MaybeBuiltin, "fwrite", 4},
    {CDF_MaybeBuiltin, "memchr", 3},
    {CDF_MaybeBuiltin, "memcpy", 3},
    {CDF_MaybeBuiltin, "memmove", 3},
    {CDF_MaybeBuiltin, "memset", 3},
    {CDF_MaybeBuiltin, "read", 3},
    {CDF_MaybeBuiltin, "realloc", 2},
    {CDF_MaybeBuiltin, "remove", 1},
    {CDF_MaybeBuiltin, "rename", 2},
    {CDF_MaybeBuiltin, "scanf"},
    {CDF_MaybeBuiltin, "fscanf"},
    {CDF_MaybeBuiltin, "sscanf"},
    {CDF_MaybeBuiltin, "vfscanf"},
    {CDF_MaybeBuiltin, "vfwscanf"},
    {CDF_MaybeBuiltin, "vscanf"},
    {CDF_MaybeBuiltin, "vsscanf"},
    {CDF_MaybeBuiltin, "sprintf"},
    {CDF_MaybeBuiltin, "snprintf"},
    {CDF_MaybeBuiltin, "vdprintf"},
    {CDF_MaybeBuiltin, "vfprintf"},
    {CDF_MaybeBuiltin, "vprintf"},
    {CDF_MaybeBuiltin, "vsnprintf"},
    {CDF_MaybeBuiltin, "vsprintf"},
    {CDF_MaybeBuiltin, "setvbuf", 4},
    {CDF_MaybeBuiltin, "signal", 2},
    {CDF_MaybeBuiltin, "strcat", 2},
    {CDF_MaybeBuiltin, "strchr", 2},
    {CDF_MaybeBuiltin, "strcpy", 2},
    {CDF_MaybeBuiltin, "strerror", 1},
    {CDF_MaybeBuiltin, "strncat", 3},
    {CDF_MaybeBuiltin, "strncpy", 3},
    {CDF_MaybeBuiltin, "strnlen", 2},
    {CDF_MaybeBuiltin, "strpbrk", 2},
    {CDF_MaybeBuiltin, "strrchr", 2},
    {CDF_MaybeBuiltin, "strstr", 2},
    {CDF_MaybeBuiltin, "strtok_r", 3},
    {CDF_MaybeBuiltin, "strtok", 2},
    {CDF_MaybeBuiltin, "tmpfile", 0},
    {CDF_MaybeBuiltin, "tmpnam", 1},
    {CDF_MaybeBuiltin, "tmpnam_r", 1},
    {CDF_MaybeBuiltin, "ungetc", 2},
    {CDF_MaybeBuiltin, "write", 3},
    {CDF_MaybeBuiltin, "pthread_mutex_lock", 1},
    {CDF_MaybeBuiltin, "pthread_mutex_unlock", 1},
    // TODO: Support more functions
};

// It defines C stardard library functions that may load a string pointer.
// Example 1: {{"fdopen", 2}, {{1}}}
// 2 is the number of arguments, 1 is the index of argument that may cause load.
// Example 2: {{"fprintf"}, {{1}, 2}}
// 2 is the index for variadic argument.
const CallDescriptionMap<ArgSet> FuncCharArgsMayRead = {
    {{"fdopen", 2}, {{1}}},         {{"fopen", 2}, {{0, 1}}},
    {{"fprintf"}, {{1}, 2}},        {{"fputs", 2}, {{0}}},
    {{"freopen", 3}, {{0, 1}}},     {{"fscanf"}, {{1}, 2}},
    {{"fsetpos", 2}, {{1}}},        {{"fwrite", 4}, {{0}}},
    {{"memchr", 3}, {{0}}},         {{"memcmp", 3}, {{0, 1}}},
    {{"memcpy", 3}, {{1}}},         {{"memmove", 3}, {{1}}},
    {{"perror", 1}, {{0}}},         {{"printf"}, {{0}, 1}},
    {{"puts", 1}, {{0}}},           {{"remove", 1}, {{0}}},
    {{"rename", 2}, {{0, 1}}},      {{"scanf"}, {{0}, 1}},
    {{"snprintf", 4}, {{2}, 3}},    {{"sprintf", 3}, {{1}, 2}},
    {{"sscanf", 3}, {{0, 1}, 2}},   {{"strcat", 2}, {{0, 1}}},
    {{"strchr", 2}, {{0}}},         {{"strcmp", 2}, {{0, 1}}},
    {{"strcoll", 2}, {{0, 1}}},     {{"strcpy", 2}, {{1}}},
    {{"strcspn", 2}, {{0, 1}}},     {{"strlen", 1}, {{0}}},
    {{"strncat", 3}, {{0, 1}}},     {{"strncmp", 3}, {{0, 1}}},
    {{"strncpy", 3}, {{1}}},        {{"strpbrk", 2}, {{0, 1}}},
    {{"strrchr", 2}, {{0}}},        {{"strspn", 2}, {{0, 1}}},
    {{"strstr", 2}, {{0, 1}}},      {{"strtok", 2}, {{0, 1}}},
    {{"strtok_r", 3}, {{0, 1, 2}}}, {{"strxfrm", 3}, {{1}}},
    {{"vfprintf", 3}, {{1}}},       {{"vfscanf", 3}, {{1}}},
    {{"vfwscanf", 3}, {{1}}},       {{"vprintf", 2}, {{0}}},
    {{"vscanf", 2}, {{0}}},         {{"vsnprintf", 4}, {{1}}},
    {{"vsprintf", 3}, {{1}}},       {{"vsscanf", 3}, {{0, 1}}}};

} // namespace naive
