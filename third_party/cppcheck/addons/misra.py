#!/usr/bin/env python3
#
# MISRA C 2012 checkers
#
# Example usage of this addon (scan a sourcefile main.cpp)
# cppcheck --dump main.cpp
# python misra.py --rule-texts=<path-to-rule-texts> main.cpp.dump
#
# Limitations: This addon is released as open source. Rule texts can't be freely
# distributed. https://www.misra.org.uk/forum/viewtopic.php?f=56&t=1189
#
# The MISRA standard documents may be obtained from https://www.misra.org.uk
#
# Total number of rules: 143

from __future__ import print_function
from multiprocessing.spawn import is_forking

import cppcheckdata
import itertools
import json
import sys
import re
import os
import argparse
import codecs
import string
import copy
import logging

try:
    from itertools import izip as zip
except ImportError:
    pass

import misra_9

def grouped(iterable, n):
    """s -> (s0,s1,s2,...sn-1), (sn,sn+1,sn+2,...s2n-1), (s2n,s2n+1,s2n+2,...s3n-1), ..."""
    return zip(*[iter(iterable)] * n)


INT_TYPES = ['bool', 'char', 'short', 'int', 'long', 'long long']


STDINT_TYPES = ['%s%d_t' % (n, v) for n, v in itertools.product(
        ['int', 'uint', 'int_least', 'uint_least', 'int_fast', 'uint_fast'],
        [8, 16, 32, 64])]


typeBits = {
    'CHAR': None,
    'SHORT': None,
    'INT': None,
    'LONG': None,
    'LONG_LONG': None,
    'POINTER': None
}

_checkRule_2_4 = False
_checkRule_2_5 = False
_checkRule_5_7 = False

def isUnsignedType(ty):
    if not ty:
        return ty
    return ty == 'unsigned' or ty.startswith('uint')


def simpleMatch(token, pattern):
    return cppcheckdata.simpleMatch(token, pattern)


def isIntConstExpr(expr):
    if not expr:
        return False
    if expr.isInt:
        return True
    if expr.isOp:
        if expr.str == '-':
            return isIntConstExpr(expr.astOperand1)
        return isIntConstExpr(expr.astOperand1) and isIntConstExpr(expr.astOperand2)
    return False


def rawlink(rawtoken):
    if rawtoken.str == '}':
        indent = 0
        while rawtoken:
            if rawtoken.str == '}':
                indent = indent + 1
            elif rawtoken.str == '{':
                indent = indent - 1
                if indent == 0:
                    break
            rawtoken = rawtoken.previous
    else:
        rawtoken = None
    return rawtoken


# Identifiers described in Section 7 "Library" of C90 Standard
# Based on ISO/IEC9899:1990 Annex D -- Library summary and
# Annex E -- Implementation limits.
C90_STDLIB_IDENTIFIERS = {
    # D.1 Errors
    'errno.h': ['EDOM', 'ERANGE', 'errno'],
    # D.2 Common definitions
    'stddef.h': ['NULL', 'offsetof', 'ptrdiff_t', 'size_t', 'double', 'int', 'long', 'wchar_t'],
    # D.3 Diagnostics
    'assert.h': ['NDEBUG', 'assert'],
    # D.4 Character handling
    'ctype.h': [
        'isalnum', 'isalpha', 'isblank', 'iscntrl', 'isdigit',
        'isgraph', 'islower', 'isprint', 'ispunct', 'isspace',
        'isupper', 'isxdigit', 'tolower', 'toupper',
    ],
    # D.5 Localization
    'locale.h': [
        'LC_ALL', 'LC_COLLATE', 'LC_CTYPE', 'LC_MONETARY',
        'LC_NUMERIC', 'LC_TIME', 'NULL', 'lconv',
        'setlocale', 'localeconv',
    ],
    # D.6 Mathematics
    'math.h': [
        'HUGE_VAL', 'acos', 'asin' , 'atan2', 'cos', 'sin', 'tan', 'cosh',
        'sinh', 'tanh', 'exp', 'frexp', 'ldexp', 'log', 'log10', 'modf',
        'pow', 'sqrt', 'ceil', 'fabs', 'floor', 'fmod',
    ],
    # D.7 Nonlocal jumps
    'setjmp.h': ['jmp_buf', 'setjmp', 'longjmp'],
    # D.8 Signal handling
    'signal.h': [
        'sig_atomic_t', 'SIG_DFL', 'SIG_ERR', 'SIG_IGN', 'SIGABRT', 'SIGFPE',
        'SIGILL', 'SIGINT', 'SIGSEGV', 'SIGTERM', 'signal', 'raise',
    ],
    # D.9 Variable arguments
    'stdarg.h': ['va_list', 'va_start', 'va_arg', 'va_end'],
    # D.10 Input/output
    'stdio.h': [
        '_IOFBF', '_IOLBF', '_IONBF', 'BUFSIZ', 'EOF', 'FILE', 'FILENAME_MAX',
        'FOPEN_MAX', 'fpos_t', 'L_tmpnam', 'NULL', 'SEEK_CUR', 'SEEK_END',
        'SEEK_SET', 'size_t', 'stderr', 'stdin', 'stdout', 'TMP_MAX',
        'remove', 'rename', 'tmpfile', 'tmpnam', 'fclose', 'fflush', 'fopen',
        'freopen', 'setbuf', 'setvbuf', 'fprintf', 'fscanf', 'printf',
        'scanf', 'sprintf', 'sscanf', 'vfprintf', 'vprintf', 'vsprintf',
        'fgetc', 'fgets', 'fputc', 'fputs', 'getc', 'getchar', 'gets', 'putc',
        'putchar', 'puts', 'ungetc', 'fread', 'fwrite', 'fgetpos', 'fseek',
        'fsetpos', 'rewind', 'clearerr', 'feof', 'ferror', 'perror',
    ],
    # D.11 General utilities
    'stdlib.h': [
        'EXIT_FAILURE', 'EXIT_SUCCESS', 'MB_CUR_MAX', 'NULL', 'RAND_MAX',
        'div_t', 'ldiv_t', 'wchar_t', 'atof', 'atoi', 'strtod', 'rand',
        'srand', 'calloc', 'free', 'malloc', 'realloc', 'abort', 'atexit',
        'exit', 'getenv', 'system', 'bsearch', 'qsort', 'abs', 'div', 'ldiv',
        'lldiv_t', 'mblen', 'mbtowc', 'wctomb', 'mbstowcs', 'wcstombs',
    ],
    # D.12 String handling
    'string.h': [
        'NULL', 'size_t', 'memcpy', 'memmove', 'strcpy', 'strncpy', 'strcat',
        'strncat', 'memcmp', 'strcmp', 'strcoll', 'strncmp', 'strxfrm',
        'memchr', 'strchr', 'strcspn', 'strpbrk', 'strrchr', 'strspn',
        'strstr', 'strtok', 'memset', 'strerror', 'strlen',
    ],
    # D.13 Date and time
    'time.h': [
        'CLK_TCK', 'NULL', 'clock_t', 'time_t', 'size_t', 'tm', 'clock',
        'difftime', 'mktime', 'time', 'asctime', 'ctime', 'gmtime',
        'localtime', 'strftime',
    ],
    # Annex E: Implementation limits
    'limits.h': [
        'CHAR_BIT', 'SCHAR_MIN', 'SCHAR_MAX', 'UCHAR_MAX', 'CHAR_MIN',
        'CHAR_MAX', 'MB_LEN_MAX', 'SHRT_MIN', 'SHRT_MAX', 'USHRT_MAX',
        'INT_MIN', 'INT_MAX', 'UINT_MAX', 'LONG_MIN', 'LONG_MAX', 'ULONG_MAX',
        ],
    'float.h': [
        'FLT_ROUNDS', 'FLT_RADIX', 'FLT_MANT_DIG', 'DBL_MANT_DIG',
        'LDBL_MANT_DIG', 'DECIMAL_DIG', 'FLT_DIG', 'DBL_DIG', 'LDBL_DIG',
        'DBL_MIN_EXP', 'LDBL_MIN_EXP', 'FLT_MIN_10_EXP', 'DBL_MIN_10_EXP',
        'LDBL_MIN_10_EXP', 'FLT_MAX_EXP', 'DBL_MAX_EXP', 'LDBL_MAX_EXP',
        'FLT_MAX_10_EXP', 'DBL_MAX_10_EXP', 'LDBL_MAX_10_EXP', 'FLT_MAX',
        'DBL_MAX', 'LDBL_MAX', 'FLT_MIN', 'DBL_MIN', 'LDBL_MIN',
        'FLT_EPSILON', 'DBL_EPSILON', 'LDBL_EPSILON'
    ],
}


# Identifiers described in Section 7 "Library" of C99 Standard
# Based on ISO/IEC 9899 WF14/N1256 Annex B -- Library summary
C99_STDLIB_IDENTIFIERS = {
    # B.1 Diagnostics
    'assert.h': C90_STDLIB_IDENTIFIERS['assert.h'],
    # B.2 Complex
    'complex.h': [
        'complex', 'imaginary', 'I', '_Complex_I', '_Imaginary_I',
        'CX_LIMITED_RANGE',
        'cacos', 'cacosf', 'cacosl',
        'casin', 'casinf', 'casinl',
        'catan', 'catanf', 'catanl',
        'ccos', 'ccosf', 'ccosl',
        'csin', 'csinf', 'csinl',
        'ctan', 'ctanf', 'ctanl',
        'cacosh', 'cacoshf', 'cacoshl',
        'casinh', 'casinhf', 'casinhl',
        'catanh', 'catanhf', 'catanhl',
        'ccosh', 'ccoshf', 'ccoshl',
        'csinh', 'csinhf', 'csinhl',
        'ctanh', 'ctanhf', 'ctanhl',
        'cexp', 'cexpf', 'cexpl',
        'clog', 'clogf', 'clogl',
        'cabs', 'cabsf', 'cabsl',
        'cpow', 'cpowf', 'cpowl',
        'csqrt', 'csqrtf', 'csqrtl',
        'carg', 'cargf', 'cargl',
        'cimag', 'cimagf', 'cimagl',
        'conj', 'conjf', 'conjl',
        'cproj', 'cprojf', 'cprojl',
        'creal', 'crealf', 'creall',
    ],
    # B.3 Character handling
    'ctype.h': C90_STDLIB_IDENTIFIERS['ctype.h'],
    # B.4 Errors
    'errno.h': C90_STDLIB_IDENTIFIERS['errno.h'] + ['EILSEQ'],
    # B.5 Floating-point environment
    'fenv.h': [
        'fenv_t', 'FE_OVERFLOW', 'FE_TOWARDZERO',
        'fexcept_t', 'FE_UNDERFLOW', 'FE_UPWARD',
        'FE_DIVBYZERO', 'FE_ALL_EXCEPT', 'FE_DFL_ENV',
        'FE_INEXACT', 'FE_DOWNWARD',
        'FE_INVALID', 'FE_TONEAREST',
        'FENV_ACCESS',
        'feclearexcept', 'fegetexceptflag', 'feraiseexcept',
        'fesetexceptflag', 'fetestexcept', 'fegetround',
        'fesetround', 'fegetenv', 'feholdexcept',
        'fesetenv', 'feupdateenv',
    ],
    # B.6 Characteristics of floating types
    'float.h': C90_STDLIB_IDENTIFIERS['float.h'] + ['FLT_EVAL_METHOD',
        'FLT_MIN_EXP'
    ],
    # B.7 Format conversion of integer types
    'inttypes.h': [
        'imaxdiv_t', 'imaxabs', 'imaxdiv', 'strtoimax',
        'strtoumax', 'wcstoimax', 'wcstoumax',
    ],
    # B.8 Alternative spellings
    'iso646.h': [
        'and', 'and_eq', 'bitand', 'bitor', 'compl', 'not', 'not_eq',
        'or', 'or_eq', 'xor', 'xor_eq',
    ],
    # B.9 Size of integer types
    'limits.h': C90_STDLIB_IDENTIFIERS['limits.h'] +
    ['LLONG_MIN', 'LLONG_MAX', 'ULLONG_MAX'],
    # B.10 Localization
    'locale.h': C90_STDLIB_IDENTIFIERS['locale.h'],
    # B.11 Mathematics
    'math.h': C90_STDLIB_IDENTIFIERS['math.h'] + [
        'float_t', 'double_t', 'HUGE_VAL', 'HUGE_VALF', 'HUGE_VALL',
        'INFINITY', 'NAN', 'FP_INFINITE', 'FP_NAN', 'FP_NORMAL',
        'FP_SUBNORMAL', 'FP_ZERO', 'FP_FAST_FMA', 'FP_FAST_FMAF',
        'FP_FAST_FMAL', 'FP_ILOGB0', 'FP_ILOGBNAN', 'MATH_ERRNO',
        'MATH_ERREXCEPT', 'math_errhandling', 'FP_CONTRACT', 'fpclassify',
        'isfinite', 'isinf', 'isnan', 'isnormal', 'signbit', 'acosf', 'acosl',
        'asinf', 'asinl', 'atanf', 'atanl', 'atan2', 'atan2f', 'atan2l',
        'cosf', 'cosl', 'sinf', 'sinl', 'tanf', 'tanl', 'acosh', 'acoshf',
        'acoshl', 'asinh', 'asinhf', 'asinhl', 'atanh', 'atanhf', 'atanhl',
        'cosh', 'coshf', 'coshl', 'sinh', 'sinhf', 'sinhl', 'tanh', 'tanhf',
        'tanhl', 'expf', 'expl', 'exp2', 'exp2f', 'exp2l', 'expm1', 'expm1f',
        'expm1l', 'frexpf', 'frexpl', 'ilogb', 'ilogbf', 'ilogbl', 'float',
        'ldexpf', 'ldexpl', 'logf', 'logl', 'log10f', 'log10l', 'log1p', 'log1pf',
        'log1pl', 'log2', 'log2f', 'log2l', 'logb', 'logbf', 'logbl', 'modff',
        'modfl', 'scalbn', 'scalbnf', 'scalbnl', 'scalbln', 'scalblnf',
        'scalblnl', 'cbrt', 'cbrtf', 'cbrtl', 'fabs', 'fabsf', 'fabsl', 'hypot', 'hypotf',
        'hypotl', 'powf', 'powl', 'sqrtf', 'sqrtl', 'erf', 'erff',
        'erfl', 'erfc', 'erfcf', 'erfcl', 'lgamma', 'lgammaf', 'lgammal',
        'tgamma', 'tgammaf', 'tgammal', 'ceilf', 'ceill', 'floorf', 'floorl',
        'nearbyint', 'nearbyintf', 'nearbyintl', 'rint', 'rintf', 'rintl',
        'lrint', 'lrintf', 'lrintl', 'llrint', 'llrintf', 'llrintl', 'round',
        'roundf', 'roundl', 'lround', 'lroundf', 'lroundl', 'llround',
        'llroundf', 'llroundl', 'trunc', 'truncf', 'truncl', 'fmodf', 'fmodl',
        'remainder', 'remainderf', 'remainderl', 'remquo', 'remquof',
        'remquol', 'copysign', 'copysignf', 'copysignl', 'nan', 'nanf',
        'nanl', 'nextafter', 'nextafterf', 'nextafterl', 'nexttoward',
        'nexttowardf', 'nexttowardl', 'fdim', 'fdimf', 'fdiml', 'fmax',
        'fmaxf', 'fmaxl', 'fmin', 'fminf', 'fminl', 'fma', 'fmaf', 'fmal', 'isgreater',
        'isgreaterequal', 'isless', 'islessequal', 'islessgreater',
        'isunordered',
    ],
    # B.12 Nonlocal jumps
    'setjmp.h': C90_STDLIB_IDENTIFIERS['setjmp.h'],
    # B.13 Signal handling
    'signal.h': C90_STDLIB_IDENTIFIERS['signal.h'],
    # B.14 Variable arguments
    'stdarg.h': C90_STDLIB_IDENTIFIERS['stdarg.h'] + ['va_copy'],
    # B.15 Boolean type and values
    'stdbool.h': ['bool', 'true', 'false', '__bool_true_false_are_defined'],
    # B.16 Common definitions
    'stddef.h': C90_STDLIB_IDENTIFIERS['stddef.h'],
    # B.17 Integer types
    'stdint.h': [
        'intptr_t', 'uintptr_t', 'intmax_t', 'uintmax_t', 'INT8_MIN',
        'INT16_MIN', 'INT32_MIN', 'INT64_MIN', 'INT8_MAX', 'INT16_MAX',
        'INT32_MAX', 'INT64_MAX', 'UINT8_MAX', 'UINT16_MAX', 'UINT32_MAX',
        'UINT64_MAX', 'INT_LEAST8_MIN', 'INT_LEAST16_MIN', 'INT_LEAST32_MIN',
        'INT_LEAST64_MIN', 'INT_LEAST8_MAX', 'INT_LEAST16_MAX',
        'INT_LEAST32_MAX', 'INT_LEAST64_MAX', 'UINT_LEAST8_MAX',
        'UINT_LEAST16_MAX', 'UINT_LEAST32_MAX', 'UINT_LEAST64_MAX',
        'INT_FAST8_MIN', 'INT_FAST16_MIN', 'INT_FAST32_MIN', 'INT_FAST64_MIN',
        'INT_FAST8_MAX', 'INT_FAST16_MAX', 'INT_FAST32_MAX', 'INT_FAST64_MAX',
        'UINT_FAST8_MAX', 'UINT_FAST16_MAX', 'UINT_FAST32_MAX', 'UINT_FAST64_MAX',
        'INTPTR_MIN', 'INTPTR_MAX', 'UINTPTR_MAX', 'INTMAX_MIN', 'INTMAX_MAX',
        'UINTMAX_MAX', 'PTRDIFF_MIN', 'PTRDIFF_MAX', 'SIG_ATOMIC_MIN',
        'SIG_ATOMIC_MAX', 'SIZE_MAX', 'WCHAR_MIN', 'WCHAR_MAX', 'WINT_MIN',
        'WINT_MAX', 'INT8_C', 'INT16_C', 'INT32_C', 'INT64_C', 'UINT8_C',
        'UINT16_C',  'UINT32_C', 'UINT64_C', 'INTMAX_C', 'UINTMAX_C',
    ] + STDINT_TYPES,
    # B.18 Input/output
    'stdio.h': C90_STDLIB_IDENTIFIERS['stdio.h'] + [
        'mode', 'restrict', 'snprintf', 'vfscanf', 'vscanf',
        'vsnprintf', 'vsscanf', 'ftell'
    ],
    # B.19 General utilities
    'stdlib.h': C90_STDLIB_IDENTIFIERS['stdlib.h'] + [
        '_Exit', 'labs', 'llabs', 'lldiv', 'lldiv_t', 'atol', 'atoll',
        'strtof', 'strtol', 'strtold', 'strtoll', 'strtoul', 'strtoull'
    ],
    # B.20 String handling
    'string.h': C90_STDLIB_IDENTIFIERS['string.h'],
    # B.21 Type-generic math
    'tgmath.h': [
        'acos', 'asin', 'atan', 'acosh', 'asinh', 'atanh', 'cos', 'sin', 'tan',
        'cosh', 'sinh', 'tanh', 'exp', 'log', 'pow', 'sqrt', 'fabs', 'atan2',
        'cbrt', 'ceil', 'copysign', 'erf', 'erfc', 'exp2', 'expm1', 'fdim',
        'floor', 'fma', 'fmax', 'fmin', 'fmod', 'frexp', 'hypot', 'ilogb',
        'ldexp', 'lgamma', 'llrint', 'llround', 'log10', 'log1p', 'log2',
        'logb', 'lrint', 'lround', 'nearbyint', 'nextafter', 'nexttoward',
        'remainder', 'remquo', 'rint', 'round', 'scalbn', 'scalbln', 'tgamma',
        'trunc', 'carg', 'cimag', 'conj', 'cproj', 'creal',
    ],
    # B.22 Date and time
    'time.h': C90_STDLIB_IDENTIFIERS['time.h'] + ['CLOCKS_PER_SEC'],
    # B.23 Extended multibyte/wide character utilities
    'wchar.h': [
        'wchar_t', 'size_t', 'mbstate_t', 'wint_t', 'tm', 'NULL', 'WCHAR_MAX',
        'WCHAR_MIN', 'WEOF', 'fwprintf', 'fwscanf', 'swprintf', 'swscanf',
        'vfwprintf', 'vfwscanf', 'vswprintf', 'vswscanf', 'vwprintf',
        'vwscanf', 'wprintf', 'wscanf', 'fgetwc', 'fgetws', 'fputwc', 'fputws',
        'fwide', 'getwc', 'getwchar', 'putwc', 'putwchar', 'ungetwc', 'wcstod',
        'wcstof', 'wcstold', 'double', 'int', 'long', 'long', 'long', 'wcscpy',
        'wcsncpy', 'wmemcpy', 'wmemmove', 'wcscat', 'wcsncat', 'wcscmp',
        'wcstol', 'wcstoll', 'wcscoll', 'wcstoul', 'wcstoull',
        'wcsncmp', 'wcsxfrm', 'wmemcmp', 'wcschr', 'wcscspn', 'wcspbrk',
        'wcsrchr', 'wcsspn', 'wcsstr', 'wcstok', 'wmemchr', 'wcslen',
        'wmemset', 'wcsftime', 'btowc', 'wctob', 'mbsinit', 'mbrlen',
        'mbrtowc', 'wcrtomb', 'mbsrtowcs', 'wcsrtombs'
    ],
    # B.24 Wide character classification and mapping utilities
    'wctype.h': [
        'wint_t', 'wctrans_t', 'wctype_t', 'WEOF', 'iswalnum', 'iswalpha',
        'iswblank', 'iswcntrl', 'iswdigit', 'iswgraph', 'iswlower', 'iswprint',
        'iswpunct', 'iswspace', 'iswupper', 'iswxdigit', 'iswctype', 'wctype',
        'towlower', 'towupper', 'towctrans', 'wctrans'
    ],
    # It's naive systems self managed field
    'naivesystems.h': ['defined'],
}

def isStdLibId(id_, standard='c99'):
    id_lists = []
    if standard == 'c89':
        id_lists = C90_STDLIB_IDENTIFIERS.values()
    elif standard in ('c99', 'c11'):
        id_lists = C99_STDLIB_IDENTIFIERS.values()
    for l in id_lists:
        if id_ in l:
            return True
    return False


# Reserved keywords defined in ISO/IEC9899:1990 -- ch 6.1.1
C90_KEYWORDS = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
    'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
    'int', 'long', 'register', 'return', 'short', 'signed',
    'sizeof', 'static', 'struct', 'switch', 'typedef', 'union', 'unsigned',
    'void', 'volatile', 'while'
}


# Reserved keywords defined in ISO/IEC 9899 WF14/N1256 -- ch. 6.4.1
C99_ADDED_KEYWORDS = {
    'inline', 'restrict', '_Bool', '_Complex', '_Imaginary',
    'bool', 'complex', 'imaginary'
}

C11_ADDED_KEYWORDS = {
    '_Alignas', '_Alignof', '_Atomic', '_Generic', '_Noreturn',
    '_Statis_assert', '_Thread_local' ,
    'alignas', 'alignof', 'noreturn', 'static_assert'
}

def isKeyword(keyword, standard='c99'):
    kw_set = {}
    if standard == 'c89':
        kw_set = C90_KEYWORDS
    elif standard == 'c99':
        kw_set = copy.copy(C90_KEYWORDS)
        kw_set.update(C99_ADDED_KEYWORDS)
    else:
        kw_set = copy.copy(C90_KEYWORDS)
        kw_set.update(C99_ADDED_KEYWORDS)
        kw_set.update(C11_ADDED_KEYWORDS)
    return keyword in kw_set


def is_source_file(file):
    return file.endswith('.c')


def is_header(file):
    return file.endswith('.h') or file.endswith('.hpp')


def is_errno_setting_function(function_name):
    return function_name and \
           function_name in ('ftell', 'fgetpos', 'fsetpos', 'fgetwc', 'fputwc'
                             'strtoimax', 'strtoumax', 'strtol', 'strtoul',
                             'strtoll', 'strtoull', 'strtof', 'strtod', 'strtold'
                             'wcstoimax', 'wcstoumax', 'wcstol', 'wcstoul',
                             'wcstoll', 'wcstoull', 'wcstof', 'wcstod', 'wcstold'
                             'wcrtomb', 'wcsrtombs', 'mbrtowc')


def get_type_conversion_to_from(token):
    def get_typetok(expr):
        if not expr:
            return None
        elif expr.variable:
            return expr.variable.typeStartToken.copy(isInt=False, isFloat=False)
        elif expr.isInt:
            return expr
        elif expr.isFloat:
            return expr

    def get_vartok(expr):
        while expr:
            if isCast(expr):
                if expr.astOperand2 is None:
                    expr = expr.astOperand1
                else:
                    expr = expr.astOperand2
            elif expr.str in ('.', '::'):
                expr = expr.astOperand2
            elif expr.str == '[':
                expr = expr.astOperand1
            else:
                break
        if expr and (expr.variable or expr.isInt or expr.isFloat):
            return expr
        else:
            return None


    if isCast(token):
        vartok = get_vartok(token)
        if vartok:
            return (token.next, get_typetok(vartok))

    elif token.str == '=':
        lhs = get_vartok(token.astOperand1)
        rhs = get_vartok(token.astOperand2)
        if lhs and rhs:
            return (get_typetok(lhs), get_typetok(rhs))

    return None

def getEssentialTypeCategory(expr):
    if not expr:
        return None
    if expr.str == ',' or expr.str == '.':
        return getEssentialTypeCategory(expr.astOperand2)
    if expr.str in ('<', '<=', '==', '!=', '>=', '>', '&&', '||', '!'):
        return 'bool'
    if expr.str in ('<<', '>>'):
        # TODO this is incomplete
        return getEssentialTypeCategory(expr.astOperand1)
    # Appendix D, D.7.8 Conditional
    if expr.str == '?' and expr.astOperand2.str == ':':
        e1 = getEssentialTypeCategory(expr.astOperand2.astOperand1)
        e2 = getEssentialTypeCategory(expr.astOperand2.astOperand2)
        if e1 and e2 and e1 == e2:
            return e1
        if expr.valueType:
            return expr.valueType.sign
    if len(expr.str) == 1 and expr.str in '+-*/%&|^':
        # TODO this is incomplete
        e1 = getEssentialTypeCategory(expr.astOperand1)
        e2 = getEssentialTypeCategory(expr.astOperand2)
        # print('{0}: {1} {2}'.format(expr.str, e1, e2))
        if e1 and e2 and e1 == e2:
            return e1
        # Appendix D, D.7.10 Addition with char
        if expr.str == '+':
            if e1 == 'char' and e2 in ('signed', 'unsigned'):
                return e1
            if e2 == 'char' and e1 in ('signed', 'unsigned'):
                return e2
        # Appendix D, D.7.11 Subtraction with char
        if expr.str == '-':
            if e1 == 'char' and e2 in ('signed', 'unsigned'):
                return e1
        if expr.valueType:
            return expr.valueType.sign
    if expr.valueType and expr.valueType.typeScope and expr.valueType.typeScope.className:
        if expr.valueType.typeScope.className.find('Anonymous') == -1:
            if expr.valueType.typeScope.isBooleanEnum:
                return 'bool'
            return "enum<" + expr.valueType.typeScope.className + "_" + expr.valueType.typeScope.Id + ">"
        return "signed"
    vartok = expr
    while simpleMatch(vartok, '[') or (vartok and vartok.str == '*' and vartok.astOperand2 is None):
        vartok = vartok.astOperand1
    if vartok and vartok.variable:
        typeToken = vartok.variable.typeStartToken
        while typeToken and typeToken.isName:
            if typeToken.str == 'char' and not typeToken.isSigned and not typeToken.isUnsigned:
                return 'char'
            if typeToken.valueType:
                if typeToken.valueType.type == 'bool':
                    return 'bool'
                if typeToken.valueType.type in ('float', 'double', 'long double'):
                    return "floating"
                if typeToken.valueType.sign:
                    return typeToken.valueType.sign
            typeToken = typeToken.next

    # See Appendix D, section D.6, Character constants
    if expr.str[0] == "'" and expr.str[-1] == "'":
        if len(expr.str) == 3 or (len(expr.str) == 4 and expr.str[1] == '\\'):
            return 'char'
        return expr.valueType.sign

    if expr.valueType:
        if expr.valueType.type == 'bool':
            return 'bool'
        if expr.valueType.type == 'container':
            return 'container'
        if expr.valueType.isFloat():
            return 'floating'
        return expr.valueType.sign
    return None


def getEssentialCategorylist(operand1, operand2):
    if not operand1 or not operand2:
        return None, None
    if (operand1.str in ('++', '--') or
            operand2.str in ('++', '--')):
        return None, None
    if ((operand1.valueType and operand1.valueType.pointer) or
            (operand2.valueType and operand2.valueType.pointer)):
        return None, None
    e1 = getEssentialTypeCategory(operand1)
    e2 = getEssentialTypeCategory(operand2)
    return e1, e2


def get_essential_type_from_value(value, is_signed):
    if value is None:
        return None
    for t in ('char', 'short', 'int', 'long', 'long long'):
        bits = bitsOfEssentialType(t)
        if bits >= 64:
            continue
        if is_signed:
            range_min = -(1 << (bits - 1))
            range_max = (1 << (bits - 1)) - 1
        else:
            range_min = 0
            range_max = (1 << bits) - 1
        sign = 'signed' if is_signed else 'unsigned'
        if is_signed and value < 0 and value >= range_min:
            return '%s %s' % (sign, t)
        if value >= 0 and value <= range_max:
            return '%s %s' % (sign, t)
    return None

def getEssentialType(expr):
    if not expr:
        return None

    # See Appendix D, section D.6, Character constants
    if expr.str[0] == "'" and expr.str[-1] == "'":
        if len(expr.str) == 3 or (len(expr.str) == 4 and expr.str[1] == '\\'):
            return 'char'
        return '%s %s' % (expr.valueType.sign, expr.valueType.type)
    # enum type
    if expr.valueType and expr.valueType.typeScope and expr.valueType.typeScope.type == 'Enum' and expr.valueType.typeScope.className:
        if expr.valueType.typeScope.className.find('Anonymous') == -1:
            if expr.valueType.typeScope.isBooleanEnum:
                return 'bool'
            return "enum<" + expr.valueType.typeScope.className + "_" + expr.valueType.typeScope.Id + ">"
        return get_essential_type_from_value(expr.getKnownIntValue(), expr.valueType.sign == 'signed')

    if expr.variable or isCast(expr):
        typeToken = expr.variable.typeStartToken if expr.variable else expr.next
        while typeToken and typeToken.isName:
            if typeToken.str == 'char' and not typeToken.isSigned and not typeToken.isUnsigned:
                return 'char'
            typeToken = typeToken.next
        if expr.valueType:
            if expr.valueType.type == 'bool':
                return 'bool'
            if expr.valueType.isFloat():
                return expr.valueType.type
            if expr.valueType.isIntegral():
                if (expr.valueType.sign is None) and expr.valueType.type == 'char':
                    return 'char'
                return '%s %s' % (expr.valueType.sign, expr.valueType.type)

    elif expr.isNumber:
        # Appendix D, D.6 The essential type of literal constants
        # Integer constants
        if expr.valueType.type == 'bool':
            return 'bool'
        if expr.valueType.isFloat():
            return expr.valueType.type
        if expr.valueType.isIntegral():
            if expr.valueType.type != 'int':
                return '%s %s' % (expr.valueType.sign, expr.valueType.type)
            return get_essential_type_from_value(expr.getKnownIntValue(), expr.valueType.sign == 'signed')

    elif expr.str in ('<', '<=', '>=', '>', '==', '!=', '&&', '||', '!'):
        return 'bool'

    elif expr.astOperand1 and expr.astOperand2 and expr.str in (
    '+', '-', '*', '/', '%', '&', '|', '^', '>>', "<<", "?", ":"):
        if expr.astOperand1.valueType and expr.astOperand1.valueType.pointer > 0:
            return None
        if expr.astOperand2.valueType and expr.astOperand2.valueType.pointer > 0:
            return None
        e1 = getEssentialType(expr.astOperand1)
        e2 = getEssentialType(expr.astOperand2)
        if e1 is None or e2 is None:
            return None
        if is_constant_integer_expression(expr):
            sign1 = e1.split(' ')[0]
            sign2 = e2.split(' ')[0]
            if sign1 == sign2 and sign1 in ('signed', 'unsigned'):
                e = get_essential_type_from_value(expr.getKnownIntValue(), sign1 == 'signed')
                if e:
                    return e
        # Appendix D, D.7.10 Addition with char
        if expr.str == '+':
            if e1 == 'char' and ('signed' in e2 or 'unsigned' in e2):
                return e1
            if e2 == 'char' and ('signed' in e1 or 'unsigned' in e1):
                return e2
        # Appendix D, D.7.11 Subtraction with char
        if expr.str == '-':
            if e1 == 'char' and ('signed' in e2 or 'unsigned' in e2):
                return e1
        if bitsOfEssentialType(e2) >= bitsOfEssentialType(e1):
            return e2
        else:
            return e1

    elif expr.str == "~":
        e1 = getEssentialType(expr.astOperand1)
        return e1

    elif expr.str == ',' or expr.str == '.':
        return getEssentialType(expr.astOperand2)

    # function call as operand
    elif expr.str == '(':
        if expr.valueType:
            if expr.valueType.type == 'bool':
                return 'bool'
            if expr.valueType.isFloat():
                return expr.valueType.type
            if expr.valueType.isIntegral():
                if (expr.valueType.sign is None) and expr.valueType.type == 'char':
                    return 'char'
                return '%s %s' % (expr.valueType.sign, expr.valueType.type)

    # get essential type of a in a[i] or *a
    vartok = expr
    while simpleMatch(vartok, '[') or (vartok and vartok.str == '*' and vartok.astOperand2 is None):
        vartok = vartok.astOperand1
    # Appendix D.1
    if vartok and vartok.variable:
        typeToken = vartok.variable.typeStartToken
        while typeToken and typeToken.isName:
            if typeToken.str == 'char' and not typeToken.isSigned and not typeToken.isUnsigned:
                return 'char'
            if typeToken.valueType:
                if typeToken.valueType.type == 'bool':
                    return 'bool'
                if typeToken.valueType.type in ('float', 'double', 'long double'):
                    return 'floating'
                if typeToken.valueType.sign:
                    return '%s %s' % (typeToken.valueType.sign, typeToken.valueType.type)
            typeToken = typeToken.next
    return None


def getFunctionDefinitionReturnEssentialType(expr):
    if not expr:
        return None
    # enum type
    if expr.valueType and expr.valueType.typeScope and expr.valueType.typeScope.type == 'Enum' and expr.valueType.typeScope.className:
        if expr.valueType.typeScope.isBooleanEnum:
                return 'bool'
        return "enum<" + expr.valueType.typeScope.className + "_" + expr.valueType.typeScope.Id + ">"

    if expr.str == '(':
        typeToken = expr.previous.previous
        while typeToken and typeToken.isName:
            if typeToken.str == 'char' and not typeToken.isSigned and not typeToken.isUnsigned:
                return 'char'
            typeToken = typeToken.previous
        if expr.valueType:
            if expr.valueType.type == 'bool':
                return 'bool'
            if expr.valueType.isFloat():
                return expr.valueType.type
            if expr.valueType.isIntegral():
                if (expr.valueType.sign is None) and expr.valueType.type == 'char':
                    return 'char'
                return '%s %s' % (expr.valueType.sign, expr.valueType.type)

    return None


def bitsOfEssentialType(ty):
    if ty is None:
        return 0
    last_type = ty.split(' ')[-1]
    if last_type == 'Boolean':
        return 1
    if last_type == 'char':
        return typeBits['CHAR']
    if last_type == 'short':
        return typeBits['SHORT']
    if last_type == 'int' or 'enum' in ty:
        return typeBits['INT']
    if ty.endswith('long long'):
        return typeBits['LONG_LONG']
    if last_type == 'long':
        return typeBits['LONG']
    for sty in STDINT_TYPES:
        if ty == sty:
            return int(''.join(filter(str.isdigit, sty)))
    return 0

def relativeLengthOfEssentialFloatingType(ty):
    if ty is None:
        return 0
    if ty == "float":
        return 1
    if ty == "double":
        return 2
    if ty == "long double":
        return 3

def isBooleanEnum(scope):
    token = scope.bodyStart
    haveTwoElements = False
    while token and token.str != "}":
        if token.str != ",":
            token = token.next
            continue
        if not haveTwoElements:
            operand1 = token.astOperand1
            operand2 = token.astOperand2
            # enum a = {x, y};
            if not operand1:
                operand1 = token.previous
            # enum a = {x = 1, y};
            elif operand1.isAssignmentOp:
                operand1 = operand1.astOperand1
            else:
                operand1 = None
            if not operand2:
                operand2 = token.next
            elif operand2.isAssignmentOp:
                operand2 = operand2.astOperand1
            else:
                operand2 = None
            # enum a = {x,}
            if operand2 and operand2.str == "}":
                return False
            if operand1 and operand2:
                value1 = operand1.getKnownIntValue()
                value2 = operand2.getKnownIntValue()
                if (value1, value2) != (0, 1) and (value1, value2) != (1, 0):
                    return False
                haveTwoElements = True
            else:
                return False
        # enum a = {x, y, z};
        # exclude the case like enum a = {x, y,};
        elif not token.next or token.next != "}":
            return False
        token = token.next
    return haveTwoElements


def get_function_pointer_type(tok):
    ret = ''
    par = 0
    while tok and (tok.isName or tok.str == '*'):
        ret += ' ' + tok.str
        tok = tok.next
    if tok is None or tok.str != '(':
        return None
    tok = tok.link
    if not simpleMatch(tok, ') ('):
        return None
    ret += '('
    tok = tok.next.next
    while tok and (tok.str not in '()'):
        ret += ' ' + tok.str
        tok = tok.next
    if (tok is None) or tok.str != ')':
        return None
    return ret[1:] + ')'

def get_literal_or(func, tok):
    if tok and (tok.isInt or tok.isFloat):
        return tok.str
    else:
        return func(tok)

def get_literal_token_or(func, tok):
    if tok and (tok.isInt or tok.isFloat):
        return tok
    else:
        return func(tok)

def isCast(expr):
    if not expr or expr.str != '(' or not expr.astOperand1 or expr.astOperand2:
        return False
    if simpleMatch(expr, '( )'):
        return False
    return True

def is_constant_integer_expression(expr):
    if expr is None:
        return False
    if expr.isInt:
        return True
    if not expr.isArithmeticalOp:
        return False
    if expr.astOperand1 and not is_constant_integer_expression(expr.astOperand1):
        return False
    if expr.astOperand2 and not is_constant_integer_expression(expr.astOperand2):
        return False
    return True

def isFunctionCall(expr, std='c99'):
    if not expr:
        return False
    if expr.str != '(' or not expr.astOperand1:
        return False
    if expr.astOperand1 != expr.previous:
        return False
    if isKeyword(expr.astOperand1.str, std):
        return False
    return True


def hasExternalLinkage(var):
    return var.isGlobal and not var.isStatic


def countSideEffects(expr):
    if not expr or expr.str in (',', ';'):
        return 0
    ret = 0
    if expr.str in ('++', '--', '='):
        ret = 1
    return ret + countSideEffects(expr.astOperand1) + countSideEffects(expr.astOperand2)


def getForLoopExpressions(forToken):
    if not forToken or forToken.str != 'for':
        return None
    lpar = forToken.next
    if not lpar or lpar.str != '(':
        return None
    if not lpar.astOperand2 or lpar.astOperand2.str != ';':
        return None
    if not lpar.astOperand2.astOperand2 or lpar.astOperand2.astOperand2.str != ';':
        return None
    return [lpar.astOperand2.astOperand1,
            lpar.astOperand2.astOperand2.astOperand1,
            lpar.astOperand2.astOperand2.astOperand2]


def getForLoopCounterVariables(forToken):
    """ Return a set of Variable objects defined in ``for`` statement and
    satisfy requirements to loop counter term from section 8.14 of MISRA
    document.
    """
    if not forToken or forToken.str != 'for':
        return None
    tn = forToken.next
    if not tn or tn.str != '(':
        return None
    vars_defined = set()
    vars_exit = set()
    vars_modified = set()
    cur_clause = 1
    te = tn.link
    while tn and tn != te:
        if tn.variable:
            if cur_clause == 1 and tn.variable.nameToken == tn:
                vars_defined.add(tn.variable)
            elif cur_clause == 2:
                vars_exit.add(tn.variable)
            elif cur_clause == 3:
                if tn.next and hasSideEffectsRecursive(tn.next):
                    vars_modified.add(tn.variable)
                elif tn.previous and tn.previous.str in ('++', '--'):
                    vars_modified.add(tn.variable)
        if tn.str == ';':
            cur_clause += 1
        tn = tn.next
    return vars_defined & vars_exit & vars_modified


def findCounterTokens(cond):
    if not cond:
        return []
    if cond.str in ['&&', '||']:
        c = findCounterTokens(cond.astOperand1)
        c.extend(findCounterTokens(cond.astOperand2))
        return c
    ret = []
    if ((cond.isArithmeticalOp and cond.astOperand1 and cond.astOperand2) or
            (cond.isComparisonOp and cond.astOperand1 and cond.astOperand2)):
        if cond.astOperand1.isName:
            ret.append(cond.astOperand1)
        if cond.astOperand2.isName:
            ret.append(cond.astOperand2)
        if cond.astOperand1.isOp:
            ret.extend(findCounterTokens(cond.astOperand1))
        if cond.astOperand2.isOp:
            ret.extend(findCounterTokens(cond.astOperand2))
    return ret


def isFloatCounterInWhileLoop(whileToken):
    if not simpleMatch(whileToken, 'while ('):
        return False
    lpar = whileToken.next
    rpar = lpar.link
    counterTokens = findCounterTokens(lpar.astOperand2)
    whileBodyStart = None
    if simpleMatch(rpar, ') {'):
        whileBodyStart = rpar.next
    elif simpleMatch(whileToken.previous, '} while') and simpleMatch(whileToken.previous.link.previous, 'do {'):
        whileBodyStart = whileToken.previous.link
    else:
        return False
    token = whileBodyStart
    while token != whileBodyStart.link:
        token = token.next
        for counterToken in counterTokens:
            if not counterToken.valueType or not counterToken.valueType.isFloat():
                continue
            if token.isAssignmentOp and token.astOperand1.str == counterToken.str:
                return True
            if token.str == counterToken.str and token.astParent and token.astParent.str in ('++', '--'):
                return True
    return False


def hasSideEffectsRecursive(expr):
    if not expr or expr.str == ';':
        return False
    if expr.str == '=' and expr.astOperand1 and expr.astOperand1.str == '[':
        prev = expr.astOperand1.previous
        if prev and (prev.str == '{' or prev.str == '{'):
            return hasSideEffectsRecursive(expr.astOperand2)
    if expr.str == '=' and expr.astOperand1 and expr.astOperand1.str == '.':
        e = expr.astOperand1
        while e and e.str == '.' and e.astOperand2:
            e = e.astOperand1
        if e and e.str == '.':
            return False
    if expr.isAssignmentOp or expr.str in {'++', '--'}:
        return True
    if expr.function:
        return True # We assume all functions have side effects
    is_volatile_var = lambda x: (x.variable and x.variable.volatility > 0)
    if (expr.astOperand1 and is_volatile_var(expr.astOperand1)) or (expr.astOperand2 and is_volatile_var(expr.astOperand2)):
        return True
    return hasSideEffectsRecursive(expr.astOperand1) or hasSideEffectsRecursive(expr.astOperand2)


def isBoolExpression(expr):
    if not expr:
        return False
    if expr.valueType and (expr.valueType.type == 'bool' or expr.valueType.bits == 1):
        return True
    if getEssentialType(expr) == 'bool':
        return True
    return expr.str in ['!', '==', '!=', '<', '<=', '>', '>=', '&&', '||', '0', '1', 'true', 'false']


def isEnumConstant(expr):
    if not expr or not expr.values:
        return False
    values = expr.values
    return len(values) == 1 and values[0].valueKind == 'known'


def isConstantExpression(expr):
    if expr.isNumber:
        return True
    if expr.isName and not isEnumConstant(expr):
        return False
    if simpleMatch(expr.previous, 'sizeof ('):
        return True
    if expr.astOperand1 and not isConstantExpression(expr.astOperand1):
        return False
    if expr.astOperand2 and not isConstantExpression(expr.astOperand2):
        return False
    return True


def isUnsignedInt(expr):
    return expr and expr.valueType and expr.valueType.type in ('short', 'int') and expr.valueType.sign == 'unsigned'


def getPrecedence(expr):
    if not expr:
        return 16
    if not expr.astOperand1 or not expr.astOperand2:
        return 16
    if expr.str in ('*', '/', '%'):
        return 12
    if expr.str in ('+', '-'):
        return 11
    if expr.str in ('<<', '>>'):
        return 10
    if expr.str in ('<', '>', '<=', '>='):
        return 9
    if expr.str in ('==', '!='):
        return 8
    if expr.str == '&':
        return 7
    if expr.str == '^':
        return 6
    if expr.str == '|':
        return 5
    if expr.str == '&&':
        return 4
    if expr.str == '||':
        return 3
    if expr.str in ('?', ':'):
        return 2
    if expr.isAssignmentOp:
        return 1
    if expr.str == ',':
        return 0
    return -1


def findRawLink(token):
    tok1 = None
    tok2 = None
    forward = False

    if token.str in '{([':
        tok1 = token.str
        tok2 = '})]'['{(['.find(token.str)]
        forward = True
    elif token.str in '})]':
        tok1 = token.str
        tok2 = '{(['['})]'.find(token.str)]
        forward = False
    else:
        return None

    # try to find link
    indent = 0
    while token:
        if token.str == tok1:
            indent = indent + 1
        elif token.str == tok2:
            if indent <= 1:
                return token
            indent = indent - 1
        if forward is True:
            token = token.next
        else:
            token = token.previous

    # raw link not found
    return None


def numberOfParentheses(tok1, tok2):
    while tok1 and tok1 != tok2:
        if tok1.str == '(' or tok1.str == ')':
            return False
        tok1 = tok1.next
    return tok1 == tok2


def hasExplicitPrecedence(tok1, tok2):
    unpaired = 0
    while tok1 and tok1 != tok2:
        if tok1.str == '(':
            unpaired = unpaired + 1
        elif tok1.str == ')':
            unpaired = unpaired - 1
        tok1 = tok1.next
    return unpaired != 0


def findGotoLabel(gotoToken):
    label = gotoToken.next.str
    tok = gotoToken.next.next
    while tok:
        if tok.str == '}' and tok.scope.type == 'Function':
            break
        if tok.str == label and tok.next.str == ':':
            return tok
        tok = tok.next
    return None


def findInclude(directives, header):
    pattern = f"# *include *{header}"
    for directive in directives:
        if re.match(pattern, directive.str):
            return directive
    return None


# Get function arguments
def getArgumentsRecursive(tok, arguments):
    if tok is None:
        return
    if tok.str == ',':
        getArgumentsRecursive(tok.astOperand1, arguments)
        getArgumentsRecursive(tok.astOperand2, arguments)
    else:
        arguments.append(tok)


def getArguments(ftok):
    arguments = []
    getArgumentsRecursive(ftok.astOperand2, arguments)
    return arguments


def isalnum(c):
    return c in string.digits or c in string.ascii_letters


def isHexEscapeSequence(symbols):
    """Checks that given symbols are valid hex escape sequence.

    hexadecimal-escape-sequence:
            \\x hexadecimal-digit
            hexadecimal-escape-sequence hexadecimal-digit

    Reference: n1570 6.4.4.4"""
    if len(symbols) < 3 or symbols[:2] != '\\x':
        return False
    return all([s in string.hexdigits for s in symbols[2:]])


def isOctalEscapeSequence(symbols):
    r"""Checks that given symbols are valid octal escape sequence:

     octal-escape-sequence:
             \ octal-digit
             \ octal-digit octal-digit
             \ octal-digit octal-digit octal-digit

    Reference: n1570 6.4.4.4"""
    if len(symbols) not in range(2, 5) or symbols[0] != '\\':
        return False
    return all([s in string.octdigits for s in symbols[1:]])


def isSimpleEscapeSequence(symbols):
    """Checks that given symbols are simple escape sequence.
    Reference: n1570 6.4.4.4"""
    if len(symbols) != 2 or symbols[0] != '\\':
        return False
    return symbols[1] in ("'", '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v')


def isTernaryOperator(token):
    if not token:
        return False
    if not token.astOperand2:
        return False
    return token.str == '?' and token.astOperand2.str == ':'


def getTernaryOperandsRecursive(token):
    """Returns list of ternary operands including nested ones."""
    if not isTernaryOperator(token):
        return []
    result = []
    result += getTernaryOperandsRecursive(token.astOperand2.astOperand1)
    if token.astOperand2.astOperand1 and not isTernaryOperator(token.astOperand2.astOperand1):
        result += [token.astOperand2.astOperand1]
    result += getTernaryOperandsRecursive(token.astOperand2.astOperand2)
    if token.astOperand2.astOperand2 and not isTernaryOperator(token.astOperand2.astOperand2):
        result += [token.astOperand2.astOperand2]
    return result


def hasNumericEscapeSequence(symbols):
    """Check that given string contains octal or hexadecimal escape sequences."""
    if '\\' not in symbols:
        return False
    for c, cn in grouped(symbols, 2):
        if c == '\\' and cn in ('x' + string.octdigits):
            return True
    return False


def isNoReturnScope(tok):
    if tok is None or tok.str != '}':
        return False
    if tok.previous is None or tok.previous.str != ';':
        return False
    if simpleMatch(tok.previous.previous, 'break ;'):
        return True
    prev = tok.previous.previous
    while prev and prev.str not in ';{}':
        if prev.str in '])':
            prev = prev.link
        prev = prev.previous
    if prev and prev.next.str in ['throw', 'return']:
        return True
    return False

# find all enum types in the type scope
def get_enum_types(type_scope):
    enum_types = {} # enum_type name -> enum_type token
    tok = type_scope.bodyStart.next
    while tok != type_scope.bodyEnd:
        if tok.isName:
            enum_types[tok.str] = tok
        tok = tok.next
    return enum_types

# Return the token which the value is assigned to
def getAssignedVariableToken(valueToken):
    if not valueToken:
        return None
    if not valueToken.astParent:
        return None
    operator = valueToken.astParent
    while operator.astParent:
        # ignore the valueToken if it is argument
        if operator.str == '(':
            return None
        operator = operator.astParent
    if operator.isAssignmentOp:
        # Skip all []
        # "12345"[0] = "1"; will return "12345"
        while operator.astOperand1.str == '[':
            operator = operator.astOperand1
        return operator.astOperand1
    if operator.isArithmeticalOp:
        return getAssignedVariableToken(operator)
    return None

# If the value is used as a return value, return the function definition
def getFunctionUsingReturnValue(valueToken):
    if not valueToken:
        return None
    if not valueToken.astParent:
        return None
    operator = valueToken.astParent
    if operator.str == 'return':
        return operator.scope.function
    if operator.isArithmeticalOp:
        return getFunctionUsingReturnValue(operator)
    return None

# Return true if the token follows a specific sequence of token str values
def tokenFollowsSequence(token, sequence):
    if not token:
        return False
    for i in reversed(sequence):
        prev = token.previous
        if not prev:
            return False
        if prev.str != i:
            return False
        token = prev
    return True

class Define:
    def __init__(self, directive):
        self.name = ''
        self.args = []
        self.expansionList = ''
        self.isFunction = False

        res = re.match(r'#define ([A-Za-z0-9_]+)\(([A-Za-z0-9_, ]+)\)[ ]+(.*)', directive.str)
        if res:
            self.name = res.group(1)
            self.args = res.group(2).strip().split(',')
            self.isFunction = True
            self.expansionList = res.group(3)
            return
        res = re.match(r'#define ([A-Za-z0-9_]+)[ ]+(.*)', directive.str)
        if res:
            self.name = res.group(1)
            self.expansionList = res.group(2)
            return
        res = re.match(r'#define ([A-Za-z0-9_]+)', directive.str)
        if res:
            self.name = res.group(1)

    def __repr__(self):
        attrs = ["name", "args", "expansionList"]
        return "{}({})".format(
            "Define",
            ", ".join(("{}={}".format(a, repr(getattr(self, a))) for a in attrs))
        )


def getAddonRules():
    """Returns dict of MISRA rules handled by this addon."""
    addon_rules = []
    compiled = re.compile(r'.*def[ ]+misra_([0-9]+)_([0-9]+)[(].*')
    for line in open(__file__):
        res = compiled.match(line)
        if res is None:
            continue
        addon_rules.append(res.group(1) + '.' + res.group(2))
    return addon_rules


def getCppcheckRules():
    """Returns list of rules handled by cppcheck."""
    return ['1.3', # <most "error">
            '2.1', # alwaysFalse, duplicateBreak
            '2.2', # alwaysTrue, redundantCondition, redundantAssignment, redundantAssignInSwitch, unreadVariable
            '2.6', # unusedLabel
            '5.3', # shadowVariable
            '8.3', # funcArgNamesDifferent
            '8.13', # constPointer
            '9.1', # uninitvar
            '14.3', # alwaysTrue, alwaysFalse, compareValueOutOfTypeRangeError
            '13.2', # unknownEvaluationOrder
            '13.6', # sizeofCalculation
            '17.4', # missingReturn
            '17.5', # argumentSize
            '18.1', # pointerOutOfBounds
            '18.2', # comparePointers
            '18.3', # comparePointers
            '18.6', # danglingLifetime
            '19.1', # overlappingWriteUnion, overlappingWriteFunction
            '20.6', # preprocessorErrorDirective
            '21.13', # invalidFunctionArg
            '21.17', # bufferAccessOutOfBounds
            '21.18', # bufferAccessOutOfBounds
            '22.1', # memleak, resourceLeak, memleakOnRealloc, leakReturnValNotUsed, leakNoVarFunctionCall
            '22.2', # autovarInvalidDeallocation
            '22.3', # incompatibleFileOpen
            '22.4', # writeReadOnlyFile
            '22.6' # useClosedFile
           ]


def generateTable():
    # print table
    numberOfRules = {}
    numberOfRules[1] = 3
    numberOfRules[2] = 7
    numberOfRules[3] = 2
    numberOfRules[4] = 2
    numberOfRules[5] = 9
    numberOfRules[6] = 2
    numberOfRules[7] = 4
    numberOfRules[8] = 14
    numberOfRules[9] = 5
    numberOfRules[10] = 8
    numberOfRules[11] = 9
    numberOfRules[12] = 4
    numberOfRules[13] = 6
    numberOfRules[14] = 4
    numberOfRules[15] = 7
    numberOfRules[16] = 7
    numberOfRules[17] = 8
    numberOfRules[18] = 8
    numberOfRules[19] = 2
    numberOfRules[20] = 14
    numberOfRules[21] = 21
    numberOfRules[22] = 10

    # Rules that can be checked with compilers:
    # compiler = ['1.1', '1.2']

    addon = getAddonRules()
    cppcheck = getCppcheckRules()
    for i1 in range(1, 23):
        for i2 in range(1, numberOfRules[i1] + 1):
            num = str(i1) + '.' + str(i2)
            s = ''
            if num in addon:
                s = 'X (Addon)'
            elif num in cppcheck:
                s = 'X (Cppcheck)'
            num = num + '       '
            print(num[:8] + s)


def remove_file_prefix(file_path, prefix):
    """
    Remove a file path prefix from a give path.  leftover
    directory separators at the beginning of a file
    after the removal are also stripped.

    Example:
        '/remove/this/path/file.c'
    with a prefix of:
        '/remove/this/path'
    becomes:
        file.c
    """
    result = None
    if file_path.startswith(prefix):
        result = file_path[len(prefix):]
        # Remove any leftover directory separators at the
        # beginning
        result = result.lstrip('\\/')
    else:
        result = file_path
    return result

def removeprefix(s, prefix):
    result = None
    if s.startswith(prefix):
        result = s[len(prefix):]
    else:
        result = s
    return result

class Rule(object):
    """Class to keep rule text and metadata"""

    MISRA_SEVERITY_LEVELS = ['Required', 'Mandatory', 'Advisory']

    def __init__(self, num1, num2):
        self.num1 = num1
        self.num2 = num2
        self.text = ''
        self.misra_severity = ''

    @property
    def num(self):
        return self.num1 * 100 + self.num2

    @property
    def misra_severity(self):
        return self._misra_severity

    @misra_severity.setter
    def misra_severity(self, val):
        if val in self.MISRA_SEVERITY_LEVELS:
            self._misra_severity = val
        else:
            self._misra_severity = ''

    @property
    def cppcheck_severity(self):
        return 'style'

    def __repr__(self):
        return "%d.%d (%s)" % (self.num1, self.num2, self.misra_severity)

class ErrorLocation:
    def __init__(self, path, line_num):
        self.path = path
        self.line_number = line_num

class Result:
    rule_trans_map = {
        #c2012-.1
        "c2012-1.4": "[C2204]",
        #c2012-.2
        "c2012-2.4": "[C2004]",
        "c2012-2.5": "[C2003]",
        "c2012-2.6": "[C2002]",
        "c2012-2.7": "[C2001]",
        #c2012-.3
        "c2012-3.1": "[C2102]",
        "c2012-3.2": "[C2101]",
        #c2012-.4
        "c2012-4.1": "[C1002]",
        "c2012-4.2": "[C1001]",
        #c2012-.5
        "c2012-5.2": "[C1108]",
        "c2012-5.3": "[C1107]",
        "c2012-5.4": "[C1106]",
        "c2012-5.5": "[C1105]",
        #c2012-.6
        "c2012-6.1": "[C0702]",
        "c2012-6.2": "[C0701]",
        #c2012-.7
        "c2012-7.1": "[C0904]",
        "c2012-7.2": "[C0903]",
        "c2012-7.3": "[C0902]",
        "c2012-7.4": "[C0901]",
        #c2012-.8
        "c2012-8.1": "[C0514]",
        "c2012-8.8": "[C0507]",
        "c2012-8.9": "[C0506]",
        "c2012-8.10": "[C0505]",
        "c2012-8.11": "[C0504]",
        "c2012-8.12": "[C0503]",
        "c2012-8.14": "[C0501]",
        #c2012-.9
        "c2012-9.2": "[C1204]",
        "c2012-9.3": "[C1203]",
        "c2012-9.4": "[C1202]",
        "c2012-9.5": "[C1201]",
        #c2012-.10
        "c2012-10.1": "[C0808]",
        "c2012-10.2": "[C0807]",
        "c2012-10.3": "[C0806]",
        "c2012-10.4": "[C0805]",
        "c2012-10.5": "[C0804]",
        "c2012-10.6": "[C0803]",
        "c2012-10.7": "[C0802]",
        "c2012-10.8": "[C0801]",
        #c2012-.11
        "c2012-11.4": "[C1406]",
        "c2012-11.5": "[C1405]",
        "c2012-11.9": "[C1401]",
        #c2012-.12
        "c2012-12.1": "[C0605]",
        "c2012-12.4": "[C0602]",
        "c2012-12.5": "[C0601]",
        #c2012-.13
        "c2012-13.6": "[C1601]",
        #c2012-.14
        "c2012-14.4": "[C1701]",
        #c2012-.15
        "c2012-15.1": "[C1807]",
        "c2012-15.2": "[C1806]",
        "c2012-15.3": "[C1805]",
        "c2012-15.4": "[C1804]",
        "c2012-15.5": "[C1803]",
        "c2012-15.6": "[C1802]",
        "c2012-15.7": "[C1801]",
        #c2012-.16
        "c2012-16.1": "[C1907]",
        "c2012-16.2": "[C1906]",
        "c2012-16.3": "[C1905]",
        "c2012-16.4": "[C1904]",
        "c2012-16.5": "[C1903]",
        "c2012-16.6": "[C1902]",
        "c2012-16.7": "[C1901]",
        #c2012-.17
        "c2012-17.1": "[C1508]",
        "c2012-17.4": "[C1505]",
        "c2012-17.6": "[C1503]",
        "c2012-17.7": "[C1502]",
        "c2012-17.8": "[C1501]",
        #c2012-.18
        "c2012-18.4": "[C1305]",
        "c2012-18.5": "[C1304]",
        "c2012-18.7": "[C1302]",
        "c2012-18.8": "[C1301]",
        #c2012-.19
        "c2012-19.2": "[C0301]",
        #c2012-.20
        "c2012-20.1": "[C0114]",
        "c2012-20.2": "[C0113]",
        "c2012-20.3": "[C0112]",
        "c2012-20.4": "[C0111]",
        "c2012-20.5": "[C0110]",
        "c2012-20.6": "[C0109]",
        "c2012-20.7": "[C0108]",
        "c2012-20.8": "[C0107]",
        "c2012-20.9": "[C0106]",
        "c2012-20.10": "[C0105]",
        "c2012-20.11": "[C0104]",
        "c2012-20.12": "[C0103]",
        "c2012-20.13": "[C0102]",
        "c2012-20.14": "[C0101]",
        #c2012-.21
        "c2012-21.1": "[C0420]",
        "c2012-21.2": "[C0419]",
        "c2012-21.3": "[C0418]",
        "c2012-21.4": "[C0417]",
        "c2012-21.5": "[C0416]",
        "c2012-21.6": "[C0415]",
        "c2012-21.7": "[C0414]",
        "c2012-21.8": "[C0413]",
        "c2012-21.9": "[C0412]",
        "c2012-21.10": "[C0411]",
        "c2012-21.11": "[C0410]",
        "c2012-21.12": "[C0409]",
        "c2012-21.15": "[C0406]",
        "c2012-21.16": "[C0405]",
        "c2012-21.21": "[C0421]",
    }
    def __init__(self, path, line_num, err_msg, optional_flag = False, other_locations = None, error_numbers = None, external_message = None):
        ns_tag = ""
        if err_msg in self.rule_trans_map:
            ns_tag = self.rule_trans_map[err_msg]
        self.path = path
        self.line_number = line_num
        if optional_flag:
            self.error_message = f'{ns_tag}[misra-{err_msg}]: {err_msg} (optional)'
        else:
            self.error_message = f'{ns_tag}[misra-{err_msg}]: {err_msg}'
        self.locations = [ErrorLocation(path, line_num)]
        if error_numbers and len(error_numbers) == 3:
            num1, num2, case_number = error_numbers
            self.error_kind = int(f"1{num1:02}{num2:02}{case_number:02}")
        if other_locations is not None:
            for loc in other_locations:
                self.locations.append(ErrorLocation(loc.file, loc.linenr))
        if not external_message or external_message == '':
            self.external_message = None
        else:
            self.external_message = external_message

class NaiveSystemsResult:
    rule_trans_map = {
        "C7966":"[C7966]",
    }
    def __init__(self, path, line_num, err_msg, other_locations = None):
        ns_tag = ""
        if err_msg in self.rule_trans_map:
            ns_tag = self.rule_trans_map[err_msg]
        self.path = path
        self.line_number = line_num
        self.error_message = f'{ns_tag}[NAIVESYSTEMS-{err_msg}]: {err_msg}'
        self.locations = [ErrorLocation(path, line_num)]
        if other_locations is not None:
            for loc in other_locations:
                self.locations.append(ErrorLocation(loc.file, loc.linenr))

class CXXResult:
    rule_trans_map = {
        # cxx2008
        "cxx2008-2.7.1": "[CXX_2_7_1]",
        "cxx2008-3.3.2": "[CXX_3_3_2]",
        "cxx2008-3.9.3": "[CXX_3_9_3]",
        "cxx2008-5.0.20": "[CXX_5_0_20]",
        "cxx2008-5.0.21": "[CXX_5_0_21]",
        "cxx2008-6.2.3": "[CXX_6_2_3]",
        "cxx2008-6.3.1": "[CXX_6_3_1]",
        "cxx2008-6.4.1": "[CXX_6_4_1]",
        "cxx2008-6.4.5": "[CXX_6_4_5]",
        "cxx2008-6.4.6": "[CXX_6_4_6]",
        "cxx2008-6.4.8": "[CXX_6_4_8]",
        "cxx2008-7.4.2": "[CXX_7_4_2]",
        "cxx2008-8.5.3": "[CXX_8_5_3]",
        "cxx2008-16.0.1": "[CXX_16_0_1]",
        "cxx2008-16.0.2": "[CXX_16_0_2]",
        "cxx2008-16.0.8": "[CXX_16_0_8]",
        "cxx2008-16.1.1": "[CXX_16_1_1]",
        "cxx2008-16.2.1": "[CXX_16_2_1]",
        "cxx2008-16.2.2": "[CXX_16_2_2]",
        "cxx2008-16.2.4": "[CXX_16_2_4]",
        "cxx2008-16.3.1": "[CXX_16_3_1]",
        "cxx2008-17.0.3": "[CXX_17_0_3]",
        "cxx2008-18.0.1": "[CXX_18_0_1]",
        "cxx2008-27.0.1": "[CXX_27_0_1]",
    }
    def __init__(self, path, line_num, err_msg, other_locations = None):
        ns_tag = ""
        if err_msg in self.rule_trans_map:
            ns_tag = self.rule_trans_map[err_msg]
        self.path = path
        self.line_number = line_num
        self.error_message = f'{ns_tag}[CXX-{err_msg}]: {err_msg}'
        self.locations = [ErrorLocation(path, line_num)]
        if other_locations is not None:
            for loc in other_locations:
                self.locations.append(ErrorLocation(loc.file, loc.linenr))

class GJBResult:
    rule_trans_map = {
        # GJB5369
        "5369-4.1.1.9": "[GJB5369_1_1_9]",
        "5369-4.2.1.3": "[GJB5369_2_1_3]",
        "5369-4.6.1.3": "[GJB5369_6_1_3]",
        "5369-4.1.1.5": "[GJB5369_1_1_5]",
        "5369-4.6.1.5": "[GJB5369_6_1_5]",
        "5369-4.6.1.7": "[GJB5369_6_1_7]",
        "5369-4.6.1.10": "[GJB5369_6_1_10]",
        "5369-4.6.1.11": "[GJB5369_6_1_11]",
        "5369-4.6.1.17": "[GJB5369_6_1_17]",
        "5369-4.6.1.18": "[GJB5369_6_1_18]",
        "5369-4.7.1.1": "[GJB5369_7_1_1]",
        "5369-4.7.1.2": "[GJB5369_7_1_2]",
        "5369-4.7.1.3": "[GJB5369_7_1_3]",
        "5369-4.7.1.8": "[GJB5369_7_1_8]",
        "5369-4.8.1.3": "[GJB5369_8_1_3]",
        "5369-4.10.1.1": "[GJB5369_10_1_1]",
        "5369-4.13.1.3": "[GJB5369_13_1_3]",
        "5369-4.14.1.1": "[GJB5369_14_1_1]",
        "5369-4.15.1.1": "[GJB5369_15_1_1]",
        "5369-4.15.1.2": "[GJB5369_15_1_2]",
        "5369-4.15.1.5": "[GJB5369_15_1_5]",
        # GJB8114
        "8114-R-1-1-6": "[GJB8114_1_1_6]",
        "8114-R-1-1-7": "[GJB8114_1_1_7]",
        "8114-R-1-1-17": "[GJB8114_1_1_17]",
        "8114-R-1-1-19": "[GJB8114_1_1_19]",
        "8114-R-1-1-22": "[GJB8114_1_1_22]",
        "8114-R-1-1-23": "[GJB8114_1_1_23]",
        "8114-R-1-4-2": "[GJB8114_1_4_2]",
        "8114-R-1-4-5": "[GJB8114_1_4_5]",
        "8114-R-1-4-6": "[GJB8114_1_4_6]",
        "8114-R-1-4-7": "[GJB8114_1_4_7]",
        "8114-R-1-6-7": "[GJB8114_1_6_7]",
        "8114-R-1-6-9": "[GJB8114_1_6_9]",
        "8114-R-1-6-18": "[GJB8114_1_6_18]",
        "8114-R-1-7-11": "[GJB8114_1_7_11]",
        "8114-R-1-7-12": "[GJB8114_1_7_12]",
        "8114-R-1-8-4": "[GJB8114_1_8_4]",
        "8114-R-1-8-5": "[GJB8114_1_8_5]"
    }
    def __init__(self, path, line_num, err_msg, other_locations = None):
        ns_tag = ""
        if err_msg in self.rule_trans_map:
            ns_tag = self.rule_trans_map[err_msg]
        self.path = path
        self.line_number = line_num
        self.error_message = f'{ns_tag}[GJB-{err_msg}]: {err_msg}'
        self.locations = [ErrorLocation(path, line_num)]
        if other_locations is not None:
            for loc in other_locations:
                self.locations.append(ErrorLocation(loc.file, loc.linenr))

class CWEResult:
    def __init__(self, path, line_num, err_msg, other_locations = None):
        self.path = path
        self.line_number = line_num
        self.error_message = f'{err_msg}'
        self.locations = [ErrorLocation(path, line_num)]
        if other_locations is not None:
            for loc in other_locations:
                self.locations.append(ErrorLocation(loc.file, loc.linenr))

class GoogleCPPResult:
    def __init__(self, path, line_num, err_msg, other_locations = None):
        self.path = path
        self.line_number = line_num
        self.error_message = f'{err_msg}'
        self.locations = [ErrorLocation(path, line_num)]
        if other_locations is not None:
            for loc in other_locations:
                self.locations.append(ErrorLocation(loc.file, loc.linenr))

class MisraSettings(object):
    """Hold settings for misra.py script."""

    __slots__ = ["verify", "quiet", "show_summary"]

    def __init__(self, args):
        """
        :param args: Arguments given by argparse.
        """
        self.verify = False
        self.quiet = False
        self.show_summary = True

        if args.verify:
            self.verify = True
        if args.cli:
            self.quiet = True
            self.show_summary = False
        if args.quiet:
            self.quiet = True
        if args.no_summary:
            self.show_summary = False

    def __repr__(self):
        attrs = ["verify", "quiet", "show_summary", "verify"]
        return "{}({})".format(
            "MisraSettings",
            ", ".join(("{}={}".format(a, repr(getattr(self, a))) for a in attrs))
        )


class MisraChecker:

    def __init__(self, settings, stdversion="c89"):
        """
        :param settings: misra.py script settings.
        """

        self.settings = settings

        # Test validation rules lists
        self.verify_expected = list()
        self.verify_actual = list()

        # List of formatted violation messages
        self.violations = dict()

        # if --rule-texts is specified this dictionary
        # is loaded with descriptions of each rule
        # by rule number (in hundreds).
        # ie rule 1.2 becomes 102
        self.ruleTexts = dict()

        # Dictionary of dictionaries for rules to suppress
        # Dict1 is keyed by rule number in the hundreds format of
        # Major *  100 + minor. ie Rule 5.2 = (5*100) + 2
        # Dict 2 is keyed by filename.  An entry of None means suppress globally.
        # Each file name entry contains a list of tuples of (lineNumber, symbolName)
        # or an item of None which indicates suppress rule for the entire file.
        # The line and symbol name tuple may have None as either of its elements but
        # should not be None for both.
        self.suppressedRules = dict()

        # Prefix to ignore when matching suppression files.
        self.filePrefix = None

        # Number of all violations suppressed per rule
        self.suppressionStats = dict()

        self.stdversion = stdversion

        self.severity = None

        self.existing_violations = set()

        self._ctu_summary_typedefs = False
        self._ctu_summary_tagnames = False
        self._ctu_summary_identifiers = False
        self._ctu_summary_usage = False
        self.current_json_list = []

    def __repr__(self):
        attrs = ["settings", "verify_expected", "verify_actual", "violations",
                 "ruleTexts", "suppressedRules", "filePrefix",
                 "suppressionStats", "stdversion", "severity"]
        return "{}({})".format(
            "MisraChecker",
            ", ".join(("{}={}".format(a, repr(getattr(self, a))) for a in attrs))
        )

    def get_num_significant_naming_chars(self, cfg):
        if cfg.standards and cfg.standards.c == "c89":
            return 31
        else:
            return 63

    def _save_ctu_summary_typedefs(self, dumpfile, typedef_info):
        if self._ctu_summary_typedefs:
            return

        self._ctu_summary_typedefs = True

        summary = []
        for ti in typedef_info:
            summary.append({ 'name': ti.name, 'file': ti.file, 'line': ti.linenr, 'column': ti.column, 'used': ti.used })
        if len(summary) > 0:
            cppcheckdata.reportSummary(dumpfile, 'MisraTypedefInfo', summary)

    def _save_ctu_summary_tagnames(self, dumpfile, cfg):
        if self._ctu_summary_tagnames:
            return

        self._ctu_summary_tagnames = True

        summary = []
        # structs/enums
        for scope in cfg.scopes:
            if scope.className is None:
                continue
            if scope.type not in ('Struct', 'Enum'):
                continue
            used = False
            tok = scope.bodyEnd
            while tok:
                if tok.str == scope.className and \
                    (tok.previous and tok.previous.previous and \
                    tok.previous.previous.str != "typedef"):
                    used = True
                    break
                tok = tok.next
            summary.append({'name': scope.className, 'used':used, 'file': scope.bodyStart.file, 'line': scope.bodyStart.linenr, 'column': scope.bodyStart.column})
        if len(summary) > 0:
            cppcheckdata.reportSummary(dumpfile, 'MisraTagName', summary)

    def _save_ctu_summary_identifiers(self, dumpfile, cfg):
        if self._ctu_summary_identifiers:
            return
        self._ctu_summary_identifiers = True

        external_identifiers = []
        internal_identifiers = []
        local_identifiers = []

        def identifier(nameToken):
            return {'name':nameToken.str, 'file':nameToken.file, 'line':nameToken.linenr, 'column':nameToken.column}

        names = []

        for var in cfg.variables:
            if var.nameToken is None:
                continue
            if var.access != 'Global':
                if var.nameToken.str in names:
                    continue
                names.append(var.nameToken.str)
                local_identifiers.append(identifier(var.nameToken))
            elif var.isStatic:
                names.append(var.nameToken.str)
                internal_identifiers.append(identifier(var.nameToken))
            else:
                names.append(var.nameToken.str)
                i = identifier(var.nameToken)
                i['decl'] = var.isExtern
                external_identifiers.append(i)

        for func in cfg.functions:
            if func.tokenDef is None:
                continue
            if func.isStatic:
                internal_identifiers.append(identifier(func.tokenDef))
            else:
                i = identifier(func.tokenDef)
                i['decl'] = func.token is None
                external_identifiers.append(i)

        cppcheckdata.reportSummary(dumpfile, 'MisraExternalIdentifiers', external_identifiers)
        cppcheckdata.reportSummary(dumpfile, 'MisraInternalIdentifiers', internal_identifiers)
        cppcheckdata.reportSummary(dumpfile, 'MisraLocalIdentifiers', local_identifiers)

    def _save_ctu_summary_usage(self, dumpfile, cfg):
        if self._ctu_summary_usage:
            return
        self._ctu_summary_usage = True

        names = []
        for token in cfg.tokenlist:
            if not token.isName:
                continue
            if token.function and token.scope.isExecutable:
                if (not token.function.isStatic) and (token.str not in names):
                    names.append(token.str)
            elif token.variable:
                if token == token.variable.nameToken:
                    continue
                if token.variable.access == 'Global' and (not token.variable.isStatic) and (token.str not in names):
                    names.append(token.str)

        if len(names) > 0:
            cppcheckdata.reportSummary(dumpfile, 'MisraUsage', names)


    def misra_1_4(self, cfg, rawTokens):
        for token in rawTokens:
            if token.str in ('<stdnoreturn.h>', '<stdalign.h>'):
                self.reportError(token, 1, 4)
        for token in cfg.tokenlist:
            if token.str in ('_Atomic', '_Noreturn', '_Generic', '_Thread_local', '_Alignas', '_Alignof', '__alignas_is_defined', '__alignof_is_defined'):
                self.reportError(token, 1, 4)
            if token.str.endswith('_s') and isFunctionCall(token.next):
                # See C specification C11 - Annex K, page 578
                if token.str in ('tmpfile_s', 'tmpnam_s', 'fopen_s', 'freopen_s', 'fprintf_s', 'fscanf_s', 'printf_s', 'scanf_s',
                                 'snprintf_s', 'sprintf_s', 'sscanf_s', 'vfprintf_s', 'vfscanf_s', 'vprintf_s', 'vscanf_s',
                                 'vsnprintf_s', 'vsprintf_s', 'vsscanf_s', 'gets_s', 'set_constraint_handler_s', 'abort_handler_s',
                                 'ignore_handler_s', 'getenv_s', 'bsearch_s', 'qsort_s', 'wctomb_s', 'mbstowcs_s', 'wcstombs_s',
                                 'memcpy_s', 'memmove_s', 'strcpy_s', 'strncpy_s', 'strcat_s', 'strncat_s', 'strtok_s', 'memset_s',
                                 'strerror_s', 'strerrorlen_s', 'strnlen_s', 'asctime_s', 'ctime_s', 'gmtime_s', 'localtime_s',
                                 'fwprintf_s', 'fwscanf_s', 'snwprintf_s', 'swprintf_s', 'swscanf_s', 'vfwprintf_s', 'vfwscanf_s',
                                 'vsnwprintf_s', 'vswprintf_s', 'vswscanf_s', 'vwprintf_s', 'vwscanf_s', 'wprintf_s', 'wscanf_s',
                                 'wcscpy_s', 'wcsncpy_s', 'wmemcpy_s', 'wmemmove_s', 'wcscat_s', 'wcsncat_s', 'wcstok_s', 'wcsnlen_s',
                                 'wcrtomb_s', 'mbsrtowcs_s', 'wcsrtombs_s'):
                    self.reportError(token, 1, 4)

    def misra_2_3(self, dumpfile, typedefInfo):
        self._save_ctu_summary_typedefs(dumpfile, typedefInfo)

    def misra_2_4(self, dumpfile, cfg):
        self._save_ctu_summary_tagnames(dumpfile, cfg)

    def misra_2_5(self, dumpfile, cfg):
        ifndef_re = re.compile(r'#ifndef[ \t]+([a-zA-Z_][a-zA-Z_0-9]*)$')
        define_re = re.compile(r'#define[ \t]+([a-zA-Z_][a-zA-Z_0-9]*)$')
        # recorder for #ifndef names
        ifndef_name_by_file = dict()

        used_macros = list()
        for m in cfg.macro_usage:
            used_macros.append(m.name)
        for directive in cfg.directives:
            res = re.match(r'#undef[ \t]+([a-zA-Z_][a-zA-Z_0-9]*).*', directive.str)
            if res:
                used_macros.append(res.group(1))
        summary = []
        for directive in cfg.directives:
            res = re.match(r'#define[ \t]+([a-zA-Z_][a-zA-Z_0-9]*).*', directive.str)
            if res:
                macro_name = res.group(1)
                # The default 'in_include_guards' is False.
                # It will turn to True if the #define macro is in include guards.
                summary.append({'name': macro_name, 'in_include_guards': False, 'used': (macro_name in used_macros), 'file': directive.file, 'line': directive.linenr, 'column': directive.column})

            if not is_header(directive.file):
                continue
            # check include guards:
                # For compliant include guards:
                # a. There must be #ifndef - #define - #endif pairs.
                # b. Identifiers following #ifndef and #define should be the same.
                # c. There is any other processors outsides the processor pairs.
            # consider cases that the source file includes many header files
            if directive.file not in ifndef_name_by_file.keys():
                ifndef_name_by_file[directive.file] = ""
            # check the pairing of #ifndef, #define
            if directive.str.startswith('#ifndef '):
                # TODO: Check whether there are any other codes
                # (except comments) in the start of the header file.
                if ifndef_name_by_file[directive.file] != '':
                    # #ifndef exists when another #ifndef is seen
                    continue
                match = ifndef_re.match(directive.str)
                if not match:
                    continue
                ifndef_name_by_file[directive.file] = match.group(1)
            elif directive.str.startswith('#define '):
                if ifndef_name_by_file[directive.file] == '':
                    # #ifndef does not exist when #define is seen
                    continue
                match = define_re.match(directive.str)
                if not match:
                    continue
                name = match.group(1)
                if name != ifndef_name_by_file[directive.file]:
                    # #ifndef and #define names are not same
                    continue
                ifndef_name_by_file[directive.file] = ''
                # the #define directive is in include guards
                if summary:
                    summary[-1]['in_include_guards'] = True

        if len(summary) > 0:
            cppcheckdata.reportSummary(dumpfile, 'MisraMacro', summary)

    def misra_2_6(self, cfg):
        def match_label(tok):
            if tok is None or tok.str not in ("{", "}", ";"):
                return False
            tok = tok.next
            if tok is None or not tok.isName:
                return False
            tok = tok.next
            return tok is not None and tok.str == ":"

        def match_goto(start, label, end):
            tok = start
            while tok and tok != end:
                if tok.str == "goto" and tok.next and tok.next.str == label:
                    return True
                tok = tok.next
            return False

        for scope in cfg.scopes:
            tok = scope.bodyStart
            while tok is not None:
                if not tok.scope.isExecutable:
                    tok = tok.scope.bodyEnd
                    if tok is None:
                        break
                label = tok.next.str if tok and tok.next is not None else ""
                if match_label(tok) and label != "default":
                    start, end = scope.bodyStart.next, scope.bodyEnd.previous
                    if not match_goto(start, label, end):
                        self.reportError(tok, 2, 6)
                tok = tok.next

    def misra_2_7(self, data):
        for func in data.functions:
            # Skip function with no parameter
            if len(func.argument) == 0:
                continue
            # Setup list of function parameters
            func_param_list = list()
            for arg in func.argument:
                func_arg = func.argument[arg]
                if func_arg.typeStartToken and func_arg.typeStartToken.str == '...':
                    continue
                func_param_list.append(func_arg)
            # Search for scope of current function
            for scope in data.scopes:
                if (scope.type == "Function") and (scope.function == func):
                    # Search function body: remove referenced function parameter from list
                    token = scope.bodyStart
                    while token.next is not None and token != scope.bodyEnd and len(func_param_list) > 0:
                        if token.variable is not None and token.variable in func_param_list:
                            func_param_list.remove(token.variable)
                        token = token.next
                    # Emit a warning for each unused variable, but no more that one warning per line
                    reported_linenrs = set()
                    for func_param in func_param_list:
                        if func_param.nameToken:
                            linenr = func_param.nameToken
                            if linenr not in reported_linenrs:
                                self.reportError(func_param.nameToken, 2, 7)
                                reported_linenrs.add(linenr)
                        else:
                            linenr = func.tokenDef.linenr
                            if linenr not in reported_linenrs:
                                self.reportError(func.tokenDef, 2, 7)
                                reported_linenrs.add(linenr)

    def misra_3_1(self, rawTokens):
        for token in rawTokens:
            starts_with_double_slash = token.str.startswith('//')
            if token.str.startswith('/*') or starts_with_double_slash:
                s = token.str.lstrip('/')
                if ((not starts_with_double_slash) and '//' in s) or '/*' in s:
                    self.reportError(token, 3, 1)

    def misra_3_2(self, rawTokens):
        for token in rawTokens:
            if token.str.startswith('//'):
                # Check for comment ends with trigraph which might be replaced
                # by a backslash.
                if token.str.endswith('??/'):
                    self.reportError(token, 3, 2)
                # Check for comment which has been merged with subsequent line
                # because it ends with backslash.
                # The last backslash is no more part of the comment token thus
                # check if next token exists and compare line numbers.
                elif (token.next is not None) and (token.linenr == token.next.linenr):
                    self.reportError(token, 3, 2)

    def misra_4_1(self, rawTokens):
        for token in rawTokens:
            if (token.str[0] != '"') and (token.str[0] != '\''):
                continue
            if len(token.str) < 3:
                continue

            delimiter = token.str[0]
            symbols = token.str[1:-1]

            # No closing delimiter. This will not compile.
            if token.str[-1] != delimiter:
                continue

            if len(symbols) < 2:
                continue

            if not hasNumericEscapeSequence(symbols):
                continue

            # String literals that contains one or more escape sequences. All of them should be
            # terminated.
            for sequence in ['\\' + t for t in symbols.split('\\')][1:]:
                if (isHexEscapeSequence(sequence) or isOctalEscapeSequence(sequence) or
                        isSimpleEscapeSequence(sequence)):
                    continue
                else:
                    self.reportError(token, 4, 1)

    def misra_4_2(self, rawTokens):
        for token in rawTokens:
            if (token.str[0] != '"') or (token.str[-1] != '"'):
                continue
            # Check for trigraph sequence as defined by ISO/IEC 9899:1999
            for sequence in ['??=', '??(', '??/', '??)', '??\'', '??<', '??!', '??>', '??-']:
                if sequence in token.str[1:-1]:
                    # First trigraph sequence match, report error and leave loop.
                    self.reportError(token, 4, 2)
                    break

    def misra_5_1(self, data):
        long_vars = {}
        num_sign_chars = self.get_num_significant_naming_chars(data)
        for var in data.variables:
            if var.nameToken is None:
                continue
            if len(var.nameToken.str) <= num_sign_chars:
                continue
            if not hasExternalLinkage(var):
                continue
            long_vars.setdefault(var.nameToken.str[:num_sign_chars], []).append(var.nameToken)
        for name_prefix in long_vars:
            tokens = long_vars[name_prefix]
            if len(tokens) < 2:
                continue
            for tok in sorted(tokens, key=lambda t: (t.linenr, t.column))[1:]:
                self.reportError(tok, 5, 1)

    def misra_5_2(self, data):
        scopeVars = {}
        num_sign_chars = self.get_num_significant_naming_chars(data)
        for var in data.variables:
            if var.nameToken is None or var.scope is None:
                continue
            if var.scope not in scopeVars:
                scopeVars.setdefault(var.scope, {})["varlist"] = []
                scopeVars.setdefault(var.scope, {})["scopelist"] = []
            scopeVars[var.scope]["varlist"].append(var)
        for scope in data.scopes:
            if scope.nestedIn and scope.className:
                if scope.nestedIn not in scopeVars:
                    scopeVars.setdefault(scope.nestedIn, {})["varlist"] = []
                    scopeVars.setdefault(scope.nestedIn, {})["scopelist"] = []
                scopeVars[scope.nestedIn]["scopelist"].append(scope)
        for scope in scopeVars:
            if len(scopeVars[scope]["varlist"]) <= 1:
                continue
            for i, variable1 in enumerate(scopeVars[scope]["varlist"]):
                for variable2 in scopeVars[scope]["varlist"][i + 1:]:
                    if variable1.isArgument and variable2.isArgument:
                        continue
                    if hasExternalLinkage(variable1) and hasExternalLinkage(variable2):
                        continue
                    if (variable1.nameToken.str[:num_sign_chars] == variable2.nameToken.str[:num_sign_chars] and
                            variable1 is not variable2):
                        if int(variable1.nameToken.linenr) > int(variable2.nameToken.linenr):
                            self.reportError(variable1.nameToken, 5, 2, other_locations = [variable2.nameToken])
                        else:
                            self.reportError(variable2.nameToken, 5, 2, other_locations = [variable1.nameToken])
                for innerscope in scopeVars[scope]["scopelist"]:
                    if variable1.nameToken.str[:num_sign_chars] == innerscope.className[:num_sign_chars]:
                        if int(variable1.nameToken.linenr) > int(innerscope.bodyStart.linenr):
                            self.reportError(variable1.nameToken, 5, 2, other_locations = [innerscope.bodyStart])
                        else:
                            self.reportError(innerscope.bodyStart, 5, 2, other_locations = [variable1.nameToken])
            if len(scopeVars[scope]["scopelist"]) <= 1:
                continue
            for i, scopename1 in enumerate(scopeVars[scope]["scopelist"]):
                for scopename2 in scopeVars[scope]["scopelist"][i + 1:]:
                    if scopename1.className[:num_sign_chars] == scopename2.className[:num_sign_chars]:
                        if int(scopename1.bodyStart.linenr) > int(scopename2.bodyStart.linenr):
                            self.reportError(scopename1.bodyStart, 5, 2, other_locations = [scopename2.bodyStart])
                        else:
                            self.reportError(scopename2.bodyStart, 5, 2, other_locations = [scopename1.bodyStart])

    def addInnerScope(self, scope, scopeVars, visited):
        if scope in visited:
            return
        if scope not in scopeVars:
            visited.add(scope)
            return
        for innerscope in scopeVars[scope]["scopelist"]:
            if innerscope not in visited:
                self.addInnerScope(innerscope, scopeVars, visited)
            if innerscope in scopeVars:
                scopeVars[scope]["scopelist"] = scopeVars[scope]["scopelist"].union(scopeVars[innerscope]["scopelist"])
        visited.add(scope)

    def prepareScopeVars(self, data):
        scopeVars = {}
        for var in data.variables:
            if var.nameToken is None:
                continue
            if var.scope and var.scope.type == "Struct":
                continue
            if var.isArgument:
                var_scope = var.scope
            else:
                var_scope = var.nameToken.scope
            if var_scope not in scopeVars:
                scopeVars.setdefault(var_scope, {})["namelist"] = set()
                scopeVars.setdefault(var_scope, {})["scopelist"] = set()
            scopeVars[var_scope]["namelist"].add(var)
        for typedef in data.typedefInfo:
            if not typedef.nameToken or typedef.nameToken.file != typedef.file:
                continue
            typedef_scope = typedef.nameToken.scope
            if typedef_scope not in scopeVars:
                scopeVars.setdefault(typedef_scope, {})["namelist"] = set()
                scopeVars.setdefault(typedef_scope, {})["scopelist"] = set()
            scopeVars[typedef_scope]["namelist"].add(typedef)
        for scope in data.scopes:
            if scope.nestedIn:
                if scope.nestedIn not in scopeVars:
                    scopeVars.setdefault(scope.nestedIn, {})["namelist"] = set()
                    scopeVars.setdefault(scope.nestedIn, {})["scopelist"] = set()
                scopeVars[scope.nestedIn]["scopelist"].add(scope)
        visited = set()
        for scope in scopeVars:
            self.addInnerScope(scope, scopeVars, visited)
        return scopeVars

    def misra_5_3(self, data):
        scopeVars = self.prepareScopeVars(data)
        num_sign_chars = self.get_num_significant_naming_chars(data)
        for scope in scopeVars:
            # split rules will be added here
            for innerscope in scopeVars[scope]["scopelist"]:
                if innerscope not in scopeVars:
                    continue
                for element1 in scopeVars[scope]["namelist"]:
                    for element2 in scopeVars[innerscope]["namelist"]:
                        if element1.nameToken.str[:num_sign_chars] == element2.nameToken.str[:num_sign_chars]:
                            if int(element1.nameToken.linenr) > int(element2.nameToken.linenr):
                                self.reportError(element1.nameToken, 5, 3, other_locations = [element2.nameToken])
                            else:
                                self.reportError(element2.nameToken, 5, 3, other_locations = [element1.nameToken])

    def misra_5_4(self, data):
        num_sign_chars = self.get_num_significant_naming_chars(data)
        macro = {}
        compile_name = re.compile(r'#define ([a-zA-Z0-9_]+)')
        compile_param = re.compile(r'#define ([a-zA-Z0-9_]+)[(]([a-zA-Z0-9_, ]+)[)]')
        short_names = {}
        macro_w_arg = []
        for dir in data.directives:
            res1 = compile_name.match(dir.str)
            if res1:
                if dir not in macro:
                    macro.setdefault(dir, {})["name"] = []
                    macro.setdefault(dir, {})["params"] = []
                full_name = res1.group(1)
                macro[dir]["name"] = full_name
                short_name = full_name[:num_sign_chars]
                if short_name in short_names:
                    _dir = short_names[short_name]
                    if full_name != macro[_dir]["name"]:
                        self.reportError(dir, 5, 4)
                else:
                    short_names[short_name] = dir
            res2 = compile_param.match(dir.str)
            if res2:
                res_gp2 = res2.group(2).split(",")
                res_gp2 = [macroname.replace(" ", "") for macroname in res_gp2]
                macro[dir]["params"].extend(res_gp2)
                macro_w_arg.append(dir)
        for mvar in macro_w_arg:
            for i, macroparam1 in enumerate(macro[mvar]["params"]):
                for j, macroparam2 in enumerate(macro[mvar]["params"]):
                    if j > i and macroparam1[:num_sign_chars] == macroparam2[:num_sign_chars]:
                        self.reportError(mvar, 5, 4)
                param = macroparam1
                if param[:num_sign_chars] in short_names:
                    m_var1 = short_names[param[:num_sign_chars]]
                    if m_var1.linenr > mvar.linenr:
                        self.reportError(m_var1, 5, 4, other_locations = [mvar])
                    else:
                        self.reportError(mvar, 5, 4, other_locations = [m_var1])

    def misra_5_5(self, data):
        num_sign_chars = self.get_num_significant_naming_chars(data)
        macroNames = {}
        compiled = re.compile(r'#define ([A-Za-z0-9_]+)')
        for dir in data.directives:
            res = compiled.match(dir.str)
            if res:
                macroNames[res.group(1)[:num_sign_chars]] = dir
        for var in data.variables:
            if var.nameToken and var.nameToken.str[:num_sign_chars] in macroNames:
                self.reportError(var.nameToken, 5, 5)
        for scope in data.scopes:
            if scope.className and scope.className[:num_sign_chars] in macroNames:
                self.reportError(scope.bodyStart, 5, 5)


    def misra_5_6(self, dumpfile, typedefInfo):
        self._save_ctu_summary_typedefs(dumpfile, typedefInfo)

    def misra_5_7(self, dumpfile, cfg):
        self._save_ctu_summary_tagnames(dumpfile, cfg)

    def misra_5_8(self, dumpfile, cfg):
        self._save_ctu_summary_identifiers(dumpfile, cfg)

    def misra_5_9(self, dumpfile, cfg):
        self._save_ctu_summary_identifiers(dumpfile, cfg)

    def misra_6_1(self, data):
        # Bitfield type must be bool or explicitly signed/unsigned int
        for token in data.tokenlist:
            if not token.valueType:
                continue
            if token.valueType.bits == 0:
                continue
            if not token.variable:
                continue
            if not token.scope:
                continue
            if token.scope.type not in 'Struct':
                continue

            if data.standards.c == 'c89':
                if token.valueType.type != 'int' and  not isUnsignedType(token.variable.typeStartToken.str):
                    self.reportError(token, 6, 1)
            elif data.standards.c == 'c99':
                if token.valueType.type == 'bool':
                    continue

            isExplicitlySignedOrUnsigned = False
            typeToken = token.variable.typeStartToken
            while typeToken:
                if typeToken.isUnsigned or typeToken.isSigned or isUnsignedType(typeToken.str):
                    isExplicitlySignedOrUnsigned = True
                    break

                if typeToken is token.variable.typeEndToken:
                    break

                typeToken = typeToken.next

            if not isExplicitlySignedOrUnsigned:
                self.reportError(token, 6, 1)


    def misra_6_2(self, data):
        # Bitfields of size 1 can not be signed
        for token in data.tokenlist:
            if not token.valueType:
                continue
            if not token.scope:
                continue
            if token.scope.type not in 'Struct':
                continue
            if token.valueType.bits == 1 and token.valueType.sign == 'signed':
                self.reportError(token, 6, 2)


    def misra_7_1(self, rawTokens):
        compiled = re.compile(r'^0[0-7]+$')
        for tok in rawTokens:
            if compiled.match(tok.str):
                self.reportError(tok, 7, 1)

    def misra_7_2(self, data):
        for token in data.tokenlist:
            if token.isInt and ('U' not in token.str.upper()) and token.valueType and token.valueType.sign == 'unsigned':
                self.reportError(token, 7, 2)

    def misra_7_3(self, rawTokens):
        compiled = re.compile(r'^[0-9.]+[Uu]*l+[Uu]*$')
        for tok in rawTokens:
            if compiled.match(tok.str):
                self.reportError(tok, 7, 3)

    def misra_7_4(self, data):
        def isVarTypeOk(var):
            varValueType = var.valueType
            if (varValueType.constness % 2) != 1 or varValueType.pointer == 0:
                return False
            typeTokens = [['const', 'char16_t', '*'], ['const', 'char16_t', '*', '*'], \
                        ['const', 'char32_t', '*'], ['const', 'char32_t', '*', '*']]
            for typeList in typeTokens:
                # skip the case like mystructa.name = "s";
                # where the type of "name" is const void *
                if not var.variable:
                    break
                if tokenFollowsSequence(var.variable.nameToken, typeList):
                    return True
            if varValueType.type == 'char' or varValueType.type == 'wchar_t':
                return True
            return False

        # A string literal shall not be assigned to an object unless the object's type
        # is constant pointer char.
        def reportErrorIfVariableIsNotPointerToConstQualifiedChar(variable, stringLiteral):
            # Variable should not be char or string literal
            if variable.isString or variable.isChar:
                self.reportError(stringLiteral, 7, 4)
            elif variable.valueType:
                if not isVarTypeOk(variable) and stringLiteral.isString:
                    self.reportError(stringLiteral, 7, 4)

        for token in data.tokenlist:
            if token.isString or token.isChar:
                # Check normal variable assignment
                variable = getAssignedVariableToken(token)
                # Exclude the case that variable == token
                if variable and token != variable:
                    reportErrorIfVariableIsNotPointerToConstQualifiedChar(variable, token)

                # Check use as return value
                function = getFunctionUsingReturnValue(token)
                if function:
                    # "Primitive" test since there is no info available on return value type
                    if not (tokenFollowsSequence(function.tokenDef, ['const', 'char', '*']) or \
                        tokenFollowsSequence(function.tokenDef, ['const', 'wchar_t', '*']) or \
                        tokenFollowsSequence(function.tokenDef, ['const', 'char16_t', '*']) or \
                        tokenFollowsSequence(function.tokenDef, ['const', 'char32_t', '*'])):
                        self.reportError(token, 7, 4)

            # Check use as function parameter
            if isFunctionCall(token) and token.astOperand1 and token.astOperand1.function:
                functionDeclaration = token.astOperand1.function

                if functionDeclaration.tokenDef:
                    if functionDeclaration.tokenDef is token.astOperand1:
                        # Token is not a function call, but it is the definition of the function
                        continue

                    parametersUsed = getArguments(token)
                    for i in range(len(parametersUsed)):
                        usedParameter = parametersUsed[i]
                        parameterDefinition = functionDeclaration.argument.get(i+1)

                        if usedParameter and usedParameter.isString and parameterDefinition and parameterDefinition.nameToken:
                            reportErrorIfVariableIsNotPointerToConstQualifiedChar(parameterDefinition.nameToken, usedParameter)

    def misra_8_1(self, cfg):
        for token in cfg.tokenlist:
            if token.isImplicitInt:
                self.reportError(token, 8, 1)
            if token.function:
                # if current token is a function but it has no return valueType
                if not token.next.valueType:
                    self.reportError(token, 8, 1)
                # check argument list
                for arg in token.function.argument:
                    argumentToken = token.function.argument[arg]
                    if not argumentToken.nameToken and not (argumentToken.typeStartToken and argumentToken.typeStartToken.str == "..."):
                        self.reportError(token, 8, 1)

        for typeDef in cfg.typedefInfo:
            if not typeDef.isValid:
                self.reportError(typeDef.nameToken, 8, 1)

    def misra_8_2(self, data, rawTokens):
        def getFollowingRawTokens(rawTokens, token, count):
            following =[]
            for rawToken in rawTokens:
                if (rawToken.file == token.file and
                        rawToken.linenr == token.linenr and
                        rawToken.column == token.column):
                    for _ in range(count):
                        rawToken = rawToken.next
                        # Skip comments
                        while rawToken and (rawToken.str.startswith('/*') or rawToken.str.startswith('//')):
                            rawToken = rawToken.next
                        if rawToken is None:
                            break
                        following.append(rawToken)
            return following

        # Zero arguments should be in form ( void )
        def checkZeroArguments(func, startCall, endCall):
            if (len(func.argument) == 0):
                voidArg = startCall.next
                while voidArg is not endCall:
                    if voidArg.str == 'void':
                        break
                    voidArg = voidArg.next
                if not voidArg.str == 'void':
                    if func.tokenDef.next:
                        self.reportError(func.tokenDef.next, 8, 2)
                    else:
                        self.reportError(func.tokenDef, 8, 2)

        def checkDeclarationArgumentsViolations(func, startCall, endCall):
            # Collect the tokens for the arguments in function definition
            argNameTokens = set()
            for arg in func.argument:
                argument = func.argument[arg]
                typeStartToken = argument.typeStartToken
                if typeStartToken is None:
                    continue
                nameToken = argument.nameToken
                if nameToken is None:
                    continue
                argNameTokens.add(nameToken)

            # Check if we have the same number of variables in both the
            # declaration and the definition.
            #
            # TODO: We actually need to check if the names of the arguments are
            # the same. But we can't do this because we have no links to
            # variables in the arguments in function definition in the dump file.
            foundVariables = 0
            while startCall and startCall != endCall:
                if startCall.varId:
                    foundVariables += 1
                startCall = startCall.next

            if len(argNameTokens) != foundVariables:
                if func.tokenDef.next:
                    self.reportError(func.tokenDef.next, 8, 2)
                else:
                    self.reportError(func.tokenDef, 8, 2)

        def checkDefinitionArgumentsViolations(func, startCall, endCall):
            for arg in func.argument:
                argument = func.argument[arg]
                typeStartToken = argument.typeStartToken
                if typeStartToken is None:
                    continue

                # Arguments should have a name unless variable length arg
                nameToken = argument.nameToken
                if nameToken is None and typeStartToken.str != '...':
                    self.reportError(typeStartToken, 8, 2)

                # Type declaration on next line (old style declaration list) is not allowed
                if typeStartToken.linenr > endCall.linenr:
                    self.reportError(typeStartToken, 8, 2)

        # Check arguments in function declaration
        for func in data.functions:

            # Check arguments in function definition
            tokenImpl = func.token
            if tokenImpl:
                startCall = tokenImpl.next
                if startCall is None or startCall.str != '(':
                    continue
                endCall = startCall.link
                if endCall is None or endCall.str != ')':
                    continue
                checkZeroArguments(func, startCall, endCall)
                checkDefinitionArgumentsViolations(func, startCall, endCall)

            # Check arguments in function declaration
            tokenDef = func.tokenDef
            if tokenDef:
                startCall = func.tokenDef.next
                if startCall is None or startCall.str != '(':
                    continue
                endCall = startCall.link
                if endCall is None or endCall.str != ')':
                    continue
                checkZeroArguments(func, startCall, endCall)
                if tokenImpl:
                    checkDeclarationArgumentsViolations(func, startCall, endCall)
                else:
                    # When there is no function definition, we should execute
                    # its checks for the declaration token. The point is that without
                    # a known definition we have no Function.argument list required
                    # for declaration check.
                    checkDefinitionArgumentsViolations(func, startCall, endCall)

        # Check arguments in pointer declarations
        for var in data.variables:
            if not var.isPointer:
                continue

            if var.nameToken is None:
                continue

            rawTokensFollowingPtr = getFollowingRawTokens(rawTokens, var.nameToken, 3)
            if len(rawTokensFollowingPtr) != 3:
                continue

            # Compliant:           returnType (*ptrName) ( ArgType )
            # Non-compliant:       returnType (*ptrName) ( )
            if (rawTokensFollowingPtr[0].str == ')' and
                    rawTokensFollowingPtr[1].str == '(' and
                    rawTokensFollowingPtr[2].str == ')'):
                self.reportError(var.nameToken, 8, 2)


    def misra_8_4(self, cfg):
        for func in cfg.functions:
            if func.isStatic:
                continue
            if func.token is None:
                continue
            if not is_source_file(func.token.file):
                continue
            if func.token.file != func.tokenDef.file:
                continue
            if func.tokenDef.str == 'main':
                continue
            self.reportError(func.tokenDef, 8, 4)

        extern_vars = []
        var_defs = []

        for var in cfg.variables:
            if not var.isGlobal:
                continue
            if var.isStatic:
                continue
            if var.nameToken is None:
                continue
            if var.isExtern:
                extern_vars.append(var.nameToken.str)
            else:
                var_defs.append(var.nameToken)
        for vartok in var_defs:
            if vartok.str not in extern_vars:
                self.reportError(vartok, 8, 4)

    def misra_8_5(self, dumpfile, cfg):
        self._save_ctu_summary_identifiers(dumpfile, cfg)

    def misra_8_6(self, dumpfile, cfg):
        self._save_ctu_summary_identifiers(dumpfile, cfg)

    def misra_8_7(self, dumpfile, cfg):
        self._save_ctu_summary_usage(dumpfile, cfg)

    def checkStaticFuncStmt(self, cfg, reportErrorFunc):
        # if a function is staic. it can not be extern after
        for func in cfg.functions:
            cur = func.token
            hasStatic = False
            while cur and not simpleMatch(cur, ";"):
                if func.isStatic and cur.str == "extern":
                    reportErrorFunc(cur)
                if cur.str == "static":
                    hasStatic = True
                cur = cur.previous
            if func.token and not hasStatic and func.isStatic:
                reportErrorFunc(func.token)

    def misra_8_8(self, cfg):
        def reportErrorFunc(token):
            self.reportError(token, 8, 8)

        vars = {}
        for var in cfg.variables:
            if var.access != 'Global':
                continue
            if var.nameToken is None:
                continue
            varname = var.nameToken.str
            if varname in vars:
                vars[varname].append(var)
            else:
                vars[varname] = [var]
        for varname, varlist in vars.items():
            static_var = None
            extern_var = None
            for var in varlist:
                if var.isStatic:
                    static_var = var
                elif var.isExtern:
                    extern_var = var
            if static_var and extern_var:
                reportErrorFunc(extern_var.nameToken)

        self.checkStaticFuncStmt(cfg, reportErrorFunc)

    def misra_8_9(self, cfg):
        variables = {}
        for scope in cfg.scopes:
            if scope.type != 'Function':
                continue
            variables_used_in_scope = []
            tok = scope.bodyStart
            while tok != scope.bodyEnd:
                if tok.variable and tok.variable.access == 'Global' and tok.variable.isStatic:
                    if tok.variable not in variables_used_in_scope:
                        variables_used_in_scope.append(tok.variable)
                tok = tok.next
            for var in variables_used_in_scope:
                if var in variables:
                    variables[var] += 1
                else:
                    variables[var] = 1
        for var, count in variables.items():
            if count == 1:
                self.reportError(var.nameToken, 8, 9)


    def misra_8_10(self, cfg):
        for func in cfg.functions:
            if func.isInlineKeyword and not func.isStatic:
                self.reportError(func.tokenDef, 8, 10)

    def misra_8_11(self, data):
        for var in data.variables:
            if var.isExtern and simpleMatch(var.nameToken.next, '[ ]'):
                self.reportError(var.nameToken, 8, 11)

    def misra_8_12(self, data):
        for scope in data.scopes:
            if scope.type != 'Enum':
                continue
            enum_values = []
            implicit_enum_values = []
            e_token = scope.bodyStart.next
            while e_token != scope.bodyEnd:
                if e_token.str == '(':
                    e_token = e_token.link
                    continue
                if e_token.previous.str not in ',{':
                    e_token = e_token.next
                    continue
                if e_token.isName and e_token.values and e_token.valueType and e_token.valueType.typeScope == scope:
                    token_values = [v.intvalue for v in e_token.values]
                    enum_values += token_values
                    if e_token.next.str != "=":
                        implicit_enum_values += token_values
                e_token = e_token.next
            for implicit_enum_value in implicit_enum_values:
                if enum_values.count(implicit_enum_value) != 1:
                    self.reportError(scope.bodyStart, 8, 12)

    def misra_8_14(self, rawTokens):
        for token in rawTokens:
            if token.str == 'restrict':
                self.reportError(token, 8, 14)

    def misra_9_2(self, data):
        misra_9.misra_9_x(self, data, 902)

    def misra_9_3(self, data):
        misra_9.misra_9_x(self, data, 903)

    def misra_9_4(self, data):
        misra_9.misra_9_x(self, data, 904)

    def misra_9_5(self, data, rawTokens):
        misra_9.misra_9_x(self, data, 905, rawTokens)
        for token in rawTokens:
           if simpleMatch(token, '[ ] = {'):
               self.reportError(token, 9, 5)

    def gjb5369_6_1_17(self, data):
        for token in data.tokenlist:
            if token.isOp and token.str in ('&', '|', '^'):
                for t1, t2 in itertools.product(
                        list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                        list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
                ):
                    e1 = getEssentialTypeCategory(t1)
                    e2 = getEssentialTypeCategory(t2)
                    if (e1 == "bool" and t1.varId) or (e2 == "bool" and t2.varId):
                        self.reportGJBError(token, 6, 1, 17)

    def gjb5369_6_1_18(self, data):
        for token in data.tokenlist:
            if token.isOp and token.str in ('&', '|', '^'):
                for t1, t2 in itertools.product(
                        list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                        list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
                ):
                    e1 = getEssentialTypeCategory(t1)
                    e2 = getEssentialTypeCategory(t2)
                    if (e1 == "bool" and not t1.varId) or (e2 == "bool" and not t2.varId):
                        self.reportGJBError(token, 6, 1, 18)

    def gjb5369_13_1_3(self, data):
        # This checker function is derived from misra_9.misra_9_x
        from misra_9 import InitializerParser, getElementDef
        parser = InitializerParser()
        for variable in data.variables:
            if not variable.nameToken:
                continue
            nameToken = variable.nameToken
            # Check if declaration and initialization is
            # split into two separate statements in ast.
            if nameToken.next and nameToken.next.isSplittedVarDeclEq:
                nameToken = nameToken.next.next
            # Find declarations with initializer assignment
            eq = nameToken
            while not eq.isAssignmentOp and eq.astParent:
                eq = eq.astParent
            # We are only looking for initializers
            if not eq.isAssignmentOp or eq.astOperand2.isName:
                continue
            if variable.isClass: # Unlike misra_9, we don't need to check array variables
                ed = getElementDef(nameToken, None)
                # No need to check non-arrays if valueType is missing,
                # since we can't say anything useful about the structure
                # without it.
                if ed.valueType is None and not variable.isArray:
                    continue
                parser.parseInitializer(ed, eq.astOperand2)
                if not ed.isMisra92Compliant():
                    self.reportGJBError(nameToken, 13, 1, 3)

    def misra_10_1(self, data):
        def getEnumPrefix(essentialTypeCategory):
            if essentialTypeCategory and 'enum' in essentialTypeCategory:
                return 'enum'
            return essentialTypeCategory

        for token in data.tokenlist:
            if not token.isOp and token.str not in ('[', '?', ':'):
                continue

            for t1, t2 in itertools.product(
                    list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                    list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
            ):
                e1 = getEnumPrefix(getEssentialTypeCategory(t1))
                e2 = getEnumPrefix(getEssentialTypeCategory(t2))
                if not e1 or not e2:
                    if token.str not in ('++', '--', '+', '-', '[', '!', '~'):
                        continue
                    if token.str in ('++', '--'):
                        if e1 in ('bool', 'enum'):
                            self.reportError(token, 10, 1, category = [e1])
                        elif e2 in ('bool', 'enum'):
                            self.reportError(token, 10, 1, category = [e2])
                    elif token.str in ('+', '-'):
                        if e1 in ('bool', 'enum', 'char'):
                            self.reportError(token, 10, 1, category = [e1])
                        elif token.str == '-' and isUnsignedType(e1):
                            self.reportError(token, 10, 1, category = [e1])
                    elif token.str == '[':
                        if token.link.str == ']' and e2 in ('bool', 'char', 'floating'):
                            self.reportError(token, 10, 1, category = [e2])
                    elif token.str == '!':
                        if e1 != 'bool':
                            self.reportError(token, 10, 1, category = [e1])
                    elif token.str == '~':
                        if not isUnsignedType(e1):
                            self.reportError(token, 10, 1, category = [e1])

                if token.str in ('<<', '>>'):
                    if not isUnsignedType(e1):
                        self.reportError(token, 10, 1, category = [e1, ''])
                    elif not isUnsignedType(e2) and (not token.astOperand2.isNumber or token.astOperand2.getKnownIntValue() < 0):
                        self.reportError(token, 10, 1, category = ['', e2])
                elif token.str in ('+', '-', '+=', '-='):
                    category = [e for e in (e1, e2) if e in ('bool', 'enum')]
                    if any(category):
                        self.reportError(token, 10, 1, category = category)
                elif token.str in ('*', '/', '%'):
                    category = [e for e in (e1, e2) if e in ('bool', 'enum', 'char')]
                    if any(category):
                        self.reportError(token, 10, 1, category = category)
                    if token.str == '%':
                        category = [e for e in (e1, e2) if e == 'floating']
                        if any(category):
                            self.reportError(token, 10, 1, category = category)
                elif token.str in ('>', '<', '<=', '>='):
                    category = [e for e in (e1, e2) if e == 'bool']
                    if any(category):
                        self.reportError(token, 10, 1, category = category)
                elif token.str in ('&&', '||'):
                    category = [e for e in (e1, e2) if e != 'bool']
                    if any(category):
                        self.reportError(token, 10, 1, category = category)
                elif token.str in ('&', '|', '^'):
                    category = [e for e in (e1, e2) if not isUnsignedType(e)]
                    if any(category):
                        self.reportError(token, 10, 1, category = category)
                elif token.str == '?' and token.astOperand2.str == ':':
                    if e1 != 'bool':
                        self.reportError(token, 10, 1, category = [e1, ''])

    def misra_10_2(self, data):
        def isEssentiallySignedOrUnsigned(op):
            e = getEssentialType(op)
            return e and (e.split(' ')[0] in ('unsigned', 'signed'))

        def isEssentiallyChar(op):
            if op is None:
                return False
            if op.str == '+':
                return isEssentiallyChar(op.astOperand1) or isEssentiallyChar(op.astOperand2)
            return getEssentialType(op) == 'char'

        for token in data.tokenlist:
            if token.str not in ('+', '-'):
                continue

            if (not isEssentiallyChar(token.astOperand1)) and (not isEssentiallyChar(token.astOperand2)):
                continue

            if token.str == '+':
                if isEssentiallyChar(token.astOperand1) and not isEssentiallySignedOrUnsigned(token.astOperand2):
                    self.reportError(token, 10, 2)
                if isEssentiallyChar(token.astOperand2) and not isEssentiallySignedOrUnsigned(token.astOperand1):
                    self.reportError(token, 10, 2)

            if token.str == '-':
                e1 = getEssentialType(token.astOperand1)
                if e1 and e1.split(' ')[-1] != 'char':
                    self.reportError(token, 10, 2)
                if not isEssentiallyChar(token.astOperand2) and not isEssentiallySignedOrUnsigned(token.astOperand2):
                    self.reportError(token, 10, 2)

    def get_category(self, essential_type):
        if essential_type:
            if essential_type in ('bool', 'char'):
                return essential_type
            if essential_type.split(' ')[-1] in ('float', 'double'):
                return 'floating'
            if essential_type.split(' ')[0] in ('unsigned', 'signed'):
                return essential_type.split(' ')[0]
            if 'enum' in essential_type:
                return 'enum'
        return None

    def reportErrorIfExpressionAssignedToNarrowerType(self, tok, astOperand1_essentialType, astOperand2):
        lhs = astOperand1_essentialType
        rhs = getEssentialType(astOperand2)
        if lhs is None or rhs is None:
            return
        lhs_category = self.get_category(lhs)
        rhs_category = self.get_category(rhs)

        # exclude case like cha += 1
        if tok.str in ('+=', '-='):
            if lhs_category == "char" and (rhs_category == "signed" or rhs_category == "unsigned"):
                return
        if lhs_category and rhs_category and lhs_category != rhs_category and rhs_category not in ('signed','unsigned'):
            self.reportError(tok, 10, 3)
        # deal with assign signed to unsigned
        if lhs_category == 'unsigned' and rhs_category == 'signed':
            if not isIntConstExpr(astOperand2) or (astOperand2.getKnownIntValue() and astOperand2.getKnownIntValue() < 0):
                self.reportError(tok, 10, 3)
        # deal with float
        if lhs_category == rhs_category and lhs_category == "floating":
            if relativeLengthOfEssentialFloatingType(lhs) < relativeLengthOfEssentialFloatingType(rhs):
                self.reportError(tok, 10, 3)
        #deal with char bit
        lhs_bits = 1 if lhs == 'char' else bitsOfEssentialType(lhs)
        rhs_bits = 1 if rhs == 'char' else bitsOfEssentialType(rhs)
        if lhs_bits < rhs_bits:
            self.reportError(tok, 10, 3)

    def reportErrorIfArrayElementAssignedToNarrowerType(self, ed, array_essentialType, reportErrorFunction):
        if ed.isArray:
            for child in ed.children:
                self.reportErrorIfArrayElementAssignedToNarrowerType(child, array_essentialType, reportErrorFunction)
        elif ed.isValue:
            reportErrorFunction(ed.token, array_essentialType, ed.token)

    def misra_10_3(self, cfg, reportErrorFunction=None):
        if reportErrorFunction == None:
            reportErrorFunction = self.reportErrorIfExpressionAssignedToNarrowerType
        # check initialization of array
        parser = misra_9.InitializerParser()
        for variable in cfg.variables:
            if not variable.nameToken:
                continue

            nameToken = variable.nameToken

            # Check if declaration and initialization is
            # split into two separate statements in ast.
            if nameToken.next and nameToken.next.isSplittedVarDeclEq:
                nameToken = nameToken.next.next

            # Find declarations with initializer assignment
            eq = nameToken
            while not eq.isAssignmentOp and eq.astParent:
                eq = eq.astParent

            # We are only looking for initializers
            if not eq.isAssignmentOp or eq.astOperand2.isName:
                continue

            if variable.isArray:
                ed = misra_9.getElementDef(nameToken)
                # No need to check non-arrays if valueType is missing,
                # since we can't say anything useful about the structure
                # without it.
                if ed.valueType is None and not variable.isArray:
                    continue

                parser.parseInitializer(ed, eq.astOperand2)
                astOperand1_essentialType = getEssentialType(eq.astOperand1)
                self.reportErrorIfArrayElementAssignedToNarrowerType(ed, astOperand1_essentialType, reportErrorFunction)

        for tok in cfg.tokenlist:
            # exclude the assignment in enum initialization
            if tok.isAssignmentOp and tok.scope.type != "Enum":
                reportErrorFunction(tok, getEssentialType(tok.astOperand1), tok.astOperand2)
            # Check use as return value
            function = getFunctionUsingReturnValue(tok)
            if function:
                returnEssentialType = getFunctionDefinitionReturnEssentialType(function.tokenDef.next)
                reportErrorFunction(tok, returnEssentialType, tok)
            # Check use as function parameter
            if isFunctionCall(tok) and tok.astOperand1 and tok.astOperand1.function:
                functionDeclaration = tok.astOperand1.function
                if functionDeclaration.tokenDef:
                    if functionDeclaration.tokenDef is tok.astOperand1:
                        # Token is not a function call, but it is the definition of the function
                        continue

                    parametersUsed = getArguments(tok)
                    for i in range(len(parametersUsed)):
                        usedParameter = parametersUsed[i]
                        parameterDefinition = functionDeclaration.argument.get(i+1)
                        if parameterDefinition:
                            reportErrorFunction(tok, getEssentialType(parameterDefinition.nameToken), usedParameter)

    def misra_10_4(self, data):
        op = {'+', '-', '*', '/', '%', '&', '|', '^', '+=', '-=', ':'}
        for token in data.tokenlist:
            if token.str not in op and not token.isComparisonOp:
                continue
            if not token.astOperand1 or not token.astOperand2:
                continue
            if not token.astOperand1.valueType or not token.astOperand2.valueType:
                continue
            if token.astOperand1.isComparisonOp and token.astOperand1.isComparisonOp:
                e1, e2 = getEssentialCategorylist(token.astOperand1.astOperand2, token.astOperand2.astOperand1)
            elif token.astOperand1.isComparisonOp:
                e1, e2 = getEssentialCategorylist(token.astOperand1.astOperand2, token.astOperand2)
            elif token.astOperand2.isComparisonOp:
                e1, e2 = getEssentialCategorylist(token.astOperand1, token.astOperand2.astOperand1)
            else:
                e1, e2 = getEssentialCategorylist(token.astOperand1, token.astOperand2)
            if token.str == "+=" or token.str == "+":
                if e1 == "char" and (e2 == "signed" or e2 == "unsigned"):
                    continue
                if e2 == "char" and (e1 == "signed" or e1 == "unsigned"):
                    continue
            if token.str == "-=" or token.str == "-":
                if e1 == "char" and (e2 == "signed" or e2 == "unsigned"):
                    continue
            if e1 and e2 and (e1.find('Anonymous') != -1 and (e2 == "signed" or e2 == "unsigned")):
                continue
            if e1 and e2 and (e2.find('Anonymous') != -1 and (e1 == "signed" or e1 == "unsigned")):
                continue
            if e1 and e2 and e1 != e2:
                self.reportError(token, 10, 4)

    def misra_10_5(self, cfg):
        def _get_essential_category(token):
            essential_type = getEssentialType(token)
            #print(essential_type)
            if essential_type:
                if essential_type in ('bool', 'char'):
                    return essential_type
                if essential_type.split(' ')[-1] in ('float', 'double'):
                    return 'floating'
                if essential_type.split(' ')[0] in ('unsigned', 'signed'):
                    return essential_type.split(' ')[0]
                if 'enum' in essential_type:
                    return 'enum'
            return None
        for token in cfg.tokenlist:
            if not isCast(token):
                continue
            to_type = _get_essential_category(token)
            #print(to_type)
            if to_type is None:
                continue
            from_type = _get_essential_category(token.astOperand1)
            #print(from_type)
            if from_type is None:
                continue
            if to_type == from_type:
                continue
            if to_type == 'bool' or from_type == 'bool':
                if token.astOperand1.isInt and token.astOperand1.getKnownIntValue() in [0, 1]:
                    # Exception
                    continue
                self.reportError(token, 10, 5)
                continue
            if to_type == 'enum':
                self.reportError(token, 10, 5)
                continue
            if from_type == 'floating' and to_type == 'char':
                self.reportError(token, 10, 5)
                continue
            if from_type == 'char' and to_type == 'floating':
                self.reportError(token, 10, 5)
                continue

    def misra_10_6(self, data):
        def reportErrorIfCompositeExpressionAssignedToWiderType(token, astOperand1, astOperand2):
            if (astOperand2.str not in ('+', '-', '*', '/', '%', '&', '|', '^', '>>', "<<", "?", ":", '~') and
                    not isCast(astOperand2)):
                return
            vt1 = astOperand1.valueType
            vt2 = astOperand2.valueType
            if not vt1 or vt1.pointer > 0:
                return
            if not vt2 or vt2.pointer > 0:
                return
            try:
                if isCast(astOperand2):
                    e = vt2.type
                else:
                    e = getEssentialType(astOperand2)
                if not e:
                    return
                lhsbits = vt1.bits if vt1.bits else bitsOfEssentialType(vt1.type)
                if lhsbits > bitsOfEssentialType(e):
                    self.reportError(token, 10, 6)
                if vt1.type.split(' ')[-1] in ('float', 'double') and e.split(' ')[-1] in ('float', 'double'):
                    if relativeLengthOfEssentialFloatingType(vt1.type) > relativeLengthOfEssentialFloatingType(e):
                        self.reportError(token, 10, 6)
            except ValueError:
                pass

        for token in data.tokenlist:
            if token.str == '=' and token.astOperand1 and token.astOperand2:
                reportErrorIfCompositeExpressionAssignedToWiderType(token, token.astOperand1, token.astOperand2)

            if isFunctionCall(token) and token.astOperand1 and token.astOperand1.function:
                functionDeclaration = token.astOperand1.function
                if functionDeclaration.tokenDef:
                    if functionDeclaration.tokenDef is token.astOperand1:
                        # Token is not a function call, but it is the definition of the function
                        continue
                parametersUsed = getArguments(token)
                for i in range(len(parametersUsed)):
                    parameterDefinition = functionDeclaration.argument.get(i+1)
                    if parameterDefinition and parameterDefinition.nameToken:
                        reportErrorIfCompositeExpressionAssignedToWiderType(token, parameterDefinition.nameToken, parametersUsed[i])

    def misra_10_7(self, cfg):
        for token in cfg.tokenlist:
            if token.astOperand1 is None or token.astOperand2 is None:
                continue
            if not token.isArithmeticalOp:
                continue
            parent = token.astParent
            if parent is None or parent.str == '=' or not parent.isOp:
                continue
            token_type = getEssentialType(token)
            if token_type is None:
                continue
            sibling = parent.astOperand1 if (token == parent.astOperand2) else parent.astOperand2
            sibling_type = getEssentialType(sibling)
            if sibling_type is None:
                continue
            b1 = bitsOfEssentialType(token_type)
            b2 = bitsOfEssentialType(sibling_type)
            if b1 > 0 and b1 < b2:
                self.reportError(token, 10, 7)
            if token_type.split(' ')[-1] in ('float', 'double') and sibling_type.split(' ')[-1] in ('float', 'double'):
                if relativeLengthOfEssentialFloatingType(token_type) < relativeLengthOfEssentialFloatingType(sibling_type):
                    self.reportError(token, 10, 7)

    def misra_10_8(self, data):
        for token in data.tokenlist:
            if not isCast(token):
                continue
            if not token.valueType or token.valueType.pointer > 0:
                continue
            if not token.astOperand1.valueType or token.astOperand1.valueType.pointer > 0:
                continue
            if not token.astOperand1.astOperand1:
                continue
            if token.astOperand1.str not in ('+', '-', '*', '/', '%', '&', '|', '^', '>>', "<<", "?", ":", '~'):
                continue
            if token.astOperand1.str != '~' and not token.astOperand1.astOperand2:
                continue
            if is_constant_integer_expression(token.astOperand1):
                continue
            e2 = getEssentialTypeCategory(token.astOperand1)
            e1 = getEssentialTypeCategory(token)
            if e1 != e2:
                self.reportError(token, 10, 8)
            else:
                try:
                    e = getEssentialType(token.astOperand1)
                    if not e:
                        continue
                    if bitsOfEssentialType(token.valueType.type) > bitsOfEssentialType(e):
                        self.reportError(token, 10, 8)
                    if e1 == "floating" and relativeLengthOfEssentialFloatingType(getEssentialType(token)) > relativeLengthOfEssentialFloatingType(e):
                        self.reportError(token, 10, 8)
                except ValueError:
                    pass

    def misra_11_1(self, data):
        for token in data.tokenlist:
            to_from = get_type_conversion_to_from(token)
            if to_from is None:
                continue
            from_type = get_literal_or(get_function_pointer_type, to_from[1])
            if from_type is None:
                continue
            to_type = get_function_pointer_type(to_from[0])
            if to_type is None or to_type != from_type:
                self.reportError(token, 11, 1)

    def misra_11_2(self, data):
        def get_pointer_type(type_token):
            while type_token and (type_token.str in ('const', 'struct')):
                type_token = type_token.next
            if type_token is None:
                return None
            if not type_token.isName:
                return None
            return type_token if (type_token.next and type_token.next.str == '*') else None

        incomplete_types = []

        for token in data.tokenlist:
            if token.str == 'struct' and token.next and token.next.next and token.next.isName and token.next.next.str == ';':
                incomplete_types.append(token.next.str)
            to_from = get_type_conversion_to_from(token)
            if to_from is None:
                continue
            to_pointer_type_token = get_literal_token_or(get_pointer_type, to_from[0])
            if to_pointer_type_token is None:
                continue
            from_pointer_type_token = get_literal_token_or(get_pointer_type, to_from[1])
            if from_pointer_type_token is None:
                continue
            if to_pointer_type_token.str == from_pointer_type_token.str:
                continue
            if from_pointer_type_token.typeScope is None and (from_pointer_type_token.str in incomplete_types):
                self.reportError(token, 11, 2)
            elif to_pointer_type_token.typeScope is None and (to_pointer_type_token.str in incomplete_types):
                self.reportError(token, 11, 2)

    def misra_11_3(self, data):
        for token in data.tokenlist:
            if not isCast(token):
                continue
            vt1 = token.valueType
            vt2 = token.astOperand1.valueType
            if not vt1 or not vt2:
                continue
            if vt1.type == 'void' or vt2.type == 'void':
                continue
            if (vt1.pointer > 0 and vt1.type == 'record' and
                    vt2.pointer > 0 and vt2.type == 'record' and
                    vt1.typeScopeId != vt2.typeScopeId):
                self.reportError(token, 11, 3)
            elif (vt1.pointer == vt2.pointer and vt1.pointer > 0 and
                  ((vt1.type != vt2.type) or (vt1.type == vt2.type and vt1.constness % 2 != vt2.constness % 2))
                  and vt1.type != 'char'):
                self.reportError(token, 11, 3)

    def misra_11_4(self, data):
        for token in data.tokenlist:
            if not isCast(token):
                continue
            vt1 = token.valueType
            vt2 = token.astOperand1.valueType
            if not vt1 or not vt2:
                continue
            if vt2.pointer > 0 and vt1.pointer == 0 and (vt1.isIntegral() or vt1.isEnum()) and vt2.type != 'void':
                self.reportError(token, 11, 4)
            elif vt1.pointer > 0 and vt2.pointer == 0 and (vt2.isIntegral() or vt2.isEnum()) and vt1.type != 'void':
                self.reportError(token, 11, 4)
        for scope in data.scopes:
            if scope.type != "Function":
                continue
            tok = scope.bodyStart
            while tok is not None:
                if tok.str != "=" or not tok.astOperand1 or not tok.astOperand2:
                    tok = tok.next
                    continue
                lhstype = tok.astOperand1.valueType
                rhstype = tok.astOperand2.valueType
                if lhstype is None or rhstype is None:
                    tok = tok.next
                    continue
                if (
                    lhstype.pointer >= 1
                    and not tok.astOperand2.isNumber
                    and rhstype.pointer == 0
                    and rhstype.type in {'short', 'int', 'long', 'long long'}
                ) or (
                    rhstype.pointer >= 1
                    and lhstype.pointer == 0
                    and lhstype.type in {'short', 'int', 'long', 'long long'}
                ):
                    self.reportError(tok, 11, 4)
                tok = tok.next

    def misra_11_5(self, data):
        for token in data.tokenlist:
            if not isCast(token):
                if token.astOperand1 and token.astOperand2 and token.str == "=" and token.next.str != "(":
                    vt1 = token.astOperand1.valueType
                    vt2 = token.astOperand2.valueType
                    if not vt1 or not vt2:
                        continue
                    if vt1.pointer > 0 and vt1.type != 'void' and vt2.pointer == vt1.pointer and vt2.type == 'void':
                        self.reportError(token, 11, 5)
                continue
            if token.astOperand1.astOperand1 and token.astOperand1.astOperand1.str in (
            'malloc', 'calloc', 'realloc', 'free'):
                continue
            vt1 = token.valueType
            vt2 = token.astOperand1.valueType
            if not vt1 or not vt2:
                continue
            if vt1.pointer > 0 and vt1.type != 'void' and vt2.pointer == vt1.pointer and vt2.type == 'void':
                self.reportError(token, 11, 5)

    def misra_11_6(self, data):
        for token in data.tokenlist:
            if not isCast(token):
                continue
            if token.astOperand1.astOperand1:
                continue
            vt1 = token.valueType
            vt2 = token.astOperand1.valueType
            if not vt1 or not vt2:
                continue
            if vt1.pointer == 1 and vt1.type == 'void' and vt2.pointer == 0 and token.astOperand1.str != "0":
                self.reportError(token, 11, 6)
            elif vt1.pointer == 0 and vt1.type != 'void' and vt2.pointer == 1 and vt2.type == 'void':
                self.reportError(token, 11, 6)

    def misra_11_7(self, data):
        for token in data.tokenlist:
            if not isCast(token):
                continue
            vt1 = token.valueType
            vt2 = token.astOperand1.valueType
            if not vt1 or not vt2:
                continue
            if token.astOperand1.astOperand1:
                continue
            if (vt2.pointer > 0 and vt1.pointer == 0 and
                    not vt1.isIntegral() and not vt1.isEnum() and
                    vt1.type != 'void'):
                self.reportError(token, 11, 7)
            elif (vt1.pointer > 0 and vt2.pointer == 0 and
                  not vt2.isIntegral() and not vt2.isEnum() and
                  vt1.type != 'void'):
                self.reportError(token, 11, 7)

    def misra_11_8(self, data):
        # TODO: reuse code in CERT-EXP05
        def check_constness_safe(const1, const2):
            while(const1 > 0 or const2 > 0):
                if const2 > const1:
                    return False
                const1 = const1 >> 1
                const2 = const2 >> 1
            return True

        for token in data.tokenlist:
            if isCast(token):
                # C-style cast
                if not token.valueType:
                    continue
                if not token.astOperand1.valueType:
                    continue
                if token.valueType.pointer == 0:
                    continue
                if token.astOperand1.valueType.pointer == 0:
                    continue
                const1 = token.valueType.constness
                const2 = token.astOperand1.valueType.constness
                if(not check_constness_safe(const1, const2)):
                    self.reportError(token, 11, 8)
                volat1 = token.valueType.volatility
                volat2 = token.astOperand1.valueType.volatility
                if (volat1 % 2) < (volat2 % 2):
                    self.reportError(token, 11, 8)

            elif token.str == '(' and token.astOperand1 and token.astOperand2 and token.astOperand1.function:
                # Function call
                function = token.astOperand1.function
                arguments = getArguments(token)
                for argnr, argvar in function.argument.items():
                    if argnr < 1 or argnr > len(arguments):
                        continue
                    if not argvar.isPointer:
                        continue
                    argtok = arguments[argnr - 1]
                    if not argtok.valueType:
                        continue
                    if argtok.valueType.pointer == 0:
                        continue
                    const1 = argvar.constness
                    const2 = arguments[argnr - 1].valueType.constness
                    if(not check_constness_safe(const1, const2)):
                        self.reportError(token, 11, 8)
                    volat1 = argvar.volatility
                    volat2 = arguments[argnr - 1].valueType.volatility
                    if (volat1 % 2) < (volat2 % 2):
                        self.reportError(token, 11, 8)

    def misra_11_9(self, data):
        for token in data.tokenlist:
            if token.astOperand1 and token.astOperand2 and token.str in ["=", "==", "!=", "?", ":"]:
                vt1 = token.astOperand1.valueType
                vt2 = token.astOperand2.valueType
                if not vt1 or not vt2:
                    continue
                if vt1.pointer > 0 and vt2.pointer == 0 and token.astOperand2.str == "NULL":
                    continue
                if (token.astOperand2.values and vt1.pointer > 0 and
                        vt2.pointer == 0 and token.astOperand2.values):
                    if token.astOperand2.getValue(0):
                        self.reportError(token, 11, 9)

    def misra_12_1_sizeof(self, rawTokens):
        state = 0
        compiled = re.compile(r'^[a-zA-Z_]')
        for tok in rawTokens:
            if tok.str.startswith('//') or tok.str.startswith('/*'):
                continue
            if tok.str == 'sizeof' or tok.str == "alignof":
                state = 1
            elif state == 1:
                if compiled.match(tok.str):
                    state = 2
                else:
                    state = 0
            elif state == 2:
                if tok.str in ('+', '-', '*', '/', '%'):
                    self.reportError(tok, 12, 1)
                else:
                    state = 0

    def misra_12_1(self, data):
        for token in data.tokenlist:
            p = getPrecedence(token)
            if p < 2 or p > 12:
                continue
            p1 = getPrecedence(token.astOperand1)
            if p < p1 <= 12 and not hasExplicitPrecedence(token.astOperand1, token):
                self.reportError(token, 12, 1)
                continue
            p2 = getPrecedence(token.astOperand2)
            if p < p2 <= 12 and not hasExplicitPrecedence(token, token.astOperand2):
                self.reportError(token, 12, 1)
                continue

    def misra_12_2(self, data):
        for token in data.tokenlist:
            if not (token.str in ('<<', '>>')):
                continue
            if (not token.astOperand2) or (not token.astOperand2.values):
                continue
            maxval = 0
            for val in token.astOperand2.values:
                if val.intvalue and val.intvalue > maxval:
                    maxval = val.intvalue
            if maxval == 0:
                continue
            sz = bitsOfEssentialType(getEssentialType(token.astOperand1))
            if sz <= 0:
                continue
            if maxval >= sz:
                self.reportError(token, 12, 2)

    def misra_12_3(self, data):
        for token in data.tokenlist:
            if token.str == ',' and token.astParent and token.astParent.str == ';':
                self.reportError(token, 12, 3)
            if token.str == ',' and token.astParent is None:
                if token.scope.type in ('Class', 'Struct'):
                    # Is this initlist..
                    tok = token
                    while tok and tok.str == ',':
                        tok = tok.next
                        if tok and tok.next and tok.isName and tok.next.str == '(':
                            tok = tok.next.link.next
                    if tok.str == '{':
                        # This comma is used in initlist, do not warn
                        continue
                prev = token.previous
                while prev:
                    if prev.str == ';':
                        self.reportError(token, 12, 3)
                        break
                    elif prev.str in ')}]':
                        prev = prev.link
                    elif prev.str in '({[':
                        break
                    prev = prev.previous

    def misra_12_4(self, cfg):
        for expr in cfg.tokenlist:
            if not expr.astOperand2 or not expr.astOperand1:
                continue
            if expr.valueType is None:
                continue
            if expr.valueType.sign is None or expr.valueType.sign != 'unsigned':
                continue
            if expr.valueType.pointer > 0:
                continue
            if not expr.valueType.isIntegral():
                continue
            op1 = expr.astOperand1.getKnownIntValue()
            if op1 is None:
                continue
            op2 = expr.astOperand2.getKnownIntValue()
            if op2 is None:
                continue
            bits = bitsOfEssentialType('unsigned ' + expr.valueType.type)
            if bits <= 0 or bits >= 64:
                continue
            max_value = (1 << bits) - 1
            if not is_constant_integer_expression(expr):
                continue
            if expr.str == '+' and op1 + op2 > max_value:
                self.reportError(expr, 12, 4)
            elif expr.str == '-' and op1 - op2 < 0:
                self.reportError(expr, 12, 4)
            elif expr.str == '*' and op1 * op2 > max_value:
                self.reportError(expr, 12, 4)

    def misra_12_5(self, data):
        for token in data.tokenlist:
            if not token.str == "sizeof":
                continue
            left_parenthese = token.next
            tok = left_parenthese
            while tok.next and tok.next.link != left_parenthese:
                tok = tok.next
                if not tok.variable:
                    continue
                if tok.variable.isArgument and tok.variable.isArray and tok.astParent.str != '[':
                    self.reportError(token, 12, 5)

    def misra_13_1(self, data):
        for token in data.tokenlist:
            if simpleMatch(token, ") {") and token.next.astParent == token.link:
                pass
            elif not simpleMatch(token, '= {'):
                continue
            init = token.next
            end = init.link
            if not end:
                continue  # syntax is broken

            tn = init
            while tn and tn != end:
                if tn.str == '[' and tn.link:
                    tn = tn.link
                    if tn and tn.next and tn.next.str == '=':
                        tn = tn.next.next
                        continue
                    else:
                        break
                if tn.str == '.' and tn.next and tn.next.isName:
                    tn = tn.next
                    if tn.next and tn.next.str == '=':
                        tn = tn.next.next
                    continue
                if tn.str in {'++', '--'} or tn.isAssignmentOp:
                    self.reportError(init, 13, 1)
                tn = tn.next

    def misra_13_3(self, data):
        for token in data.tokenlist:
            if token.str not in ('++', '--'):
                continue
            astTop = token
            while astTop.astParent and astTop.astParent.str not in (',', ';'):
                astTop = astTop.astParent
            if countSideEffects(astTop) >= 2:
                self.reportError(astTop, 13, 3)

    def misra_13_4(self, data):
        for token in data.tokenlist:
            if token.str not in ['=', '+=', '-=', '*=', '/=', '%=']:
                continue
            if not token.astParent or not token.astOperand1 or not token.astOperand1.previous:
                continue
            if token.astOperand1.str == '[' and token.astOperand1.previous.str in ('{', ','):
                continue
            if not (token.astParent.str in [',', ';', '{']):
                self.reportError(token, 13, 4)

    def misra_13_5(self, data):
        for token in data.tokenlist:
            if token.isLogicalOp and hasSideEffectsRecursive(token.astOperand2):
                self.reportError(token, 13, 5)

    def misra_13_6(self, data):
        for token in data.tokenlist:
            if not token.astParent:
                continue
            if token.str == 'sizeof' and hasSideEffectsRecursive(token.astParent.astOperand2):
                self.reportError(token, 13, 6)

    def misra_14_1(self, data):
        for token in data.tokenlist:
            if token.str == 'for':
                exprs = getForLoopExpressions(token)
                if not exprs:
                    continue
                for counter in findCounterTokens(exprs[1]):
                    if counter.valueType and counter.valueType.isFloat():
                        self.reportError(token, 14, 1)
            elif token.str == 'while':
                if isFloatCounterInWhileLoop(token):
                    self.reportError(token, 14, 1)

    def misra_14_2(self, data):
        for token in data.tokenlist:
            expressions = getForLoopExpressions(token)
            if not expressions:
                continue
            if expressions[0] and not expressions[0].isAssignmentOp:
                self.reportError(token, 14, 2)
            elif hasSideEffectsRecursive(expressions[1]):
                self.reportError(token, 14, 2)

            # Inspect modification of loop counter in loop body
            counter_vars = getForLoopCounterVariables(token)
            outer_scope = token.scope
            body_scope = None
            tn = token.next
            while tn and tn.next != outer_scope.bodyEnd:
                if tn.scope and tn.scope.nestedIn == outer_scope:
                    body_scope = tn.scope
                    break
                tn = tn.next
            if not body_scope:
                continue
            tn = body_scope.bodyStart
            while tn and tn != body_scope.bodyEnd:
                if tn.variable and tn.variable in counter_vars:
                    if tn.next:
                        # TODO: Check modifications in function calls
                        if hasSideEffectsRecursive(tn.next):
                            self.reportError(tn, 14, 2)
                tn = tn.next

    def misra_14_4(self, data):
        for token in data.tokenlist:
            if token.str != '(':
                continue
            if not token.astOperand1 or not (token.astOperand1.str in ['if', 'while', 'for']):
                continue

            if token.astOperand1.str == "for":
                # last ;
                token = token.astOperand2.astOperand2
                if token.astOperand1:
                    token = token.astOperand1
                    if token.str == ",":
                        if not isBoolExpression(token.astOperand2):
                            self.reportError(token, 14, 4)
                    elif not isBoolExpression(token):
                        self.reportError(token, 14, 4)

            else:
                if token.astOperand2.str == ",":
                    token = token.astOperand2
                if not isBoolExpression(token.astOperand2):
                    self.reportError(token, 14, 4)

    def misra_15_1(self, data):
        for token in data.tokenlist:
            if token.str == "goto":
                self.reportError(token, 15, 1)

    def misra_15_2(self, data):
        for token in data.tokenlist:
            if token.str != 'goto':
                continue
            if (not token.next) or (not token.next.isName):
                continue
            if not findGotoLabel(token):
                self.reportError(token, 15, 2)

    def misra_15_3(self, data):
        for token in data.tokenlist:
            if token.str != 'goto':
                continue
            if (not token.next) or (not token.next.isName):
                continue
            tok = findGotoLabel(token)
            if not tok:
                self.reportError(token, 15, 3)
                continue
            scope = token.scope
            while scope and scope != tok.scope:
                scope = scope.nestedIn
            if not scope:
                self.reportError(token, 15, 3)
            # Jump crosses from one switch-clause to another is non-compliant
            elif scope.type == 'Switch':
                # Search for start of a current case block
                tcase_start = token
                while tcase_start and tcase_start.str not in ('case', 'default'):
                    tcase_start = tcase_start.previous
                # Make sure that goto label doesn't occurs in the other
                # switch-clauses
                if tcase_start:
                    t = scope.bodyStart
                    in_this_case = False
                    while t and t != scope.bodyEnd:
                        if t == tcase_start:
                            in_this_case = True
                        if in_this_case and simpleMatch(t, 'break'):
                            in_this_case = False
                        if t == tok and not in_this_case:
                            self.reportError(token, 15, 3)
                            break
                        t = t.next

    def misra_15_4(self, data):
        # Return a list of scopes affected by a break or goto
        def getLoopsAffectedByBreak(knownLoops, scope, isGoto, gotoLabel):
            if scope and scope.type and scope.type not in ['Global', 'Function']:
                if not isGoto and scope.type == 'Switch':
                    return
                # if scope is the scope of gotoLabel, then no termination occurs.
                if gotoLabel and isGoto and scope == gotoLabel.scope:
                    return
                if scope.type in ['For', 'While', 'Do']:
                    knownLoops.append(scope)
                    if not isGoto:
                        return
                getLoopsAffectedByBreak(knownLoops, scope.nestedIn, isGoto, gotoLabel)

        loopWithBreaks = {}
        for token in data.tokenlist:
            if token.str not in ['break', 'goto']:
                continue
            affectedLoopScopes = []
            label = None
            if token.str == 'goto':
                # find goto label
                label = findGotoLabel(token)
                if label == None:
                    continue
            getLoopsAffectedByBreak(affectedLoopScopes, token.scope, token.str == 'goto', label)
            for scope in affectedLoopScopes:
                if scope in loopWithBreaks:
                    loopWithBreaks[scope] += 1
                else:
                    loopWithBreaks[scope] = 1

        for scope, breakCount in loopWithBreaks.items():
            if breakCount > 1:
                self.reportError(scope.bodyStart, 15, 4)

    def misra_15_5(self, data):
        for token in data.tokenlist:
            if token.str == 'return' and token.scope.type != 'Function':
                self.reportError(token, 15, 5)

    def gjb5369_2_1_3(self, rawTokens):
        # deprecated: cannot handle "else if" and complicated macros
        states = {"start":0, "afterIf":1,"beforeBranch":2}
        state = states["start"]
        indent = 0
        for token in rawTokens:
            if token.str.startswith('//') or token.str.startswith('/*'):
                continue
            if state == states["start"]:
                if token.str == 'if' and not simpleMatch(token.previous, '# if'):
                    state = states["afterIf"]
                elif token.str == 'else' and not simpleMatch(token.previous, '# else'):
                    state = states['beforeBranch']
            elif state == states["afterIf"]:
                if indent == 0 and token.str != '(':
                    state = states["start"]
                    continue
                elif token.str == '(':
                    indent += 1
                elif token.str == ')':
                    indent -= 1
                if indent == 0:
                    state = states['beforeBranch']
            elif state == states['beforeBranch']:
                if token.str != '{':
                    self.reportGJBError(token, 2, 1, 3)
                state = states['start']

    def misra_15_6(self, rawTokens):
        state = 0
        indent = 0
        tok1 = None
        for token in rawTokens:
            if token.str in ['if', 'for', 'while']:
                if simpleMatch(token.previous, '# if'):
                    continue
                if simpleMatch(token.previous, "} while"):
                    # is there a 'do { .. } while'?
                    start = rawlink(token.previous)
                    if start and simpleMatch(start.previous, 'do {'):
                        continue
                if state == 2:
                    self.reportError(tok1, 15, 6)
                state = 1
                indent = 0
                tok1 = token
            elif token.str == 'else':
                if simpleMatch(token.previous, '# else'):
                    continue
                if simpleMatch(token, 'else if'):
                    continue
                if state == 2:
                    self.reportError(tok1, 15, 6)
                state = 2
                indent = 0
                tok1 = token
            elif state == 1:
                if indent == 0 and token.str != '(':
                    state = 0
                    continue
                if token.str == '(':
                    indent = indent + 1
                elif token.str == ')':
                    if indent == 0:
                        state = 0
                    elif indent == 1:
                        state = 2
                    indent = indent - 1
            elif state == 2:
                if token.str.startswith('//') or token.str.startswith('/*'):
                    continue
                state = 0
                if token.str not in ('{', '#'):
                    self.reportError(tok1, 15, 6)

    def misra_15_7(self, data, rawTokens):
        for scope in data.scopes:
            if scope.type != 'Else':
                continue
            if not simpleMatch(scope.bodyStart, '{ if ('):
                continue
            if scope.bodyStart.column > 0:
                continue
            tok = scope.bodyStart.next.next.link
            if not simpleMatch(tok, ') {'):
                continue
            tok = tok.next.link
            if not simpleMatch(tok, '} else'):
                self.reportError(tok, 15, 7)
            # need raw tokens to process, otherwise can not get comment in token
            tok = tok.next
            if simpleMatch(tok, 'else { }'):
                for t in rawTokens:
                    if t.linenr == tok.linenr and t.str == tok.str:
                        if not t.next.next.str.startswith('//') and not t.next.next.str.startswith('/*'):
                            self.reportError(tok, 15, 7)

    def misra_16_1(self, cfg):
        for scope in cfg.scopes:
            if scope.type != 'Switch':
                continue
            in_case_or_default = False
            cur_case = None
            tok = scope.bodyStart.next
            if scope.bodyStart.next == scope.bodyEnd:
                self.reportError(tok, 16, 1)
            while tok != scope.bodyEnd:
                if not in_case_or_default:
                    if tok.str not in ('case', 'default'):
                        self.reportError(tok, 16, 1)
                    else:
                        if tok.str  == 'case': # to validate if case label is not a const expr
                            cur_case = tok
                            stack = [tok]
                            while stack:
                                cur = stack.pop()
                                if cur.function or cur.varId:
                                    self.reportError(cur, 16, 1)
                                if cur.astOperand1:
                                    stack.append(cur.astOperand1)
                                if cur.astOperand2:
                                    stack.append(cur.astOperand2)
                        in_case_or_default = True
                else:
                    if simpleMatch(tok, 'break ;'):
                        in_case_or_default = False
                        tok = tok.next
                if tok.str == '{':
                    tok = tok.link
                    if tok.scope.type == 'Unconditional' and simpleMatch(tok.previous.previous, 'break ;'):
                        in_case_or_default = False
                # in case there is no break to help me detect that current case is finished
                if simpleMatch(tok.next, "case") or simpleMatch(tok.next, "default") and tok.next != cur_case:
                    in_case_or_default = False
                tok = tok.next

    def misra_16_2(self, data):
        for token in data.tokenlist:
            if (token.str == 'case' or token.str == 'default') and token.scope.type != 'Switch':
                self.reportError(token, 16, 2)

    def misra_16_3(self, rawTokens):
        STATE_NONE = 0  # default state, not in switch case/default block
        STATE_BREAK = 1  # break/comment is seen but not its ';'
        STATE_OK = 2  # a case/default is allowed (we have seen 'break;'/'comment'/'{'/attribute)
        STATE_SWITCH = 3  # walking through switch statement scope

        state = STATE_NONE
        end_swtich_token = None  # end '}' for the switch scope
        for token in rawTokens:
            # Find switch scope borders
            if token.str == 'switch':
                state = STATE_SWITCH
            if state == STATE_SWITCH:
                if token.str == '{':
                    end_swtich_token = findRawLink(token)
                else:
                    continue

            if token.str == 'break' or token.str == 'return' or token.str == 'throw':
                state = STATE_BREAK
            elif token.str == ';':
                if state == STATE_BREAK:
                    state = STATE_OK
                elif token.next and token.next == end_swtich_token:
                    self.reportError(token.next, 16, 3)
                else:
                    state = STATE_NONE
            elif token.str.startswith('/*') or token.str.startswith('//'):
                if 'fallthrough' in token.str.lower():
                    state = STATE_OK
            elif simpleMatch(token, '[ [ fallthrough ] ] ;'):
                state = STATE_BREAK
            elif token.str == '{':
                state = STATE_OK
            elif token.str == '}' and state == STATE_OK:
                # is this {} an unconditional block of code?
                prev = findRawLink(token)
                if prev:
                    prev = prev.previous
                    while prev and prev.str[:2] in ('//', '/*'):
                        prev = prev.previous
                if (prev is None) or (prev.str not in ':;{}'):
                    state = STATE_NONE
            elif token.str == 'case' or token.str == 'default':
                if state != STATE_OK:
                    self.reportError(token, 16, 3)
                state = STATE_OK

    def misra_16_4(self, data):
        for token in data.tokenlist:
            if token.str != 'switch':
                continue
            if not simpleMatch(token, 'switch ('):
                continue
            if not simpleMatch(token.next.link, ') {'):
                continue
            startTok = token.next.link.next
            tok = startTok.next
            while tok and tok.str != '}':
                if tok.str == '{':
                    tok = tok.link
                elif tok.str == 'default':
                    break
                tok = tok.next
            if tok and tok.str != 'default':
                self.reportError(token, 16, 4)

    def misra_16_5(self, data):
        for token in data.tokenlist:
            if token.str != 'default':
                continue
            if token.previous and token.previous.str == '{':
                continue
            tok2 = token
            while tok2:
                if tok2.str in ('}', 'case'):
                    break
                if tok2.str == '{':
                    tok2 = tok2.link
                tok2 = tok2.next
            if tok2 and tok2.str == 'case':
                self.reportError(token, 16, 5)

    def misra_16_6(self, data):
        for token in data.tokenlist:
            if not (simpleMatch(token, 'switch (') and simpleMatch(token.next.link, ') {')):
                continue
            tok = token.next.link.next.next
            count = 0
            while tok:
                if tok.str in ['break', 'return', 'throw']:
                    count = count + 1
                elif tok.str == '{':
                    tok = tok.link
                    if isNoReturnScope(tok):
                        count = count + 1
                elif tok.str == '}':
                    break
                tok = tok.next
            if count < 2:
                self.reportError(token, 16, 6)

    def misra_16_7(self, data):
        for token in data.tokenlist:
            if simpleMatch(token, 'switch (') and isBoolExpression(token.next.astOperand2):
                self.reportError(token, 16, 7)

    def misra_17_1(self, data):
        for token in data.tokenlist:
            if isFunctionCall(token) and token.astOperand1.str in (
            'va_list', 'va_arg', 'va_start', 'va_end', 'va_copy'):
                self.reportError(token, 17, 1)
            elif token.str == 'va_list':
                self.reportError(token, 17, 1)

    def misra_17_2(self, data):
        # find recursions..
        def find_recursive_call(search_for_function, direct_call, calls_map, visited=None):
            if visited is None:
                visited = set()
            if direct_call == search_for_function:
                return True
            for indirect_call in calls_map.get(direct_call, []):
                if indirect_call == search_for_function:
                    return True
                if indirect_call in visited:
                    # This has already been handled
                    continue
                visited.add(indirect_call)
                if find_recursive_call(search_for_function, indirect_call, calls_map, visited):
                    return True
            return False

        # List functions called in each function
        function_calls = {}
        for scope in data.scopes:
            if scope.type != 'Function':
                continue
            calls = []
            tok = scope.bodyStart
            while tok != scope.bodyEnd:
                tok = tok.next
                if not isFunctionCall(tok, data.standards.c):
                    continue
                f = tok.astOperand1.function
                if f is not None and f not in calls:
                    calls.append(f)
            function_calls[scope.function] = calls

        # Report warnings for all recursions..
        for func in function_calls:
            for call in function_calls[func]:
                if not find_recursive_call(func, call, function_calls):
                    # Function call is not recursive
                    continue
                # Warn about all functions calls..
                for scope in data.scopes:
                    if scope.type != 'Function' or scope.function != func:
                        continue
                    tok = scope.bodyStart
                    while tok != scope.bodyEnd:
                        if tok.function and tok.function == call:
                            self.reportError(tok, 17, 2)
                        tok = tok.next

    def misra_17_4(self, cfg, create_dump_error):
        # case that missing return has been handled by cppcheck
        if create_dump_error and "Found a exit path from function with non-void return type that has missing return statement [missingReturn]" in create_dump_error:
            self.reportMessageError(create_dump_error, 17, 4)
            return
        # code below is only for checking if a return statement is compeleted
        for token in cfg.tokenlist:
            if token.str != "return":
                continue

            # if valueType is None, we assume that
            # current function has return value
            # and the return value type is FILE, time_t....
            if not token.valueType:
                if simpleMatch(token.next, ';'):
                    self.reportError(token, 17, 4)

            elif token.valueType.type != 'void':
                if simpleMatch(token.next, ';'):
                    self.reportError(token, 17, 4)

    def misra_17_6(self, rawTokens):
        for token in rawTokens:
            if simpleMatch(token, '[ static'):
                self.reportError(token, 17, 6)

    def misra_17_7(self, data):
        for token in data.tokenlist:
            if not token.scope.isExecutable:
                continue
            if token.str != '(' or token.astParent:
                continue
            if not token.previous.isName or token.previous.varId:
                continue
            if token.valueType is None:
                continue
            if token.valueType.type == 'void' and token.valueType.pointer == 0:
                continue
            self.reportError(token, 17, 7)

    def misra_17_8(self, data):
        for token in data.tokenlist:
            if not (token.isAssignmentOp or (token.str in ('++', '--'))):
                continue
            if not token.astOperand1:
                continue
            var = token.astOperand1.variable
            if var and var.isArgument:
                self.reportError(token, 17, 8)

    def misra_18_4(self, data):
        for token in data.tokenlist:
            if token.str not in ('+', '-', '+=', '-='):
                continue
            if token.astOperand1 is None or token.astOperand2 is None:
                continue
            vt1 = token.astOperand1.valueType
            vt2 = token.astOperand2.valueType
            if vt1 and vt1.pointer > 0 and vt2 and vt2.pointer > 0:
                continue
            if vt1 and vt1.pointer > 0:
                self.reportError(token, 18, 4)
            elif vt2 and vt2.pointer > 0:
                self.reportError(token, 18, 4)

    def misra_18_5(self, data):
        for tok in data.tokenlist:
            if tok.variable and tok.variable.isArgument and \
               tok.valueType and tok.valueType.pointer > 2:
                    self.reportError(tok.variable.nameToken, 18, 5)
        for var in data.variables:
            if not var.isPointer:
                continue
            typetok = var.nameToken
            count = 0
            while typetok:
                if typetok.str == '*':
                    count = count + 1
                elif not typetok.isName:
                    # continue loop for nested function pointer
                    # declarations (e.g. `int8_t*** (**p)(void)`)
                    if typetok.str == "(" and not count > 2:
                        typetok = typetok.previous
                        count = 0
                        continue
                    break
                typetok = typetok.previous
            if count > 2:
                self.reportError(var.nameToken, 18, 5)

    def misra_18_7(self, data):
        for scope in data.scopes:
            if scope.type != 'Struct':
                continue

            token = scope.bodyStart.next
            while token != scope.bodyEnd and token is not None:
                # Handle nested structures to not duplicate an error.
                if token.str == '{':
                    token = token.link

                if cppcheckdata.simpleMatch(token, "[ ]"):
                    self.reportError(token, 18, 7)
                    break
                token = token.next

    def misra_18_8(self, data):
        for var in data.variables:
            if not var.isArray or (not var.isLocal and not var.isArgument):
                continue
            # TODO Array dimensions are not available in dump, must look in tokens
            if var.nameToken == None:
                # The nameToken might be None if uses unnamed parameters.
                # In this case, use the var's typeStartToken instead.
                var.nameToken = var.typeStartToken
            typetok = var.nameToken.next
            if not typetok or typetok.str != '[':
                continue
            # Unknown define or syntax error
            if not typetok.astOperand2:
                continue
            if not isConstantExpression(typetok.astOperand2):
                self.reportError(var.nameToken, 18, 8)
            # This handle multiple dimensions array
            while True:
                link = typetok.link
                typetok = link.next
                if not typetok or typetok.str != '[':
                    break
                # Unknown define or syntax error
                if not typetok.astOperand2:
                    continue
                if not isConstantExpression(typetok.astOperand2):
                    self.reportError(var.nameToken, 18, 8)

    def misra_19_2(self, rawTokens):
        for token in rawTokens:
            if token.str == 'union':
                self.reportError(token, 19, 2)

    def misra_20_1(self, data):
        token_in_file = {}
        for token in data.tokenlist:
            if token.file not in token_in_file:
                token_in_file[token.file] = int(token.linenr)
            else:
                token_in_file[token.file] = min(token_in_file[token.file], int(token.linenr))
        for directive in data.directives:
            if not directive.str.startswith('#include'):
                continue
            if directive.file not in token_in_file:
                continue
            if token_in_file[directive.file] < int(directive.linenr):
                self.reportError(directive, 20, 1)

    def misra_20_2(self, data):
        for directive in data.directives:
            if not directive.str.startswith('#include '):
                continue
            for pattern in ('\\', '//', '/*', ',', "'"):
                if pattern in directive.str:
                    self.reportError(directive, 20, 2)
                    break

    def misra_20_3(self, data):
        def expandMacro(directive_map, name):
            expand_result = ""
            if name in directive_map:
                value_list = directive_map[name]
                if len(value_list) > 1:
                    return expand_result, True
                for value in value_list:
                    if value in directive_map:
                        result, is_concat = expandMacro(directive_map, value)
                        if is_concat:
                            return expand_result, True
                        expand_result += result
                    else:
                        expand_result += value
            return expand_result, False

        directive_map = {}
        for directive in data.directives:
            if not directive.str.startswith('#define '):
                continue

            words = directive.str.split(' ')
            if len(words) < 2:
                continue
            # Now we only handle normal case, others will be reported in next loop
            directive_map[words[1]] = words[2:]

        for directive in data.directives:
            if not directive.str.startswith('#include '):
                continue

            words = directive.str.split(' ')

            # If include directive contains more than two words, here would be
            # violation anyway.
            if len(words) > 2:
                self.reportError(directive, 20, 3)

            # Handle include directives with not quoted argument
            elif len(words) > 1:
                filename = words[1]
                if not ((filename.startswith('"') and
                         filename.endswith('"')) or
                        (filename.startswith('<') and
                         filename.endswith('>'))):
                    # We are handle only directly included files in the
                    # following format: #include file.h
                    # Cases with macro expansion provided by MISRA document are
                    # skipped because we don't always have access to directive
                    # definition.
                    if '.' in filename:
                        self.reportError(directive, 20, 3)
                    else:
                        # handle macro expansion, don't consider can't access cases now.
                        if filename in directive_map:
                            real_filename, is_concat = expandMacro(directive_map, filename)
                            if is_concat:
                                self.reportError(directive, 20, 3)
                            if not ((real_filename.startswith('"') and
                                     real_filename.endswith('"')) or
                                    (real_filename.startswith('<') and
                                     real_filename.endswith('>'))):
                                self.reportError(directive, 20, 3)
                        else:
                            self.reportError(directive, 20, 3)

    def misra_20_4(self, data):
        for directive in data.directives:
            res = re.search(r'#define ([a-z][a-z0-9_]+)', directive.str)
            if res and isKeyword(res.group(1), data.standards.c):
                self.reportError(directive, 20, 4)

    def misra_20_5(self, data):
        for tok in getattr(data, 'rawTokens', []):
            if tok.str == '#' and tok.next and tok.next.str == 'undef':
                self.reportError(tok, 20, 5)
        for directive in getattr(data, 'directives', []):
            if directive.str.startswith('#undef '):
                self.reportError(directive, 20, 5)

    def misra_20_6(self, create_dump_error):
        if "it is invalid to use a preprocessor directive as macro parameter [preprocessorErrorDirective]" in create_dump_error:
            self.reportMessageError(create_dump_error, 20, 6)

    def checkMarcoArgs(self, data, reportErrorFunc):
        def find_string_concat(exp, arg, directive_args):
            # Handle concatenation of string literals, e.g.:
            # #define MACRO(A, B) (A " " B)
            # Addon should not report errors for both macro arguments.
            arg_pos = exp.find(arg, 0)
            need_check = False
            skip_next = False
            state_in_string = False
            pos_search = arg_pos + 1
            directive_args = [a.strip() for a in directive_args if a != arg]
            arg = arg.strip()
            while pos_search < len(exp):
                if exp[pos_search] == '"':
                    if state_in_string:
                        state_in_string = False
                    else:
                        state_in_string = True
                    pos_search += 1
                elif exp[pos_search].isalnum():
                    word = ""
                    while pos_search < len(exp) and exp[pos_search].isalnum():
                        word += exp[pos_search]
                        pos_search += 1
                    if word == arg:
                        pos_search += 1
                    elif word in directive_args:
                        skip_next = True
                        break
                elif exp[pos_search] == ' ':
                    pos_search += 1
                elif state_in_string:
                    pos_search += 1
                else:
                    need_check = True
                    break
            return need_check, skip_next

        for directive in data.directives:
            d = Define(directive)
            if d.expansionList == '':
                continue
            exp = '(' + d.expansionList + ')'
            skip_next = False
            for arg in d.args:
                if skip_next:
                    _, skip_next = find_string_concat(exp, arg, d.args)
                    continue
                need_check, skip_next = find_string_concat(exp, arg, d.args)
                if not need_check:
                    continue

                pos = 0
                while pos < len(exp):
                    pos = exp.find(arg, pos)
                    if pos < 0:
                        break
                    # is 'arg' used at position pos
                    pos1 = pos - 1
                    pos2 = pos + len(arg)
                    pos = pos2
                    if pos1 >= 0 and (isalnum(exp[pos1]) or exp[pos1] == '_'):
                        continue
                    if pos2 < len(exp) and (isalnum(exp[pos2]) or exp[pos2] == '_'):
                        continue

                    while pos1 >= 0 and exp[pos1] == ' ':
                        pos1 -= 1
                    if exp[pos1] == '#':
                        continue
                    if exp[pos1] not in '([,.':
                        # check the token before the argument, like const qualified
                        word_before = ""
                        pos_before = pos1
                        while pos_before > 0 and (exp[pos_before].isalnum() or exp[pos_before] == '_'):
                            word_before = exp[pos_before] + word_before
                            pos_before -= 1
                        if isKeyword(word_before, data.standards.c):
                            continue
                        reportErrorFunc(directive)
                        break
                    while pos2 < len(exp) and exp[pos2] == ' ':
                        pos2 += 1
                    if pos2 < len(exp) and exp[pos2] not in ')]#,':
                        # ignore * as the pointer of type
                        if exp[pos2] == "*":
                            word_after_star = ""
                            pos_after_star = pos2 + 1
                            while pos_after_star < len(exp) and exp[pos_after_star] == ' ':
                                pos_after_star += 1
                            while pos_after_star < len(exp) and (exp[pos_after_star].isalnum() or exp[pos_after_star] == '_'):
                                word_after_star += exp[pos_after_star]
                                pos_after_star += 1
                            directive_args = [a.strip() for a in d.args if a != arg]
                            if word_after_star not in directive_args:
                                continue
                        reportErrorFunc(directive)
                        break

    def misra_20_7(self, data):
        def reportErrorFunc(directive):
            self.reportError(directive, 20, 7)
        self.checkMarcoArgs(data, reportErrorFunc)

    def misra_20_8(self, cfg):
        for cond in cfg.preprocessor_if_conditions:
            #print(cond)
            if cond.result and cond.result not in (0,1):
                self.reportError(cond, 20, 8)

    def misra_20_9(self, cfg):
        for cond in cfg.preprocessor_if_conditions:
            if cond.E is None:
                continue
            defined = []
            for directive in cfg.directives:
                if directive.file == cond.file and directive.linenr == cond.linenr:
                    for name in re.findall(r'[^_a-zA-Z0-9]defined[ ]*\([ ]*([_a-zA-Z0-9]+)[ ]*\)', directive.str):
                        defined.append(name)
                    for name in re.findall(r'[^_a-zA-Z0-9]defined[ ]*([_a-zA-Z0-9]+)', directive.str):
                        defined.append(name)
                    break
            for s in cond.E.split(' '):
                if len(s) == 0:
                    continue
                if (s[0] >= 'A' and s[0] <= 'Z') or (s[0] >= 'a' and s[0] <= 'z'):
                    if isKeyword(s):
                        continue
                    if s in defined:
                        continue
                    self.reportError(cond, 20, 9)

    def misra_20_10(self, data):
        for directive in data.directives:
            d = Define(directive)
            if d.expansionList.find('#') >= 0:
                self.reportError(directive, 20, 10)

    def misra_20_11(self, cfg):
        for directive in cfg.directives:
            d = Define(directive)
            for arg in d.args:
                res = re.search(r'[^#]#[ ]*%s[ ]*##' % arg, ' ' + d.expansionList)
                if res:
                    self.reportError(directive, 20, 11)

    def misra_20_12(self, cfg):
        def _is_hash_hash_op(expansion_list, arg):
            return re.search(r'##[ ]*%s[^a-zA-Z0-9_]' % arg, expansion_list) or \
                   re.search(r'[^a-zA-Z0-9_]%s[ ]*##' % arg, expansion_list)

        def _is_other_op(expansion_list, arg):
            pos = expansion_list.find(arg)
            while pos >= 0:
                pos1 = pos - 1
                pos2 = pos + len(arg)
                pos = expansion_list.find(arg, pos2)
                if isalnum(expansion_list[pos1]) or expansion_list[pos1] == '_':
                    continue
                if isalnum(expansion_list[pos2]) or expansion_list[pos2] == '_':
                    continue
                while expansion_list[pos1] == ' ':
                    pos1 = pos1 - 1
                if expansion_list[pos1] == '#':
                    continue
                while expansion_list[pos2] == ' ':
                    pos2 = pos2 + 1
                if expansion_list[pos2] == '#':
                    continue
                return True
            return False

        def _is_arg_macro_usage(directive, arg):
            for macro_usage in cfg.macro_usage:
                if macro_usage.file == directive.file and macro_usage.linenr == directive.linenr:
                    for macro_usage_arg in cfg.macro_usage:
                        if macro_usage_arg == macro_usage:
                            continue
                        if (macro_usage.usefile == macro_usage_arg.usefile and
                            macro_usage.uselinenr == macro_usage_arg.uselinenr and
                            macro_usage.usecolumn == macro_usage_arg.usecolumn):
                            # TODO: check arg better
                            return True
            return False

        for directive in cfg.directives:
            define = Define(directive)
            if define.expansionList == '':
                continue
            expansion_list = '(%s)' % define.expansionList
            for arg in define.args:
                if not _is_hash_hash_op(expansion_list, arg):
                    continue
                if not _is_other_op(expansion_list, arg):
                    continue
                if _is_arg_macro_usage(directive, arg):
                    self.reportError(directive, 20, 12)
                    break

    def misra_20_13(self, data):
        dir_pattern = re.compile(r'#[ ]*([^ (<]*)')
        for directive in data.directives:
            dir = directive.str
            mo = dir_pattern.match(dir)
            if mo:
                dir = mo.group(1)
            if dir not in ['define', 'elif', 'else', 'endif', 'error', 'if', 'ifdef', 'ifndef', 'include',
                           'pragma', 'undef', 'warning'] and not (dir.startswith("include\"") or dir.startswith("include<")):
                self.reportError(directive, 20, 13)

    def misra_20_14(self, data):
        # stack for #if blocks. contains the #if directive until the corresponding #endif is seen.
        # the size increases when there are inner #if directives.
        ifStack = []
        for directive in data.directives:
            if directive.str.startswith('#if ') or directive.str.startswith('#ifdef ') or directive.str.startswith(
                    '#ifndef '):
                ifStack.append(directive)
            elif directive.str == '#else' or directive.str.startswith('#elif '):
                if len(ifStack) == 0:
                    self.reportError(directive, 20, 14)
                    ifStack.append(directive)
                elif directive.file != ifStack[-1].file:
                    self.reportError(directive, 20, 14)
            elif directive.str == '#endif':
                if len(ifStack) == 0:
                    self.reportError(directive, 20, 14)
                elif directive.file != ifStack[-1].file:
                    self.reportError(directive, 20, 14)
                    ifStack.pop()

    def misra_21_1(self, data):
        re_forbidden_macro = re.compile(r'#(?:define|undef) _[_A-Z]+')
        re_macro_name = re.compile(r'#(?:define|undef) (.+)[ $]')
        re_defined_name = re.compile(r'#(?:define|undef) (defined)$')

        for d in data.directives:
            # Search for forbidden identifiers
            m = re.search(re_forbidden_macro, d.str)
            if m:
                self.reportError(d, 21, 1)
                continue

            if re.search(re_defined_name, d.str) is not None:
                self.reportError(d, 21, 1)
                continue

            # Search standard library identifiers in macro names
            define = Define(d)
            if define.name and isStdLibId(define.name, data.standards.c):
                self.reportError(d, 21, 1)

    def misra_21_2(self, cfg):
        for directive in cfg.directives:
            define = Define(directive)
            if define.isFunction and define.expansionList and re.match(r'\(+_+BUILTIN_.*', define.expansionList.upper()):
                # exception
                continue
            if define.name and re.match(r'_+BUILTIN_.*', define.name.upper()):
                self.reportError(directive, 21, 2)
            if isStdLibId(define.name, cfg.standards.c):
                self.reportError(directive, 21, 2)
        for func in cfg.functions:
            if isStdLibId(func.name, cfg.standards.c):
                tok = func.tokenDef if func.tokenDef else func.token
                self.reportError(tok, 21, 2)
            # handle define function name with "_BUILTIN_" prefix
            if func.name and re.match(r'_+BUILTIN_.*', func.name.upper()):
                tok = func.tokenDef if func.tokenDef else func.token
                self.reportError(tok, 21, 2)
        for scope in cfg.scopes:
            if scope.type != "Struct" and scope.type != "Enum" and scope.type != "Union" or not scope.className:
                continue
            if isStdLibId(scope.className, cfg.standards.c):
                self.reportError(scope.bodyStart, 21, 2)
            if re.match(r'_+BUILTIN_.*', scope.className.upper()):
                self.reportError(scope.bodyStart, 21, 2)
        for var in cfg.variables:
            tok = var.nameToken
            if (tok != None): # No need to check `...` whose nameToken will be "0" thus becomes NoneType
                if isStdLibId(tok.str, cfg.standards.c):
                    self.reportError(tok, 21, 2)
                if re.match(r'_+BUILTIN_.*', tok.str.upper()):
                    self.reportError(tok, 21, 2)
        for typedef in cfg.typedefInfo:
            if not typedef.name:
                continue
            if isStdLibId(typedef.name, cfg.standards.c):
                self.reportError(typedef.nameToken, 21, 2)
            if re.match(r'_+BUILTIN_.*', typedef.name.upper()):
                self.reportError(typedef.nameToken, 21, 2)


    def misra_21_3(self, data):
        for token in data.tokenlist:
            if isFunctionCall(token) and (token.astOperand1.str in ('malloc', 'calloc', 'realloc', 'free', 'aligned_alloc')):
                self.reportError(token, 21, 3)

    def misra_21_4(self, data):
        directive = findInclude(data.directives, '<setjmp.h>')
        if directive:
            self.reportError(directive, 21, 4)

    def misra_21_5(self, data):
        directive = findInclude(data.directives, '<signal.h>')
        if directive:
            self.reportError(directive, 21, 5)

    def misra_21_6(self, data):
        dir_stdio = findInclude(data.directives, '<stdio.h>')
        dir_wchar = findInclude(data.directives, '<wchar.h>')
        if dir_stdio:
            self.reportError(dir_stdio, 21, 6)
        if dir_wchar:
            self.reportError(dir_wchar, 21, 6)
        define_map = {}
        for directive in data.directives:
            if directive.str.startswith("#include"):
                header = removeprefix(directive.str, "#include").strip()
                if header not in define_map:
                    continue
                if define_map[header] == "<stdio.h>" or define_map[header] == "<wchar.h>":
                    self.reportError(directive, 21, 6)
                continue
            d = Define(directive)
            if d.args != [] or d.expansionList == '':
                # don't handle macro function
                continue
            value = ""
            for item in d.expansionList.split(" "):
                if item == "":
                    continue
                if item in define_map:
                    value += define_map[item]
                else:
                    value += item
            define_map[d.name] = value

    def misra_21_7(self, data):
        for token in data.tokenlist:
            if isFunctionCall(token) and (token.astOperand1.str in ('atof', 'atoi', 'atol', 'atoll')):
                self.reportError(token, 21, 7)

    def misra_21_8(self, data):
        for token in data.tokenlist:
            if isFunctionCall(token) and (token.astOperand1.str in ('abort', 'exit', 'getenv', '_Exit', 'quick_exit')):
                self.reportError(token, 21, 8)

    def misra_21_9(self, data):
        for token in data.tokenlist:
            # If user define a function named "bsearch" or "qsort", it could be false negative.
            if isFunctionCall(token) and (token.astOperand1.str in ('bsearch', 'qsort')):
                self.reportError(token, 21, 9)

    def misra_21_10(self, data):
        directive = findInclude(data.directives, '<time.h>')
        if directive:
            self.reportError(directive, 21, 10)

        for token in data.tokenlist:
            if (token.str == 'wcsftime') and token.next and token.next.str == '(':
                self.reportError(token, 21, 10)

    def misra_21_11(self, data):
        directive = findInclude(data.directives, '<tgmath.h>')
        if directive:
            self.reportError(directive, 21, 11)

    def misra_21_12(self, data):
        if findInclude(data.directives, '<fenv.h>'):
            for token in data.tokenlist:
                if token.str in [
                    'fexcept_t',
                    'FE_INEXACT',
                    'FE_DIVBYZERO',
                    'FE_UNDERFLOW',
                    'FE_OVERFLOW',
                    'FE_INVALID',
                    'FE_ALL_EXCEPT',
                ] and token.isName:
                    self.reportError(token, 21, 12)
                if isFunctionCall(token) and (token.astOperand1.str in (
                        'feclearexcept',
                        'fegetexceptflag',
                        'feraiseexcept',
                        'fesetexceptflag',
                        'fetestexcept')):
                    self.reportError(token, 21, 12)

    def misra_21_14(self, data):
        # buffers used in strcpy/strlen/etc function calls
        string_buffers = []
        for token in data.tokenlist:
            if token.str[0] == 's' and isFunctionCall(token.next):
                name, args = cppcheckdata.get_function_call_name_args(token)
                if name is None:
                    continue
                def _get_string_buffers(match, args, argnum):
                    if not match:
                        return []
                    ret = []
                    for a in argnum:
                        if a < len(args):
                            arg = args[a]
                            while arg and arg.str in ('.', '::'):
                                arg = arg.astOperand2
                            if arg and arg.varId != 0 and arg.varId not in ret:
                                ret.append(arg.varId)
                    return ret
                string_buffers += _get_string_buffers(name == 'strcpy', args, [0, 1])
                string_buffers += _get_string_buffers(name == 'strncpy', args, [0, 1])
                string_buffers += _get_string_buffers(name == 'strlen', args, [0])
                string_buffers += _get_string_buffers(name == 'strcmp', args, [0, 1])
                string_buffers += _get_string_buffers(name == 'sprintf', args, [0])
                string_buffers += _get_string_buffers(name == 'snprintf', args, [0, 3])

        for token in data.tokenlist:
            if token.str != 'memcmp':
                continue
            name, args = cppcheckdata.get_function_call_name_args(token)
            if name is None:
                continue
            if len(args) != 3:
                continue
            for arg in args[:2]:
                if arg.str[-1] == '\"':
                    self.reportError(arg, 21, 14)
                    continue
                while arg and arg.str in ('.', '::'):
                    arg = arg.astOperand2
                if arg and arg.varId and arg.varId in string_buffers:
                    self.reportError(arg, 21, 14)

    def misra_21_15(self, data):
        for token in data.tokenlist:
            if token.str not in ('memcpy', 'memmove', 'memcmp'):
                continue
            name, args = cppcheckdata.get_function_call_name_args(token)
            if name is None:
                continue
            if len(args) != 3:
                continue
            if args[0].valueType is None or args[1].valueType is None:
                continue
            if args[0].valueType.type == args[1].valueType.type:
                continue
            if args[0].valueType.type == 'void' or args[1].valueType.type == 'void':
                continue
            self.reportError(token, 21, 15)

    def misra_21_16(self, cfg):
        for token in cfg.tokenlist:
            if token.str != 'memcmp':
                continue
            name, args = cppcheckdata.get_function_call_name_args(token)
            if name is None:
                continue
            if len(args) != 3:
                continue
            for arg in args[:2]:
                if arg.valueType is None:
                    continue
                if arg.valueType.pointer > 1:
                    continue
                if arg.valueType.sign in ('unsigned', 'signed') and \
                  (getEssentialType(arg) != 'char' or arg.valueType.pointer != 1):
                    continue
                if arg.valueType.isEnum():
                    continue
                # handle essentially boolean type
                if arg.valueType.isBool():
                    continue
                self.reportError(token, 21, 16)

    def misra_21_19(self, cfg):
        for token in cfg.tokenlist:
            if token.str in ('localeconv', 'getenv', 'setlocale', 'strerror') and simpleMatch(token.next, '('):
                name, _ = cppcheckdata.get_function_call_name_args(token)
                if name is None or name != token.str:
                    continue
                parent = token.next
                while simpleMatch(parent.astParent, '+'):
                    parent = parent.astParent
                # x = f()
                if simpleMatch(parent.astParent, '=') and parent == parent.astParent.astOperand2:
                    lhs = parent.astParent.astOperand1
                    if lhs and lhs.valueType and lhs.valueType.pointer > 0 and lhs.valueType.constness == 0:
                        self.reportError(token, 21, 19)
            if token.str == '=':
                lhs = token.astOperand1
                while simpleMatch(lhs, '*') and lhs.astOperand2 is None:
                    lhs = lhs.astOperand1
                if not simpleMatch(lhs, '.'):
                    continue
                while simpleMatch(lhs, '.'):
                    lhs = lhs.astOperand1
                if lhs and lhs.variable and simpleMatch(lhs.variable.typeStartToken, 'lconv'):
                    self.reportError(token, 21, 19)

    def misra_21_20(self, cfg):
        assigned = {}
        invalid = []
        for token in cfg.tokenlist:
            # No sophisticated data flow analysis, bail out if control flow is "interrupted"
            if token.str in ('{', '}', 'break', 'continue', 'return'):
                assigned = {}
                invalid = []
                continue

            # When pointer is assigned, remove it from 'assigned' and 'invalid'
            if token.varId and token.varId > 0 and simpleMatch(token.next, '='):
                for name in assigned.keys():
                    while token.varId in assigned[name]:
                        assigned[name].remove(token.varId)
                while token.varId in invalid:
                    invalid.remove(token.varId)
                continue

            # Calling dangerous function
            if token.str in ('asctime', 'ctime', 'gmtime', 'localtime', 'localeconv', 'getenv', 'setlocale', 'strerror'):
                name, args = cppcheckdata.get_function_call_name_args(token)
                if name and name == token.str:
                    # make assigned pointers invalid
                    for varId in assigned.get(name, ()):
                        if varId not in invalid:
                            invalid.append(varId)

                    # assign pointer
                    parent = token.next
                    while parent.astParent and (parent.astParent.str == '+' or isCast(parent.astParent)):
                        parent = parent.astParent
                    if simpleMatch(parent.astParent, '='):
                        eq = parent.astParent
                        vartok = eq.previous
                        if vartok and vartok.varId and vartok.varId > 0:
                            if name not in assigned:
                                assigned[name] = [vartok.varId]
                            elif vartok.varId not in assigned[name]:
                                assigned[name].append(vartok.varId)
                continue

            # taking value of invalid pointer..
            if token.astParent and token.varId:
                if token.varId in invalid:
                    self.reportError(token, 21, 20)

    def misra_21_21(self, cfg):
        for token in cfg.tokenlist:
            if token.str == 'system':
                name, args = cppcheckdata.get_function_call_name_args(token)
                if name == 'system' and len(args) == 1:
                    self.reportError(token, 21, 21)

    def misra_22_5(self, cfg):
        for token in cfg.tokenlist:
            if token.isUnaryOp("*") or (token.isBinaryOp() and token.str == '.'):
                fileptr = token.astOperand1
                if fileptr.variable and cppcheckdata.simpleMatch(fileptr.variable.typeStartToken, 'FILE *'):
                    self.reportError(token, 22, 5)

    def misra_22_7(self, cfg):
        for eofToken in cfg.tokenlist:
            if eofToken.str != 'EOF':
                continue
            if eofToken.astParent is None or not eofToken.astParent.isComparisonOp:
                continue
            if eofToken.astParent.astOperand1 == eofToken:
                eofTokenSibling = eofToken.astParent.astOperand2
            else:
                eofTokenSibling = eofToken.astParent.astOperand1
            while isCast(eofTokenSibling) and eofTokenSibling.valueType and eofTokenSibling.valueType.type and eofTokenSibling.valueType.type == 'int':
                eofTokenSibling = eofTokenSibling.astOperand2 if eofTokenSibling.astOperand2 else eofTokenSibling.astOperand1
            if eofTokenSibling is not None and eofTokenSibling.valueType and eofTokenSibling.valueType and eofTokenSibling.valueType.type in ('bool', 'char', 'short'):
                self.reportError(eofToken, 22, 7)

    def misra_22_8(self, cfg):
        is_zero = False
        for token in cfg.tokenlist:
            if simpleMatch(token, 'errno = 0'):
                is_zero = True
            if token.str == '(' and not simpleMatch(token.link, ') {'):
                name, _ = cppcheckdata.get_function_call_name_args(token.previous)
                if name is None:
                    continue
                if is_errno_setting_function(name):
                    if not is_zero:
                        self.reportError(token, 22, 8)
                else:
                    is_zero = False

    def misra_22_9(self, cfg):
        errno_is_set = False
        for token in cfg.tokenlist:
            if token.str == '(' and not simpleMatch(token.link, ') {'):
                name, args = cppcheckdata.get_function_call_name_args(token.previous)
                if name is None:
                    continue
                errno_is_set = is_errno_setting_function(name)
            if errno_is_set and token.str in '{};':
                errno_is_set = False
                tok = token.next
                while tok and tok.str not in ('{','}',';','errno'):
                    tok = tok.next
                if tok is None or tok.str != 'errno':
                    self.reportError(token, 22, 9)
                elif (tok.astParent is None) or (not tok.astParent.isComparisonOp):
                    self.reportError(token, 22, 9)

    def misra_22_10(self, cfg):
        last_function_call = None
        for token in cfg.tokenlist:
            if token.str == '(' and not simpleMatch(token.link, ') {'):
                name, args = cppcheckdata.get_function_call_name_args(token.previous)
                last_function_call = name
            if token.str == '}':
                last_function_call = None
            if token.str == 'errno' and token.astParent and token.astParent.isComparisonOp:
                if last_function_call is None:
                    self.reportError(token, 22, 10)
                elif not is_errno_setting_function(last_function_call):
                    self.reportError(token, 22, 10)

    def gjb5369_1_1_9(self, data):
        def isInvalid(tokenName, standard):
            return isKeyword(tokenName, standard) or isStdLibId(tokenName, standard)

        for var in data.variables:
            if var.nameToken and isInvalid(var.nameToken.str, 'c99'):
                self.reportGJBError(var.nameToken, 1, 1, 9)
        for func in data.functions:
            if isInvalid(func.name, 'c99'):
                if (func.token is None):
                    self.reportGJBError(func.tokenDef, 1, 1, 9)
                else:
                    self.reportGJBError(func.token, 1, 1, 9)
        for scope in data.scopes:
            if scope.type in ('Struct', 'Enum', 'Union') and isInvalid(scope.className, 'c99'):
                self.reportGJBError(scope.bodyStart, 1, 1, 9)

    def gjb5369_6_1_3(self, data):
        for token in data.tokenlist:
            if token.isOp and token.str in ('<<', '>>', '>>=', '<<='):
                for t1, t2 in itertools.product(
                        list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                        list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
                ):
                    e1 = getEssentialTypeCategory(t1)
                    e2 = getEssentialTypeCategory(t2)
                    if not isUnsignedType(e1):
                        self.reportGJBError(token, 6, 1, 3)

    def gjb5369_1_1_5(self, cfg):
        def isTypeToken(token, selfDefinedTypeNames, stdlibTypes):
            return token.str in INT_TYPES or token.str in stdlibTypes or token.str in ('double', 'float', 'FILE') or token.str in selfDefinedTypeNames

        def isArgContainTypeToken(argument, selfDefinedTypeNames, stdlibTypes):
            token = argument.typeStartToken
            while not token is None:
                if isTypeToken(token, selfDefinedTypeNames, stdlibTypes):
                    return True
                if token == argument.typeEndToken:
                    return False
                token = token.next

        def isSharedScope(scope1, scope2):
            while not scope1 is None:
                if scope1 == scope2:
                    return True
                scope1 = scope1.nestedIn
            return False

        def getSelfDefinedTypeNames(funcScope):
            result = []
            for scope in cfg.scopes:
                if scope.type == 'Struct' or scope.type == 'Enum' or scope.type == 'Union':
                    if isSharedScope(funcScope, scope.nestedIn):
                        result.append(scope.className)
            return result

        def getStdlibTypes():
            result = []
            for l in C99_STDLIB_IDENTIFIERS.values():
                for identifier in l:
                    if identifier[-2:] == '_t':
                        result.append(identifier)
            return result

        stdlibTypes = getStdlibTypes()
        for func in cfg.functions:
            selfDefinedTypeNames = getSelfDefinedTypeNames(func.nestedIn)
            for arg in func.argument:
                argument = func.argument[arg]
                if not isArgContainTypeToken(argument, selfDefinedTypeNames, stdlibTypes):
                    self.reportGJBError(argument.typeStartToken, 1, 1, 5)

    def gjb5369_6_1_5(self, cfg):
        def gjb5369_6_1_5_checker(tok, astOperand1_essentialType, astOperand2):
            lhs = astOperand1_essentialType
            rhs = getEssentialType(astOperand2)
            if lhs is None or rhs is None:
                return
            lhs_category = self.get_category(lhs)
            rhs_category = self.get_category(rhs)

            # exclude case like cha += 1
            if tok.str in ('+=', '-='):
                if lhs_category == "char" and (rhs_category == "signed" or rhs_category == "unsigned"):
                    return

            # deal with assign signed to unsigned
            if lhs_category == 'unsigned' and rhs_category == 'signed':
                if (tok.valuesId != None):
                    valueflows = filter(lambda x: x.Id == tok.valuesId, cfg.valueflow)
                    for valueflow in valueflows:
                        for value in valueflow.values:
                            # Cppcheck use `MathLib::biguint` for value.intvalue when dump to xml files
                            # so that `-1` which represents all negative values becomes ((1 << 64) - 1)
                            if (value.intvalue == 18446744073709551615):
                                self.reportGJBError(tok, 6, 1, 5)
                if not isIntConstExpr(astOperand2) or (astOperand2.getKnownIntValue() and astOperand2.getKnownIntValue() < 0):
                    self.reportGJBError(tok, 6, 1, 5)

        # Reuse checking process in misra_10_3, but use a different function to to check and report
        self.misra_10_3(cfg, gjb5369_6_1_5_checker)

    def gjb5369_6_1_7(self, data):
        # refer to misra_6_1 & misra_6_2
        for token in data.tokenlist:
            if not token.valueType:
                continue
            if token.valueType.bits == 0:
                continue
            if not token.variable:
                continue
            if not token.scope:
                continue
            if token.scope.type not in 'Struct':
                continue
            if not token.valueType.type in set(['short', 'int', 'long', 'long long']):
                self.reportGJBError(token, 6, 1, 7)

    def isTokenNotInteger(self, token):
        return token and token.valueType and token.valueType.type not in {'short', 'int', 'long', 'long long'}

    def gjb5369_6_1_10(self, data):
        for token in data.tokenlist:
            if token.str == '[':
                if self.isTokenNotInteger(token.astOperand2):
                    self.reportGJBError(token, 6, 1, 10)

    def gjb5369_6_1_11(self, cfg):
        # deprecated: cannot handle 1+1
        for token in cfg.tokenlist:
            if token.isNumber and token.astParent and token.astParent.str == '!':
                self.reportGJBError(token, 6, 1, 11)

    def gjb5369_7_1_1(self, cfg):
        for token in cfg.tokenlist:
            if not isFunctionCall(token):
                continue
            paramUsed = getArguments(token)
            for func in cfg.functions:
                if func.tokenId == token.previous.Id or func.name != token.previous.str:
                    continue
                if len(func.argument) != len(paramUsed):
                    self.reportGJBError(token, 7, 1, 1)

    def gjb5369_7_1_2(self, cfg):
        mainfile = None
        # Find the file path where the main function is defined
        for token in cfg.tokenlist:
            if token.function != None:
                if (not isFunctionCall(token.next)) and token.str == "main":
                    mainfile = token.file
                    break
        if mainfile == None:
            return

        # Check that all functions defined in that file has been called
        funcDefs = set([]) # set of funcId defined in mainfile
        funcCalls = set([]) # set of func call in mainfile
        funcDefTokens = {} # funcId -> token
        for token in cfg.tokenlist:
            if token.function != None and token.file == mainfile:
                if isFunctionCall(token.next):
                    funcCalls.add(token.function.Id)
                else:
                    funcDefs.add(token.function.Id)
                    funcDefTokens[token.function.Id] = token

        funcDefButNotCall = funcDefs - funcCalls
        if len(funcDefButNotCall) > 0:
            for funcId in funcDefButNotCall:
                funcToken = funcDefTokens[funcId]
                if funcToken.str != "main":
                    self.reportGJBError(funcToken, 7, 1, 2)

    def gjb5369_7_1_3(self, cfg):
        funcCalls = {} # func.id -> bool
        funcDefs = {} # func.id -> func
        for func in cfg.functions:
            if func.isStatic:
                funcCalls[func.Id] = False
                funcDefs[func.Id] = func
        for token in cfg.tokenlist:
            if token.function != None and isFunctionCall(token.next) and token.function.Id in funcCalls.keys():
                funcCalls[token.function.Id] = True
        for funcId in funcCalls.keys():
            if not funcCalls[funcId]:
                func = funcDefs[funcId]
                self.reportGJBError(func.tokenDef if func.tokenDef else func.token, 7, 1, 3)

    def gjb5369_7_1_8(self, data):
        for token in data.tokenlist:
            name, args = cppcheckdata.get_function_call_name_args(token)
            if name is None: # Not a function call
                continue
            for arg in args:
                if arg.valueType is None:
                    continue
                if arg.valueType.type == 'void':
                    self.reportGJBError(token, 7, 1, 8)

    def gjb5369_8_1_3(self, rawTokens):
        def has_comment(searchLine):
            for tok in rawTokens:
                if tok.linenr != searchLine:
                    continue
                if ('/*' in tok.str) or ('*/' in tok.str) or ("//" in tok.str):
                    return True
            return False

        if len(rawTokens) == 0:
            return;
        compiled = re.compile(r'^0[0-7]+$')
        lastLine = rawTokens[-1].linenr
        for tok in rawTokens:
            if compiled.match(tok.str) == None:
                continue
            if has_comment(tok.linenr):
                continue
            if tok.linenr != 1 and has_comment(tok.linenr-1):
                continue
            if tok.linenr != lastLine and has_comment(tok.linenr+1):
                continue
            self.reportGJBError(tok, 8, 1, 3)

    def gjb5369_10_1_1(self, rawTokens):
        for token in rawTokens:
            starts_with_double_slash = token.str.startswith('//')
            if token.str.startswith('/*') or starts_with_double_slash:
                s = token.str.lstrip('/')
                # The // character sequence in an URL path should not be treated as a comment starting sequence
                # An URL path always has format like protocol://hostname, the :// sequence can be used to distinguish comments and URL path
                s = s.replace('://','')
                if (((not starts_with_double_slash) and '//' in s)) or '/*' in s:
                    self.reportGJBError(token, 10, 1, 1)

    def gjb5369_14_1_1(self, data):
        for token in data.tokenlist:
            if token.isOp and token.str in set(["==", "!="]):
                if not token.astOperand1 or not token.astOperand2:
                    continue
                if token.astOperand1.valueType == None or token.astOperand2.valueType == None:
                    continue
                if token.astOperand1.valueType.isFloat() or token.astOperand2.valueType.isFloat():
                    self.reportGJBError(token, 14, 1, 1)

    def gjb5369_15_1_1(self, data):
        for scope in data.scopes:
            if scope.type != 'Enum':
                continue
            enum_types = {} # enum_type name -> enum_type token
            tok = scope.bodyStart.next
            while tok != scope.bodyEnd:
                if tok.isName:
                    enum_types[tok.str] = tok
                tok = tok.next
            # Check nested scopes layer by layer from this Enum until Global
            cur_scope = scope
            while True:
                if cur_scope == None:
                    break
                else:
                    if cur_scope.type == "Global":
                        start_tok = data.tokenlist[0]
                        end_tok = data.tokenlist[-1]
                    else:
                        start_tok = cur_scope.bodyStart
                        end_tok = cur_scope.bodyEnd
                    tok = start_tok
                    while tok != end_tok:
                        if tok.variable and tok.isName:
                            if tok.str in enum_types.keys():
                                # Report where the enumerator is located, with variable token in other_location
                                self.reportGJBError(enum_types[tok.str], 15, 1, 1, [tok])
                        tok = tok.next
                    cur_scope = cur_scope.nestedIn

    def gjb5369_15_1_2(self, data):
        scopeVars = self.prepareScopeVars(data)
        num_sign_chars = self.get_num_significant_naming_chars(data)
        for scope in scopeVars:
            # split from misra_5_3
            if scope == None:
                continue
            if scope.type != "Global":
                continue
            for innerscope in scopeVars[scope]["scopelist"]:
                if innerscope not in scopeVars:
                    continue
                for element1 in scopeVars[scope]["namelist"]:
                    for element2 in scopeVars[innerscope]["namelist"]:
                        if element1.nameToken.str[:num_sign_chars] == element2.nameToken.str[:num_sign_chars]:
                            if int(element1.nameToken.linenr) > int(element2.nameToken.linenr):
                                self.reportGJBError(element1.nameToken, 15, 1, 2, [element2.nameToken])
                            else:
                                self.reportGJBError(element2.nameToken, 15, 1, 2, [element1.nameToken])

    def gjb5369_15_1_5(self, data):
        scopeVars = self.prepareScopeVars(data)
        num_sign_chars = self.get_num_significant_naming_chars(data)
        for scope in scopeVars:
            # split from misra_5_3
            if scope == None or scope.type == "Global":
                continue
            for innerscope in scopeVars[scope]["scopelist"]:
                if innerscope not in scopeVars:
                    continue
                for element1 in scopeVars[scope]["namelist"]:
                    for element2 in scopeVars[innerscope]["namelist"]:
                        if element1.nameToken.str[:num_sign_chars] == element2.nameToken.str[:num_sign_chars]:
                            if int(element1.nameToken.linenr) > int(element2.nameToken.linenr):
                                self.reportGJBError(element1.nameToken, 15, 1, 5, [element2.nameToken])
                            else:
                                self.reportGJBError(element2.nameToken, 15, 1, 5, [element1.nameToken])

    def gjb8114_1_1_6(self, cfg):
        def getMacroName(directive):
            return (directive.str.split())[1]

        def getScope(linenr):
            globalScope = None
            for scope in cfg.scopes:
                if scope.type != 'Global':
                    if scope.bodyStart.linenr <= linenr and scope.bodyEnd.linenr >= linenr:
                        return scope
                else:
                    globalScope = scope
            return globalScope

        def isInSameScope(directive1, directive2):
            scope1 = getScope(directive1.linenr)
            scope2 = getScope(directive2.linenr)
            return scope1.Id == scope2.Id

        defined_macros = dict()
        for directive in cfg.directives:
            if re.match(r'#define[ \t]+([\w_][\w_\d]*).*', directive.str):
                defined_macros[getMacroName(directive)] = directive
            if re.match(r'#undef[ \t]+([\w_][\w_\d]*).*', directive.str):
                macroName = getMacroName(directive)
                if defined_macros.get(macroName) is None:
                    self.reportGJBError(directive, 1, 1, 6, gjb_type = '8114')
                else:
                    if not isInSameScope(defined_macros[macroName], directive):
                        self.reportGJBError(directive, 1, 1, 6, gjb_type = '8114')
                    defined_macros.pop(macroName)

        # report error for unmatched #define
        for directive in defined_macros.values():
            self.reportGJBError(directive, 1, 1, 6, gjb_type = '8114')
    def gjb8114_1_1_7(self, data):
        def reportErrorFunc(directive):
            self.reportGJBError(directive, 1, 1, 7, gjb_type='8114')

        # check the outer most brackets
        def outerBracketsMatching(expansionList):
            stack = []
            for i in range(len(expansionList)):
                if i == len(expansionList) - 1:
                    if expansionList[i] == ')' and len(stack) == 1 and stack[0] == 0:
                        return True
                    else:
                        return False
                if expansionList[i] == '(':
                    stack.append(i)
                if expansionList[i] == ')':
                    stack.pop()

        for directive in data.directives:
            d = Define(directive)
            if d.expansionList != '' and d.isFunction:
                if not outerBracketsMatching(d.expansionList):
                    self.reportGJBError(directive, 1, 1, 7, gjb_type='8114')

        # check arguments' brackects
        self.checkMarcoArgs(data, reportErrorFunc)

    def gjb8114_1_1_17(self, data):
        for token in data.tokenlist:
            if token.str == "extern" and token.scope != None:
                if token.scope.type == "Function":
                    self.reportGJBError(token, 1, 1, 17, gjb_type = '8114')

    def gjb8114_1_6_18(self, data):
        for token in data.tokenlist:
            if token.str == "(" and isFunctionCall(token):
                # skip self defined gets function
                if token.previous.str == "gets" and not token.previous.function:
                    self.reportGJBError(token, 1, 6, 18, gjb_type= "8114")

    def gjb8114_1_1_19(self, data):
        for token in data.tokenlist:
            # Cppcheck splits "extern type a = b;" to "extern type a; a = b;", and
            # isSplittedVarDeclEq is true only for the first ';' in spllited form
            if token.isSplittedVarDeclEq and token.next.variable and token.next.variable.isExtern:
                self.reportGJBError(token, 1, 1, 19, gjb_type = '8114')

    def gjb8114_1_1_22(self, data):
        included_paths = []
        for directive in data.directives:
            if not directive.str.startswith('#include'):
                continue
            include_re = re.compile(r'^#include\s*[<"]([a-zA-Z0-9]+[a-zA-Z\-_./\\0-9]*)[">]$')
            m = re.match(include_re, directive.str)
            if not m:
                continue
            included_file_path = m.group(1)
            if included_file_path not in included_paths:
                included_paths.append(included_file_path)
            else:
                self.reportGJBError(directive, 1, 1, 22, gjb_type = '8114')

    def gjb8114_1_1_23(self, cfg):
        for func in cfg.functions:
            if func.tokenDef == None or func.tokenDef.next.str != '(':
                continue
            tokNextToBracket = func.tokenDef.next.next
            if tokNextToBracket.str == ')':
                self.reportGJBError(tokNextToBracket, 1, 1, 23, gjb_type = '8114')

    def gjb8114_1_4_2(self, data, rawTokens):
        for scope in data.scopes:
            if scope.type != 'Else' and scope.type != 'If':
                continue
            if scope.bodyStart.next.str == '}':
                self.reportGJBError(scope.bodyStart, 1, 4, 2, gjb_type='8114')
                continue
            if scope.bodyStart.next.str == ';':
                # need raw tokens to process, otherwise can not get comment in token
                semicolon = scope.bodyStart.next
                for rawToken in rawTokens:
                    if rawToken.linenr == semicolon.linenr and rawToken.str == semicolon.str:
                        if rawToken.next.str != '/* no deal with */' or rawToken.next.linenr != rawToken.linenr:
                            self.reportGJBError(semicolon, 1, 4, 2, gjb_type='8114')

    def gjb8114_1_4_5(self, cfg):
        for scope in cfg.scopes:
            if scope.type != 'Switch':
                continue
            tok = scope.bodyStart.next
            has_case = False
            has_default = False
            while tok != scope.bodyEnd:
                if tok.str == 'case':
                    has_case = True
                    break
                if tok.str == 'default':
                    has_default = True
                tok = tok.next
            # no case found, but has default in switch
            if (not has_case) and has_default:
                self.reportGJBError(scope.bodyStart, 1, 4, 5, gjb_type='8114')

    def gjb8114_1_4_6(self, cfg):
        for scope in cfg.scopes:
            if scope.type != 'Switch':
                continue
            tok = scope.bodyStart.next
            prev_tok = scope.bodyStart.previous
            enum_types = None
            if prev_tok and prev_tok.previous and prev_tok.previous.valueType and prev_tok.previous.valueType.typeScope:
                # find switch parameter
                enum_scope = prev_tok.previous.valueType.typeScope
                # if this is a enum type in switch, get all enum types
                if enum_scope.type == 'Enum':
                    enum_types = get_enum_types(enum_scope)
            while tok and tok != scope.bodyEnd:
                if enum_types and tok.str == 'case' and tok.next.str in enum_types.keys():
                    del enum_types[tok.next.str]
                if tok.str == 'default':
                    break
                tok = tok.next
            if tok and tok.str != 'default':
                if enum_types == {}:
                    continue
                enum_types = None
                self.reportGJBError(scope.bodyStart, 1, 4, 6, gjb_type='8114')

    def gjb8114_1_4_7(self, rawTokens):
        def is_switch_label(tok):
            return tok.str == 'case' or tok.str == 'default'
        # This state machine is similar to but slightly different from misra_16_3:
        # 1. end up with break or return, not throw
        # 2. empty case in switch is not good case
        # 3. change comment from fallthrough to shared
        # 4. report on the position of case instead
        # 5. the shared comment place can be anywhere in the case
        STATE_NONE = 0  # default state, not in switch case/default block
        STATE_BREAK = 1  # break/comment is seen but not its ';'
        STATE_OK = 2  # a case/default is allowed (we have seen 'break;'/'comment'/'{'/attribute)
        STATE_SWITCH = 3  # walking through switch statement scope
        STATE_COMMENT = 4 # find shared comment, skip to the next case

        state = STATE_NONE
        end_swtich_token = None  # end '}' for the switch scope
        last_case_token = None  # last 'case' token, for report position
        for token in rawTokens:
            # Find switch scope borders
            if token.str == 'switch':
                state = STATE_SWITCH
            if state == STATE_SWITCH:
                if token.str == '{':
                    end_swtich_token = findRawLink(token)
                else:
                    continue
            # skip all shared commented case/default
            if state == STATE_COMMENT and not is_switch_label(token):
                continue

            # case and default in the switch statement must end up with break or return
            if token.str == 'break' or token.str == 'return':
                state = STATE_BREAK
            elif token.str == ';':
                if state == STATE_BREAK:
                    state = STATE_OK
                elif token.next and token.next == end_swtich_token:
                    self.reportGJBError(last_case_token, 1, 4, 7, gjb_type='8114')
                else:
                    state = STATE_NONE
            elif token.str.startswith('/*') or token.str.startswith('//'):
                if 'shared' in token.str.lower():
                    state = STATE_COMMENT
            elif simpleMatch(token, '[ [ shared ] ] ;'):
                state = STATE_COMMENT
            elif token.str == '{':
                state = STATE_OK
            elif token.str == '}' and state == STATE_OK:
                # is this {} an unconditional block of code?
                prev = findRawLink(token)
                if prev:
                    prev = prev.previous
                    while prev and prev.str[:2] in ('//', '/*'):
                        prev = prev.previous
                if (prev is None) or (prev.str not in ':;{}'):
                    state = STATE_NONE
            elif is_switch_label(token):
                if state != STATE_OK and state != STATE_COMMENT:
                    if last_case_token:
                        self.reportGJBError(last_case_token, 1, 4, 7, gjb_type='8114')
                state = STATE_NONE
                last_case_token = token

    def gjb8114_1_6_7(self, data, create_dump_error):
        if create_dump_error and "bits is undefined behaviour [shiftTooManyBits]" in create_dump_error:
            self.reportGJBMessageError(create_dump_error, 1, 6,7, gjb_type = '8114')

    def gjb8114_1_6_9(self, data):
        for token in data.tokenlist:
            if token.str == '[' and token.astOperand2 != None:
                knownIntValue = token.astOperand2.getKnownIntValue()
                if self.isTokenNotInteger(token.astOperand2) or (knownIntValue != None and knownIntValue < 0):
                    self.reportGJBError(token, 1, 6, 9, gjb_type = '8114')

    def gjb8114_1_7_11(self, data):
        for token in data.tokenlist:
            if token.str == "(" and isFunctionCall(token):
                if token.valueType != None and token.valueType.type != "void":
                    if token.astParent == None:
                        self.reportGJBError(token, 1, 7, 11, gjb_type = '8114')
                    elif token.astParent != None and token.astParent.str == "(":
                        tk = token.astParent
                        explicitlyUseVoid = False
                        while tk == None or tk.link != token.astParent:
                            if tk.str == "void":
                                explicitlyUseVoid = True
                                break
                            tk = tk.next
                        if not explicitlyUseVoid:
                            self.reportGJBError(token, 1, 7, 11, gjb_type = '8114')

    def gjb8114_1_7_12(self, data):
        for token in data.tokenlist:
            if token.str == "(" and isFunctionCall(token):
                if token.valueType != None and token.valueType.type == "void":
                    if token.astParent != None and token.astParent.str == "(":
                        tk = token.astParent
                        explicitlyUseVoid = False
                        while tk == None or tk.link != token.astParent:
                            if tk.str == "void":
                                explicitlyUseVoid = True
                                break
                            tk = tk.next
                        if explicitlyUseVoid:
                            self.reportGJBError(token, 1, 7, 12, gjb_type = '8114')

    def gjb8114_1_8_4(self, rawTokens):
        compiled = re.compile(r'^0[0-7]+$')
        is_comment = lambda tok:tok.str.startswith('/*') and 'octal' in tok.str
        for tok in rawTokens:
            if compiled.match(tok.str):
                prev,curr = tok.previous,tok.next
                found_prev, found_curr = False, False
                while prev and not found_prev:
                    if is_comment(prev) and prev.next and prev.next.linenr == tok.linenr:
                        found_prev = True
                    prev = prev.previous
                while curr and not found_curr:
                    if is_comment(curr) and curr.linenr == tok.linenr:
                        found_curr = True
                    curr = curr.next
                if not(found_prev or found_curr):
                    self.reportGJBError(tok, 1, 8, 4, gjb_type='8114')

    def gjb8114_1_8_5(self, data):
        for tok in data.tokenlist:
            if tok.isNumber:
                for l in reversed(tok.str):
                    if l.islower():
                        self.reportGJBError(tok, 1, 8, 5, gjb_type='8115')
                    if l.isdigit():
                        break

    def misra_cpp_2_7_1(self, data, rawTokens):
        for token in rawTokens:
            if token.str.startswith('/*'):
                s = token.str.lstrip('/*')
                if '/*' in s:
                    self.reportCXXError(token, 2, 7, 1)

    def misra_cpp_3_3_2(self, cfg, rawToken):
        def reportErrorFunc(token):
            self.reportCXXError(token, 3, 3, 2)

        self.checkStaticFuncStmt(cfg, reportErrorFunc)

    def misra_cpp_3_9_3(self, data):
        for token in data.tokenlist:
            if not token.isOp:
                continue
            if token.str in ('&', '|', '^', '&=', '|=', '^=', '>>', '>>==', '<<', '<<=='):
                for t1, t2 in itertools.product(
                    list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                    list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
                ):
                    if getEssentialTypeCategory(t1) == 'floating' or getEssentialTypeCategory(t2) == 'floating':
                        self.reportCXXError(token, 3, 9, 3)
                continue
            if token.str == '~':
                for t2 in list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]):
                    if getEssentialTypeCategory(t2) == 'floating':
                        self.reportCXXError(token, 3, 9, 3)

    def misra_cpp_5_0_20(self, data):
        for token in data.tokenlist:
            if not token.isOp:
                continue
            if token.str in ('&', '|', '^', '&=', '|=', '^=', '>>', '>>==', '<<', '<<=='):
                for t1, t2 in itertools.product(
                    list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                    list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
                ):
                    e1 = getEssentialTypeCategory(t1)
                    e2 = getEssentialTypeCategory(t2)
                    # If any one of e1 and e2 is None, it indicates the None one's valueType-type cannot be parsed
                    # into an essential type and cannot be a legal operand to a binary bitwise operator, which is not
                    # in the scope of this rule. For example, in the standard stream processing command 'std::cout << "run\n";',
                    # both sides of '<<' shows with 'None' valueType-type in the dump file. Another example is that
                    # e1 is None ('std::cout') and e2 is 'container' (std::string).
                    if not e1 or not e2:
                        continue
                    if e1 != e2 or t1.valueType.type != t2.valueType.type or t1.valueType.sign != t2.valueType.sign:
                        self.reportCXXError(token, 5, 0, 20)

    def misra_cpp_5_0_21(self, data):
        for token in data.tokenlist:
            if not token.isOp:
                continue
            if token.str in ('&', '|', '^', '&=', '|=', '^=', '>>', '>>==', '<<', '<<=='):
                for t1, t2 in itertools.product(
                    list(getTernaryOperandsRecursive(token.astOperand1) or [token.astOperand1]),
                    list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]),
                ):
                    e1 = getEssentialTypeCategory(t1)
                    e2 = getEssentialTypeCategory(t2)
                    if e1 and e2 and (not isUnsignedType(e1) or not isUnsignedType(e2)):
                        self.reportCXXError(token, 5, 0, 20)
                continue
            if token.str == '~':
                for t2 in list(getTernaryOperandsRecursive(token.astOperand2) or [token.astOperand2]):
                    e2 = getEssentialTypeCategory(t2)
                    if e2 and not isUnsignedType(e2):
                        self.reportCXXError(token, 5, 0, 20)

    def misra_cpp_6_2_3(self, rawTokens):
        def is_comment(token):
            if token.str.startswith("/*") or token.str.startswith("//"):
                return True
            return False
        for token in rawTokens:
            if token.str != ";":
                continue
            if token.next == None:
                return
            if token.next.str == ";" and token.linenr == token.next.linenr:
                self.reportCXXError(token, 6, 2, 3)

            if not is_comment(token.previous) and not is_comment(token.next) :
                continue

            if is_comment(token.previous):
                if token.linenr == token.previous.linenr:
                    self.reportCXXError(token, 6, 2, 3)

            if is_comment(token.next):
                if token.previous.str != "{" and not is_comment(token.previous) \
                and token.previous.str != ";":
                    continue
                if token.linenr == token.next.linenr:
                    if token.next.column - token.column <= 1:
                        self.reportCXXError(token, 6,2,3)

    def misra_cpp_6_3_1(self, data, rawTokens):
    # copied from misra_15_6
        state = 0
        indent = 0
        tok1 = None
        for token in rawTokens:
            if token.str in ['for', 'while', 'switch']:
                if simpleMatch(token.previous, "} while"):
                    # is there a 'do { .. } while'?
                    start = rawlink(token.previous)
                    if start and simpleMatch(start.previous, 'do {'):
                        continue
                if state == 2:
                    self.reportCXXError(tok1, 6, 3, 1)
                state = 1
                indent = 0
                tok1 = token
            elif state == 1:
                if indent == 0 and token.str != '(':
                    state = 0
                    continue
                if token.str == '(':
                    indent = indent + 1
                elif token.str == ')':
                    if indent == 0:
                        state = 0
                    elif indent == 1:
                        state = 2
                    indent = indent - 1
            elif state == 2:
                if token.str.startswith('//') or token.str.startswith('/*'):
                    continue
                state = 0
                if token.str not in ('{'):
                    self.reportCXXError(tok1, 6, 3, 1)

    def misra_cpp_6_4_1(self, data, rawTokens):
    # copied from misra_15_6
        state = 0
        indent = 0
        tok1 = None
        for token in rawTokens:
            if token.str in ['if']:
                if simpleMatch(token.previous, '# if'):
                    continue
                if state == 2:
                    self.reportCXXError(tok1, 6, 4, 1)
                state = 1
                indent = 0
                tok1 = token
            elif token.str == 'else':
                if simpleMatch(token.previous, '# else'):
                    continue
                if simpleMatch(token, 'else if'):
                    continue
                if state == 2:
                    self.reportCXXError(tok1, 6, 4, 1)
                state = 2
                indent = 0
                tok1 = token
            elif state == 1:
                if indent == 0 and token.str != '(':
                    state = 0
                    continue
                if token.str == '(':
                    indent = indent + 1
                elif token.str == ')':
                    if indent == 0:
                        state = 0
                    elif indent == 1:
                        state = 2
                    indent = indent - 1
            elif state == 2:
                if token.str.startswith('//') or token.str.startswith('/*'):
                    continue
                state = 0
                if token.str not in ('{', '#'):
                    self.reportCXXError(tok1, 6, 4, 1)

    def misra_cpp_6_4_5(self, data, rawTokens):
        # this is similar to and copied from 16_3, except:
        # 1. return is not allowed
        # 2. the location of error is changed
        # 3. remove fallthrough comments
        # 4. fix the no break statement error after a compound statement

        STATE_NONE = 0  # default state, not in switch case/default block
        STATE_BREAK = 1  # break/comment is seen but not its ';'
        STATE_OK = 2  # a case/default is allowed (we have seen 'break;'/'comment'/'{'/attribute)
        STATE_SWITCH = 3  # walking through switch statement scope

        state = STATE_NONE
        end_swtich_token = None  # end '}' for the switch scope
        last_label_token = None  # record the last label for report error
        for token in rawTokens:
            # Find switch scope borders
            if token.str == 'switch':
                state = STATE_SWITCH
            if state == STATE_SWITCH:
                if token.str == '{':
                    end_swtich_token = findRawLink(token)
                else:
                    continue

            if token.str == 'break' or token.str == 'throw':
                state = STATE_BREAK
            elif token.str == ';':
                if state == STATE_BREAK:
                    state = STATE_OK
                elif token.next and token.next == end_swtich_token and last_label_token:
                    # report error if the next token after `;` is end switch
                    self.reportCXXError(last_label_token, 6, 4, 5)
                else:
                    state = STATE_NONE
            elif token.str == '{':
                state = STATE_OK
            elif token.str == '}' and state == STATE_OK:
                # is this {} an unconditional block of code?
                prev = findRawLink(token)
                if prev:
                    prev = prev.previous
                    while prev and prev.str[:2] in ('//', '/*'):
                        prev = prev.previous
                if (prev is None) or (prev.str not in ':;{}'):
                    state = STATE_NONE
                    # report error if the next token after compound
                    # statement is end switch
                    if token.next and token.next == end_swtich_token and last_label_token:
                        self.reportCXXError(last_label_token, 6, 4, 5)
            elif token.str == 'case' or token.str == 'default':
                # found the next case without a prev break or throw
                if state != STATE_OK and last_label_token:
                     self.reportCXXError(last_label_token, 6, 4, 5)
                state = STATE_OK
                last_label_token = token

    def misra_cpp_6_4_6(self, cfg):
        # this is slightly different from misra C 16.5 and gjb8114_1_4_6
        # so some codes are coppied from them and merged together

        # find enum types in switch scope
        def find_enum_types_in_switch(scope):
            enum_types = None
            prev_tok = scope.bodyStart.previous
            if prev_tok and prev_tok.previous and prev_tok.previous.valueType and prev_tok.previous.valueType.typeScope:
                # find switch parameter
                enum_scope = prev_tok.previous.valueType.typeScope
                # if this is a enum type in switch, get all enum types
                if enum_scope.type == 'Enum':
                    enum_types = get_enum_types(enum_scope)
            return enum_types

        # check the default label in switch statement is not the last one
        def check_default_in_switch_scope(scope):
            token = scope.bodyStart.next
            enum_types = find_enum_types_in_switch(scope)
            has_default = False
            is_empty_default = True
            while token != scope.bodyEnd:
                # find if case exists after default
                if token.str == 'case':
                    if has_default:
                        # default is not the last label
                        self.reportCXXError(scope.bodyStart, 6, 4, 6)
                        # break to avoid more errors on cases come after default
                        # break this while loop and continue to next switch scope
                        return
                    if enum_types and token.next.str in enum_types.keys():
                        del enum_types[token.next.str]
                if token.str == '{':
                    # skip compound statements to avoid nested switch
                    token = token.link
                if token.str == 'default':
                    # found a default label
                    has_default = True
                elif has_default and token.str != ';' and token.str != ':':
                    # record not empty after default case
                    # because we will break the while loop if we found another
                    # label, this assigned to false is always in default case
                    is_empty_default = False
                token = token.next
            if not has_default:
                if enum_types == {}:
                    # is complete enum type, exception
                    return
                enum_types = None
                # no default label found
                self.reportCXXError(scope.bodyStart, 6, 4, 6)
            if is_empty_default:
                # empty default case
                self.reportCXXError(scope.bodyStart, 6, 4, 6)

        for scope in cfg.scopes:
            if scope.type != 'Switch':
                continue
            check_default_in_switch_scope(scope)

    def misra_cpp_6_4_8(self, cfg):
        for scope in cfg.scopes:
            if scope.type != 'Switch':
                continue
            tok = scope.bodyStart.next
            while tok and tok != scope.bodyEnd:
                if tok.str == 'case':
                    break
                tok = tok.next
            if tok and tok.str != 'case':
                    self.reportCXXError(scope.bodyStart, 6, 4, 8)

    def misra_cpp_7_4_2(self, cfg):
        for directive in cfg.directives:
            if directive.str == '#pragma asm':
                self.reportCXXError(directive, 7, 4, 2)

    def misra_cpp_8_5_3(self, cfg):
    # the structure of the token processing is copied from misra_8_12
        for scope in cfg.scopes:
            if scope.type != 'Enum':
                continue
            exist_not_assignment = False
            exist_not_first_assignment = False
            is_first = True
            e_token = scope.bodyStart.next
            while e_token != scope.bodyEnd:
                if e_token.str == '(':
                    # assign e_token to corresponding ')'
                    e_token = e_token.link
                    continue
                if e_token.previous.str not in ',{':
                    # e_token.previous not in ',{', then e_token is not a member's name
                    e_token = e_token.next
                    continue
                if e_token.isName and e_token.values and e_token.valueType and e_token.valueType.typeScope == scope:
                    if e_token.next.str != "=":
                        exist_not_assignment = True
                    elif not is_first:
                        exist_not_first_assignment = True
                    is_first = False

                    if exist_not_assignment and exist_not_first_assignment:
                        self.reportCXXError(scope.bodyStart, 8, 5, 3)
                        break
                e_token = e_token.next

    def misra_cpp_16_0_1(self, data, rawTokens):
        token_in_file = {}
        using_namespace = {}
        for token in data.tokenlist:
            if token.file not in token_in_file:
                token_in_file[token.file] = int(token.linenr)
            else:
                token_in_file[token.file] = min(token_in_file[token.file], int(token.linenr))
        for raw in rawTokens:
            if raw.str.startswith("namespace"):
                using_namespace[token.file] = int(raw.linenr)
                break
        for directive in data.directives:
            if not directive.str.startswith('#include'):
                continue
            if directive.file not in token_in_file:
                continue
            if using_namespace and directive.file in using_namespace.keys() and using_namespace[directive.file] < int(directive.linenr):
                self.reportCXXError(directive, 16, 0, 1)
            if token_in_file[directive.file] < int(directive.linenr):
                self.reportCXXError(directive, 16, 0, 1)

    def misra_cpp_16_0_2(self, rawTokens):
        def find_keywords(start, end):
            t = start
            res = []
            while t != end:
                if simpleMatch(t, "# define") or simpleMatch(t, "# undef"):
                    res.append(t)
                t = t.next
            return res

        for token in rawTokens:
            if token.str == "{":
                endToken = findRawLink(token)
                finds = find_keywords(token, endToken)
                for f in finds:
                    self.reportCXXError(f, 16, 0, 2)

    def misra_cpp_16_0_8(self, data):
        # Based on method: misra_20_13
        preprocessing_tokens = [
            'define', 'undef', 'error', 'include', 'pragma', 'warning',
            'if', 'ifdef', 'ifndef', 'elif', 'else', 'endif'
        ]
        dir_pattern = re.compile(r'#\s*([_\d\w]*)\s*([^\s]*)')
        for directive in data.directives:
            mo = dir_pattern.match(directive.str)
            if mo:
                token = mo.group(1)
                if token not in preprocessing_tokens:
                    self.reportCXXError(directive, 16, 0, 8)
                if token in ['if', 'ifdef', 'ifndef', 'elif'] and not mo.group(2):
                    self.reportCXXError(directive, 16, 0, 8)
                if token in ['else', 'endif'] and mo.group(2):
                    self.reportCXXError(directive, 16, 0, 8)

    def misra_cpp_16_1_1(self, rawTokens):
        defined_alias = []
        for tok in rawTokens:
            # 1) collect defined aliases from macros
            if not tok.previous or not tok.next:
                continue
            if tok.str == 'define' and tok.previous.str == '#':
                alias_tok = tok.next
                # if there is the macro defined alias (X in '#define X defined')
                if alias_tok.next.str == 'defined' and alias_tok.str not in defined_alias:
                    defined_alias.append(alias_tok.str)
            # 2) find out whether 'defined...' is in a standard form
            if (tok.str != 'defined') and (tok.str not in defined_alias):
                continue
            if not tok.previous.previous:
                continue
            if (tok.previous.str == 'if' or tok.previous.str == 'elif') and tok.previous.previous.str == '#':
                # Using defined alias after #if or #elif is non-compliant, e.g., '#if DEFINED X'.
                if tok.str in defined_alias:
                    self.reportCXXError(tok, 16, 1, 1)
                    continue
                # This 'none' case is not in the standard forms, e.g., '#if defined'.
                if not tok.next:
                    self.reportCXXError(tok, 16, 1, 1)
                    continue
                if tok.next.str == '(':
                    # This 'none' case is not in the standard forms, e.g., '#if defined (', '#if defined (X'.
                    if not tok.next.next or not tok.next.next.next:
                        self.reportCXXError(tok, 16, 1, 1)
                        continue
                    # More than one token between brackets such as '#if defined (XX)' is non-compliant.
                    if tok.next.next.next.str != ')':
                        self.reportCXXError(tok, 16, 1, 1)

    def misra_cpp_16_2_1(self, data):
        ifndef_re = re.compile(r'#ifndef ([A-Za-z0-9_]+)')
        define_re = re.compile(r'#define ([A-Za-z0-9_]+)')
        # recorder for #ifndef and #define names
        ifndef_name_by_file = dict()
        define_name_by_file = dict()
        # recorder for matching of #ifndef/#ifdef and #endif
        if_stack = dict()
        for directive in data.directives:
            # 1) file inclusion: compliant
            if directive.str.startswith('#include '):
                continue
            # 2) other types of processors (not in header files): non-compliant
            if not is_header(directive.file):
                self.reportCXXError(directive, 16, 2, 1)
                continue
            # 3) include guards: compliant
                # For compliant include guards:
                # a. There must be #ifndef - #define - #endif pairs.
                # b. Identifiers following #ifndef and #define should be the same.
                # c. There is any other processors outsides the processor pairs.
            # consider cases that the source file includes many header files
            if directive.file not in ifndef_name_by_file.keys():
                ifndef_name_by_file[directive.file] = ''
            if directive.file not in define_name_by_file.keys():
                define_name_by_file[directive.file] = ''
            if directive.file not in if_stack.keys():
                if_stack[directive.file] = []
            # check the pairing of #ifndef, #define and #endif
            if directive.str.startswith('#ifndef '):
                # True or False is to indicate whether this #ifndef is in include guards.
                # If there is a paired #define, this value will be modified to True,
                # otherwise leave it as False.
                if_stack[directive.file].append(False)
                # TODO: Check whether there are any other codes
                # (except comments) in the start of the header file.
                if ifndef_name_by_file[directive.file] != '':
                    # #ifndef exists when another #ifndef is seen
                    self.reportCXXError(directive, 16, 2, 1)
                    continue
                match = ifndef_re.match(directive.str)
                if not match:
                    self.reportCXXError(directive, 16, 2, 1)
                    continue
                ifndef_name_by_file[directive.file] = match.group(1)
            elif directive.str.startswith('#define '):
                if ifndef_name_by_file[directive.file] == '':
                    # #ifndef does not exist when #define is seen
                    self.reportCXXError(directive, 16, 2, 1)
                    continue
                match = define_re.match(directive.str)
                if not match:
                    self.reportCXXError(directive, 16, 2, 1)
                    continue
                name = match.group(1)
                if name != ifndef_name_by_file[directive.file]:
                    # #ifndef and #define names are not same
                    self.reportCXXError(directive, 16, 2, 1)
                    continue
                ifndef_name_by_file[directive.file] = ''
                define_name_by_file[directive.file] = name
                if_stack[directive.file][-1] = True
            elif directive.str == '#endif':
                isIncludeGuards = if_stack[directive.file].pop()
                # TODO: Check whether there are any other codes
                # (except comments) in the end of the header file.
                if define_name_by_file[directive.file] == '':
                    # #define does not exist when #endif is seen
                    self.reportCXXError(directive, 16, 2, 1)
                    continue
                if isIncludeGuards:
                    define_name_by_file[directive.file] == ''
                    continue
                self.reportCXXError(directive, 16, 2, 1)
            # 4) other types of processors (in header files): non-compliant
            else:
                if directive.str.startswith("#if ") or directive.str.startswith("#ifdef "):
                    # It can not be include guards.
                    if_stack[directive.file].append(False)
                self.reportCXXError(directive, 16, 2, 1)

    def isClassSpecifierOrTypeQualifier(self, directive):
        """
        This function is to check whether the directive is a class specifier or type qualifier.

        :param directive: the directive to be checked
        :type directive: class Define object
        :return: whether is a class specifier or type qualifier or not
        :rtype: bool
        """
        storage_class_specifiers = ["auto", "register", "static", "extern", "thread_local", "mutable"]
        type_qualifiers = ["const", "volatile", "restrict"]
        if directive.name == '' and directive.args == [] and directive.expansionList == '':
            return False
        # not macros
        if directive.name == '' or directive.expansionList == '' or directive.isFunction:
            return False
        if directive.expansionList in storage_class_specifiers or directive.expansionList in type_qualifiers:
            return True
        return False

    def misra_cpp_16_2_2(self, data):
        ifndef_re = re.compile(r'#ifndef ([A-Za-z0-9_]+)')
        define_re = re.compile(r'#define ([A-Za-z0-9_]+)')
        undef_re = re.compile(r'#undef ([A-Za-z0-9_]+)')
        # recorder for #ifndef and #define names in include guards
        ifndef_name_by_file = dict()
        define_name_by_file = dict()
        # recorder for matching of #define and #undef
        define_name_list = dict()
        # recorder for matching of #ifndef/#ifdef and #endif
        if_stack = dict()
        for directive in data.directives:
            # consider cases that the source file includes many files
            if directive.file not in ifndef_name_by_file.keys():
                ifndef_name_by_file[directive.file] = ''
            if directive.file not in define_name_by_file.keys():
                define_name_by_file[directive.file] = ''
            if directive.file not in define_name_list.keys():
                define_name_list[directive.file] = []
            if directive.file not in if_stack.keys():
                if_stack[directive.file] = []

            # 1) Check whether it is a class specifier or type qualifier.
            if directive.str.startswith('#define '):
                d = Define(directive)
                if self.isClassSpecifierOrTypeQualifier(d):
                    define_name_list[directive.file].append(d.name)
                    continue
                if not is_header(directive.file):
                    self.reportCXXError(directive, 16, 2, 2)
            # 2) Check whether the #undef macro are paired with a #define.
            elif directive.str.startswith('#undef '):
                match = undef_re.match(directive.str)
                if not match:
                    self.reportCXXError(directive, 16, 2, 2)
                    continue
                name = match.group(1)
                if name not in define_name_list[directive.file]:
                    self.reportCXXError(directive, 16, 2, 2)
                    continue
                define_name_list[directive.file].remove(name)
                continue
            # 3) Check whether the #define macro is used in include guards.
            # Note: most of the following codes are copied from misra_cpp_16_2_1.
            # The difference is that it only reports errors in #define macros.
            if is_header(directive.file):
                # include guards: compliant
                    # For compliant include guards:
                    # a. There must be #ifndef - #define - #endif pairs.
                    # b. Identifiers following #ifndef and #define should be the same.
                    # c. There is any other processors outsides the processor pairs.
                # check the pairing of #ifndef, #define and #endif
                if directive.str.startswith('#ifndef '):
                    # True or False is to indicate whether this #ifndef is in include guards.
                    # If there is a paired #define, this value will be modified to True,
                    # otherwise leave it as False.
                    if_stack[directive.file].append(False)
                    # TODO: Check whether there are any other codes
                    # (except comments) in the start of the header file.
                    if ifndef_name_by_file[directive.file] != '':
                        # #ifndef exists when another #ifndef is seen
                        continue
                    match = ifndef_re.match(directive.str)
                    if not match:
                        continue
                    ifndef_name_by_file[directive.file] = match.group(1)
                elif directive.str.startswith('#define '):
                    if ifndef_name_by_file[directive.file] == '':
                        # #ifndef does not exist when #define is seen
                        self.reportCXXError(directive, 16, 2, 2)
                        continue
                    match = define_re.match(directive.str)
                    if not match:
                        self.reportCXXError(directive, 16, 2, 2)
                        continue
                    name = match.group(1)
                    if name == ifndef_name_by_file[directive.file]:
                        ifndef_name_by_file[directive.file] = ''
                        define_name_by_file[directive.file] = name
                        if_stack[directive.file][-1] = True
                        continue
                    self.reportCXXError(directive, 16, 2, 2)
                elif directive.str == '#endif':
                    isIncludeGuards = if_stack[directive.file].pop()
                    # TODO: Check whether there are any other codes
                    # (except comments) in the end of the header file.
                    if define_name_by_file[directive.file] == '':
                        # #define does not exist when #endif is seen
                        continue
                    if isIncludeGuards:
                        define_name_by_file[directive.file] == ''
                # record #if and #ifdef in if_stack
                elif directive.str.startswith("#if ") or directive.str.startswith("#ifdef "):
                    # It can not be include guards.
                    if_stack[directive.file].append(False)

    def misra_cpp_16_2_4(self, data):
        for directive in data.directives:
            if not directive.str.startswith('#include '):
                continue
            if "<" in directive.str:
                if '"' in directive.str:
                    self.reportCXXError(directive, 16, 2, 4)
            for pattern in ('//', '/*', "'"):
                if pattern in directive.str:
                    self.reportCXXError(directive, 16, 2, 4)

    def misra_cpp_16_3_1(self, data):
        single = False
        double = False
        for directive in data.directives:
            d = Define(directive)
            if d.expansionList.find('#') >= 1:
                single = True
            if d.expansionList.find("##") >= 1:
                double = True
            if single and double:
                self.reportCXXError(directive, 16, 3, 1)
            if d.expansionList.find('#') >= 2 or d.expansionList.find('##') >= 2:
                self.reportCXXError(directive, 16, 3, 1)

    def misra_cpp_17_0_3(self, cfg):
        # refer to misra_21_2
        for func in cfg.functions:
            if isStdLibId(func.name, cfg.standards.c):
                tok = func.tokenDef if func.tokenDef else func.token
                self.reportCXXError(tok, 17, 0, 3)

    def misra_cpp_18_0_1(self, cfg):
        # 参考：https://zh.cppreference.com/w/c/header
        re_c_library = re.compile(r"#\s*include\s*<(" +
            "(assert\.h)|" + # 条件编译宏，将参数与零比较
            "(complex\.h)|" + # 复数运算
            "(ctype\.h)|" + # 用来确定包含于字符数据中的类型的函数
            "(errno\.h)|" + # 报告错误条件的宏
            "(fenv\.h)|" + # 浮点环境
            "(float\.h)|" + # 浮点类型的极限
            "(inttypes\.h)|" + # 整数类型的格式转换
            "(iso646\.h)|" + # 运算符的替代写法
            "(limits\.h)|" + # 整数类型的范围
            "(locale\.h)|" + # 本地化工具
            "(math\.h)|" + # 常用数学函数
            "(setjmp\.h)|" + # 非局部跳转
            "(signal\.h)|" + # 信号处理
            "(stdalign\.h)|" + # alignas 与 alignof 便利宏
            "(stdarg\.h)|" + # 可变参数
            "(stdatomic\.h)|" + # 原子操作
            "(stdbool\.h)|" + # 布尔类型的宏
            "(stddef\.h)|" + # 常用宏定义
            "(stdint\.h)|" + # 定宽整数类型
            "(stdio\.h)|" + # 输入/输出
            "(stdlib\.h)|" + # 基础工具：内存管理、程序工具、字符串转换、随机数、算法
            "(stdnoreturn\.h)|" + # noreturn 便利宏
            "(string\.h)|" + # 字符串处理
            "(tgmath\.h)|" + # 泛型数学（包装 math.h 和 complex.h 的宏）
            "(threads\.h)|" + # 线程库
            "(time\.h)|" + # 时间/日期工具
            "(uchar\.h)|" + # UTF-16 和 UTF-32 字符工具
            "(wchar\.h)|" + # 扩展多字节和宽字符工具
            "(wctype\.h)" + # 用来确定包含于宽字符数据中的类型的函数
            ")>"
        )
        for directive in cfg.directives:
            if re_c_library.match(directive.str):
                self.reportCXXError(directive, 18, 0, 1)

    def misra_cpp_27_0_1(self, cfg):
        re_cstdio_library = re.compile(r"#\s*include\s*<cstdio>")
        for directive in cfg.directives:
            if re_cstdio_library.match(directive.str):
                self.reportCXXError(directive, 27, 0, 1)

    # cppcheck automatically adds curly brackets for clauses,
    # so bodyStart and bodyEnd must be "{" and "}".
    def cwe_483(self, cfg):
        for scope in cfg.scopes:
            if scope.type not in ('If', 'Else', 'While', 'For'):
                continue
            token_before_curly_brackets = scope.bodyStart.previous
            if token_before_curly_brackets.str == ")":
                keyword_token = token_before_curly_brackets.link.previous
            else: # keyword_token is "else"
                keyword_token = token_before_curly_brackets
            next_token = scope.bodyEnd.next
            if keyword_token is None or next_token is None:
                continue
            if next_token.column > keyword_token.column:
                if keyword_token.str == "if" and next_token.str == "else":
                    continue
                self.reportCWEError(next_token, 483)

    # Use comment to indicate field name for structured bindings
    def g1206(self, data, rawTokens):
        # collect tokens expected to be comments from rawTokens
        potential_comments_by_bind = dict()
        for tok in rawTokens:
            if tok.str == 'auto' and tok.next.str == '[':
                auto_tok = tok
                current_tok = tok
                while current_tok.str != '=' and current_tok.str != ';':
                    current_tok = current_tok.next
                # There may not be a '=' before the statement ends.
                if current_tok.str != '=':
                    continue
                if current_tok.next == None:
                    continue
                # We cannot get scope information of bindings and make clear
                # whether they are structured bindings in rawTokens, so should
                # not check comments here for its binding may not be a
                # structured binding. Record potential comments anyway.
                bind_name = current_tok.next.file + ':' + str(current_tok.next.linenr) + ':' + current_tok.next.str
                if bind_name not in potential_comments_by_bind.keys():
                    potential_comments_by_bind[bind_name] = []
                # Collected tokens may not be comments but potential comments.
                potential_comment_tok = auto_tok.next.next
                # If correctly commented, the next.next.next token of the last
                # potential comment token will be '='.
                # Otherwise, there are two possible cases:
                # 1) It is other tokens around '=', record it anyway which will
                # be checked as non-compliant later.
                # 2) Its line number is larger than that of '=', which must be
                # out of the checking scope, whatever with folded lines or not.
                while potential_comment_tok != None and potential_comment_tok != current_tok and potential_comment_tok.linenr <= current_tok.linenr:
                    potential_comments_by_bind[bind_name].append(potential_comment_tok.str)
                    potential_comment_tok = potential_comment_tok.next.next.next
        # check comments according to the target struct scope
        for tok in data.tokenlist:
            if tok.str == 'auto' and tok.next.str == '[':
                auto_tok = tok
                current_tok = tok
                while current_tok.str != '=' and current_tok.str != ';':
                    current_tok = current_tok.next
                # There may not be a '=' before the statement ends.
                if current_tok.str != '=':
                    continue
                name_tok = current_tok.next
                if name_tok == None or name_tok.valueType == None:
                    continue
                target_scope = name_tok.valueType.typeScope
                if target_scope.type != 'Struct':
                    # It is not a structured binding.
                    continue
                bind_struct_name = name_tok.file + ':' + str(name_tok.linenr) + ':' + name_tok.str
                for i in range(len(target_scope.varlist)):
                    if i >= len(potential_comments_by_bind[bind_struct_name]):
                        # It should not reach here, but if it does, it must be
                        # non-compliant.
                        self.reportGoogleCPPError(auto_tok, 1206)
                        break
                    comment_str = potential_comments_by_bind[bind_struct_name][i]
                    if not comment_str.startswith('/*') or not comment_str.endswith('=*/'):
                        # It does not match the compliant format of a comment
                        # compilant format should be like '/*field_name=*/'.
                        self.reportGoogleCPPError(auto_tok, 1206)
                        break
                    # [2:-3]: trim '/*' and '=*/'
                    if comment_str[2:-3] != target_scope.varlist[i].nameToken.str:
                        # The name of the underlying field does not match the
                        # name in the comment.
                        self.reportGoogleCPPError(auto_tok, 1206)
                        break

    # Don't use <ratio>
    def g1213(self, cfg):
        re_ratio_library = re.compile(r"#\s*include\s*<ratio>")
        for directive in cfg.directives:
            if re_ratio_library.match(directive.str):
                self.reportGoogleCPPError(directive, 1213)

    # Don't use <cfenv> <fenv.h>
    def g1214(self, cfg):
        re_cfenv_library = re.compile(r"#\s*include\s*<cfenv>")
        re_fenv_library = re.compile(r"#\s*include\s*<fenv.h>")
        for directive in cfg.directives:
            if re_cfenv_library.match(directive.str) or re_fenv_library.match(directive.str):
                self.reportGoogleCPPError(directive, 1214)

    # Don't use <filesystem>
    def g1215(self, cfg):
        re_filesystem_library = re.compile(r"#\s*include\s*<filesystem>")
        for directive in cfg.directives:
            if re_filesystem_library.match(directive.str):
                self.reportGoogleCPPError(directive, 1215)

    def naive_systems_C7966(self, cfg):
        for token in cfg.tokenlist:
            if not (isFunctionCall(token) and token.previous.str == "malloc"):
                continue
            if token.next.getKnownIntValue() and token.next.getKnownIntValue() % 4 != 0:
                self.reportNaiveSystemsError(token, 7966)

    # Don't use "'", '"', "/*", "//", "\\" in headers' name
    def rule_A16_2_1(self, cfg):
        re_include_directive = re.compile(r'#\s*include\s*[<"](.*?)[">]')
        for directive in cfg.directives:
            match = re_include_directive.match(directive.str)
            if match:
                filename = match.group(1)
                for char in ["'", '"', "/*", "//", "\\"]:
                    if char in filename:
                        self.reportAUTOSARError(directive, 16, 2, 1)
                        break  # Only report once per include directive

    def rule_A16_0_1(self, cfg, rawTokens):
        # compliant directives:
        # 1) conditional file inclusion:
            # a. #ifdef, #if, #ifndef and #if defined must be paired with #endif
            # b. only file inclusions can occur in these branches
        # 2) include guard:
            # a. follow the pattern of #ifndef / #if !defined - #define - #endif
            # b. the guard identifier in #define must be the same as that in #ifndef / #if !defined
            # c. only one guard can be defined in each file

        CONDITIONAL_INCLUSION = 1
        INCLUDE_GUARD = 2

        # recorder for include guard identifiers
        guard_name_by_file = dict()
        # whether in an include guard branch
        is_in_include_guard_by_file = dict()
        # recorder for the occurance of #ifdef, #if, #ifndef and #if defined and their type (file inclusion / include guard)
        branch_type_stack_by_file = dict()
        # recorder for the line numbers of #ifdef, #if, #ifndef and #if defined (the beginning of each conditional file inclusion)
        if_linenr_stack_by_file = dict()
        # recorder for the line number ranges of branches (only for conditional file inclusion)
        branch_linenr_ranges_by_file = dict()

        for directive in cfg.directives:
            # initialization for each file (consider cases that the source file includes many header files)
            if directive.file not in guard_name_by_file.keys():
                guard_name_by_file[directive.file] = ''
            if directive.file not in is_in_include_guard_by_file.keys():
                is_in_include_guard_by_file[directive.file] = False
            if directive.file not in branch_type_stack_by_file.keys():
                branch_type_stack_by_file[directive.file] = []
            if directive.file not in if_linenr_stack_by_file.keys():
                if_linenr_stack_by_file[directive.file] = []
            if directive.file not in branch_linenr_ranges_by_file.keys():
                branch_linenr_ranges_by_file[directive.file] = []

            if directive.str.startswith('#include '):
                # file inclusion: compliant
                continue

            elif directive.str.startswith('#ifndef '):
                ifndef_re = re.compile(r'#ifndef ([A-Za-z0-9_]+)')
                match = ifndef_re.match(directive.str)
                if match and guard_name_by_file[directive.file] == '':
                    # consider as an include guard (no include guard appears in this file before this one)
                    guard_name_by_file[directive.file] = match.group(1) # record the include guard name for this file
                    branch_type_stack_by_file[directive.file].append(INCLUDE_GUARD) # mark this branch as an include guard
                    is_in_include_guard_by_file[directive.file] = True # mark the beginning of the include guard
                else:
                    # consider as a conditional file inclusion
                    branch_type_stack_by_file[directive.file].append(CONDITIONAL_INCLUSION) # mark this branch as an conditional file inclusion
                    if_linenr_stack_by_file[directive.file].append(directive.linenr)


            elif directive.str.startswith('#if '):
                # check whether it is the second pattern of valid include guard (if !defined)
                ifndef2_re = re.compile(r'#if !\s*defined\s*\(([A-Za-z0-9_]+)\)')
                match = ifndef2_re.match(directive.str)
                if match and guard_name_by_file[directive.file] == '':
                    # consider as an include guard
                    guard_name_by_file[directive.file] = match.group(1)
                    branch_type_stack_by_file[directive.file].append(INCLUDE_GUARD)
                    is_in_include_guard_by_file[directive.file] = True
                else:
                    # consider as a conditional file inclusion
                    branch_type_stack_by_file[directive.file].append(CONDITIONAL_INCLUSION)
                    if_linenr_stack_by_file[directive.file].append(directive.linenr)

            elif directive.str.startswith('#define '):
                if not is_in_include_guard_by_file[directive.file]:
                    # this #define is not in an include guard branch
                    self.reportAUTOSARError(directive, 16, 0, 1)
                    continue
                define_re = re.compile(r'#define ([A-Za-z0-9_]+)')
                match = define_re.match(directive.str)
                if not match:
                    # invalid name after #define
                    self.reportAUTOSARError(directive, 16, 0, 1)
                    continue
                name = match.group(1)
                if name != guard_name_by_file[directive.file]:
                    # include guard name and #define name are not same
                    self.reportAUTOSARError(directive, 16, 0, 1)

            elif directive.str == '#endif':
                branch_type = branch_type_stack_by_file[directive.file].pop()
                if not branch_type:
                    # redundant #endif
                    self.reportAUTOSARError(directive, 16, 0, 1)
                    continue
                if branch_type == INCLUDE_GUARD:
                    # this #endif is for an include guard branch
                    is_in_include_guard_by_file[directive.file] = False # mark the end of the include guard
                else:
                    # this #endif is for a conditional file inclusion
                    begin_linenr = if_linenr_stack_by_file[directive.file].pop()
                    branch_linenr_ranges_by_file[directive.file].append((begin_linenr, directive.linenr)) # record the line number range of a conditional file inclusion branch

            elif directive.str.startswith('#ifdef ') or directive.str.startswith('#if defined '):
                # beginning of a conditional file inclusion
                branch_type_stack_by_file[directive.file].append(CONDITIONAL_INCLUSION)
                if_linenr_stack_by_file[directive.file].append(directive.linenr)

            elif directive.str.startswith('#elif ') or directive.str == '#else':
                # continuation of a conditional file inclusion
                branch_type = branch_type_stack_by_file[directive.file].pop()
                if not branch_type:
                    self.reportAUTOSARError(directive, 16, 0, 1)
                    continue
                if branch_type == INCLUDE_GUARD:
                    # #elif and #else should not be used for an include guard
                    self.reportAUTOSARError(directive, 16, 0, 1)
                branch_type_stack_by_file[directive.file].append(branch_type)

            else:
                # other not allowed directives
                self.reportAUTOSARError(directive, 16, 0, 1)

        # check if there are other codes except directives in each conditional file inclusion branch
        last_token_linenr = 0
        for token in rawTokens:
            # only process on the first token of each line
            if token.linenr == last_token_linenr:
                continue
            last_token_linenr = token.linenr

            if not token.str == '#':
                # not a directive
                # check whether this token is in a conditional file inclusion branch
                if directive.file in branch_linenr_ranges_by_file.keys():
                    for linenr_range in branch_linenr_ranges_by_file[token.file]:
                        if token.linenr > linenr_range[0] and token.linenr <= linenr_range[1]:
                            self.reportAUTOSARError(token, 16, 0, 1)
                            break

    # Don't use #error
    def rule_A16_6_1(self, cfg):
        re_error_library = re.compile(r"#\s*error")
        for directive in cfg.directives:
            if re_error_library.match(directive.str):
                self.reportAUTOSARError(directive, 16, 6, 1)

    # Don't use #pragma
    def rule_A16_7_1(self, cfg):
        re_pragma_library = re.compile(r"#\s*pragma")
        for directive in cfg.directives:
            if re_pragma_library.match(directive.str):
                self.reportAUTOSARError(directive, 16, 7, 1)

    # Dont' use <clocale> or <locale.h> and the setlocale function
    def rule_A18_0_3(self, cfg):
        re_clocale_library = re.compile(r"#\s*include\s*<clocale>")
        re_locale_h_library = re.compile(r"#\s*include\s*<locale.h>")
        re_setlocale_function = re.compile(r"\bsetlocale\b")
        for directive in cfg.directives:
            if re_clocale_library.match(directive.str) or re_locale_h_library.match(directive.str):
                self.reportAUTOSARError(directive, 18, 0, 3)
        for token in cfg.tokenlist:
            if re_setlocale_function.match(token.str) and simpleMatch(token.next, '('):
                name, _ = cppcheckdata.get_function_call_name_args(token)
                if name and name == token.str:
                    self.reportAUTOSARError(token, 18, 0, 3)

    def get_verify_expected(self):
        """Return the list of expected violations in the verify test"""
        return self.verify_expected

    def get_verify_actual(self):
        """Return the list of actual violations in for the verify test"""
        return self.verify_actual

    def get_violations(self, violation_type=None):
        """Return the list of violations for a normal checker run"""
        if violation_type is None:
            return self.violations.items()
        else:
            return self.violations[violation_type]

    def get_violation_types(self):
        """Return the list of violations for a normal checker run"""
        return self.violations.keys()

    def addSuppressedRule(self, ruleNum,
                          fileName=None,
                          lineNumber=None,
                          symbolName=None):
        """
        Add a suppression to the suppressions data structure

        Suppressions are stored in a dictionary of dictionaries that
        contains a list of tuples.

        The first dictionary is keyed by the MISRA rule in hundreds
        format. The value of that dictionary is a dictionary of filenames.
        If the value is None then the rule is assumed to be suppressed for
        all files.
        If the filename exists then the value of that dictionary contains a list
        with the scope of the suppression.  If the list contains an item of None
        then the rule is assumed to be suppressed for the entire file. Otherwise
        the list contains line number, symbol name tuples.
        For each tuple either line number or symbol name can can be none.

        """
        normalized_filename = None

        if fileName is not None:
            normalized_filename = os.path.expanduser(fileName)
            normalized_filename = os.path.normpath(normalized_filename)

        if lineNumber is not None or symbolName is not None:
            line_symbol = (lineNumber, symbolName)
        else:
            line_symbol = None

        # If the rule is not in the dict already then add it
        if ruleNum not in self.suppressedRules:
            ruleItemList = list()
            ruleItemList.append(line_symbol)

            fileDict = dict()
            fileDict[normalized_filename] = ruleItemList

            self.suppressedRules[ruleNum] = fileDict

            # Rule is added.  Done.
            return

        # Rule existed in the dictionary. Check for
        # filename entries.

        # Get the dictionary for the rule number
        fileDict = self.suppressedRules[ruleNum]

        # If the filename is not in the dict already add it
        if normalized_filename not in fileDict:
            ruleItemList = list()
            ruleItemList.append(line_symbol)

            fileDict[normalized_filename] = ruleItemList

            # Rule is added with a file scope. Done
            return

        # Rule has a matching filename. Get the rule item list.

        # Check the lists of rule items
        # to see if this (lineNumber, symbolName) combination
        # or None already exists.
        ruleItemList = fileDict[normalized_filename]

        if line_symbol is None:
            # is it already in the list?
            if line_symbol not in ruleItemList:
                ruleItemList.append(line_symbol)
        else:
            # Check the list looking for matches
            matched = False
            for each in ruleItemList:
                if each is not None:
                    if (each[0] == line_symbol[0]) and (each[1] == line_symbol[1]):
                        matched = True

            # Append the rule item if it was not already found
            if not matched:
                ruleItemList.append(line_symbol)

    def isRuleSuppressed(self, file_path, linenr, ruleNum):
        """
        Check to see if a rule is suppressed.

        :param ruleNum: is the rule number in hundreds format
        :param file_path: File path of checked location
        :param linenr: Line number of checked location

        If the rule exists in the dict then check for a filename
        If the filename is None then rule is suppressed globally
        for all files.
        If the filename exists then look for list of
        line number, symbol name tuples.  If the list is None then
        the rule is suppressed for the entire file
        If the list of tuples exists then search the list looking for
        matching line numbers.  Symbol names are currently ignored
        because they can include regular expressions.
        TODO: Support symbol names and expression matching.

        """
        ruleIsSuppressed = False

        # Remove any prefix listed in command arguments from the filename.
        filename = None
        if file_path is not None:
            if self.filePrefix is not None:
                filename = remove_file_prefix(file_path, self.filePrefix)
            else:
                filename = os.path.basename(file_path)

        if ruleNum in self.suppressedRules:
            fileDict = self.suppressedRules[ruleNum]

            # a file name entry of None means that the rule is suppressed
            # globally
            if None in fileDict:
                ruleIsSuppressed = True
            else:
                # Does the filename match one of the names in
                # the file list
                if filename in fileDict:
                    # Get the list of ruleItems
                    ruleItemList = fileDict[filename]

                    if None in ruleItemList:
                        # Entry of None in the ruleItemList means the rule is
                        # suppressed for all lines in the filename
                        ruleIsSuppressed = True
                    else:
                        # Iterate though the the list of line numbers
                        # and symbols looking for a match of the line
                        # number.  Matching the symbol is a TODO:
                        for each in ruleItemList:
                            if each is not None:
                                if each[0] == linenr:
                                    ruleIsSuppressed = True

        return ruleIsSuppressed

    def isRuleGloballySuppressed(self, rule_num):
        """
        Check to see if a rule is globally suppressed.
        :param rule_num: is the rule number in hundreds format
        """
        if rule_num not in self.suppressedRules:
            return False
        return None in self.suppressedRules[rule_num]

    def showSuppressedRules(self):
        """
        Print out rules in suppression list sorted by Rule Number
        """
        print("Suppressed Rules List:")
        outlist = list()

        for ruleNum in self.suppressedRules:
            fileDict = self.suppressedRules[ruleNum]

            for fname in fileDict:
                ruleItemList = fileDict[fname]

                for item in ruleItemList:
                    if item is None:
                        item_str = "None"
                    else:
                        item_str = str(item[0])

                    outlist.append("%s: %s: %s (%d locations suppressed)" % (
                    float(ruleNum) / 100, fname, item_str, self.suppressionStats.get(ruleNum, 0)))

        for line in sorted(outlist, reverse=True):
            print("  %s" % line)

    def setFilePrefix(self, prefix):
        """
        Set the file prefix to ignore from files when matching
        suppression files
        """
        self.filePrefix = prefix

    def setSeverity(self, severity):
        """
        Set the severity for all errors.
        """
        self.severity = severity

    def setSuppressionList(self, suppressionlist):
        num1 = 0
        num2 = 0
        rule_pattern = re.compile(r'([0-9]+).([0-9]+)')
        strlist = suppressionlist.split(",")

        # build ignore list
        for item in strlist:
            res = rule_pattern.match(item)
            if res:
                num1 = int(res.group(1))
                num2 = int(res.group(2))
                ruleNum = (num1 * 100) + num2

                self.addSuppressedRule(ruleNum)

    def reportNaiveSystemsError(self, location, num, other_locations = None):
        ruleNum = num
        if self.settings.verify:
            self.verify_actual.append('%s:%d' % (location.file, location.linenr, num))
        else:
            errorId = f"C{num}"
            naiveSystems_severity = 'Required'
            this_violation = '{}-{}-{}-{}'.format(location.file, location.linenr, location.column, ruleNum)
            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                self.current_json_list.append(NaiveSystemsResult(location.file, location.linenr, errorId, other_locations))
                cppcheckdata.reportError(location, naiveSystems_severity, "", "naivesystems", errorId)
                if naiveSystems_severity not in self.violations:
                    self.violations[naiveSystems_severity] = []
                self.violations[naiveSystems_severity].append('naivesystems' + "-" + errorId)

    def formExternalMessage(self, location, num1, num2, category):
        """Form the external message to report.
        location: The token to report an error message.
        num1, num2: Error numbers to specify the rule number and the error kind.
        category: A list of categories of errors reported. It is used as
            indicators to report external messages. The usage may be different
            for different rules.
        """
        if not category:
            return ''
        # check for misra_rule_10_1
        if num1 != 10 or num2 != 1:
            return ''
        if len(category) == 1:
            # unary operand
            return 'The operand of the %s operator is of an inappropriate essential type category %s.' % (location.str, category[0])
        if len(category) == 2:
            # binary operand
            left_message = ''
            if category[0] != '':
                left_message = 'The left operand of the %s operator is of an inappropriate essential type category %s.' % (location.str, category[0])
            if category[1] != '':
                right_message = 'The right operand of the %s operator is of an inappropriate essential type category %s.' % (location.str, category[1])
                if left_message != '':
                    return left_message + '\n' + right_message
                return right_message
            return left_message
        return ''

    def reportError(self, location, num1, num2, case_number = 0, optional_flag = False, other_locations = None, category = None):
        ruleNum = num1 * 100 + num2

        if self.isRuleGloballySuppressed(ruleNum):
            return

        if self.settings.verify:
            self.verify_actual.append('%s:%d %d.%d' % (location.file, location.linenr, num1, num2))
        elif self.isRuleSuppressed(location.file, location.linenr, ruleNum):
            # Error is suppressed. Ignore
            self.suppressionStats.setdefault(ruleNum, 0)
            self.suppressionStats[ruleNum] += 1
            return
        else:
            errorId = 'c2012-' + str(num1) + '.' + str(num2)
            misra_severity = 'Undefined'
            cppcheck_severity = 'style'
            if ruleNum in self.ruleTexts:
                errmsg = self.ruleTexts[ruleNum].text
                if self.ruleTexts[ruleNum].misra_severity:
                    misra_severity = self.ruleTexts[ruleNum].misra_severity
                cppcheck_severity = self.ruleTexts[ruleNum].cppcheck_severity
            elif len(self.ruleTexts) == 0:
                errmsg = 'misra violation (use --rule-texts=<file> to get proper output)'
            else:
                errmsg = 'misra violation %s with no text in the supplied rule-texts-file' % (ruleNum)

            if self.severity:
                cppcheck_severity = self.severity

            this_violation = '{}-{}-{}-{}'.format(location.file, location.linenr, location.column, ruleNum)

            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                external_message = self.formExternalMessage(location, num1, num2, category)
                self.current_json_list.append(Result(location.file, location.linenr, errorId, optional_flag = optional_flag, other_locations = other_locations, error_numbers = [num1, num2, case_number], external_message = external_message))
                cppcheckdata.reportError(location, cppcheck_severity, errmsg, 'misra', errorId, misra_severity)

                if misra_severity not in self.violations:
                    self.violations[misra_severity] = []
                self.violations[misra_severity].append('misra-' + errorId)

    def reportMessageError(self, error_message, num1, num2, case_number = 0):
        ruleNum = num1 * 100 + num2
        errorId = f'c2012-{num1}.{num2}'
        misra_severity = 'Undefined'
        cppcheck_severity = 'style'
        if ruleNum in self.ruleTexts:
            misra_severity = self.ruleTexts[ruleNum].misra_severity
        if self.severity:
            cppcheck_severity = self.severity
        this_violation = str(ruleNum)
        if this_violation not in self.existing_violations:
            self.existing_violations.add(this_violation)
            # an valid error message is like:
            # rule_20_6/bad/bad1.c:5:0: error: blabla
            split_messages = error_message.split(":", 3)
            location = cppcheckdata.LocationByDetail(split_messages[0], int(split_messages[1]), int(split_messages[2]))
            self.current_json_list.append(Result(location.file, location.linenr, errorId, error_numbers = [num1, num2, case_number]))
            cppcheckdata.reportError(location, cppcheck_severity, split_messages[3], 'misra', errorId, misra_severity)
            if misra_severity not in self.violations:
                self.violations[misra_severity] = []
            self.violations[misra_severity].append('misra-' + errorId)

    def reportGJBError(self, location, num1, num2, num3, other_locations = None, gjb_type = '5369'):
        # the format of gjb rule numbers is XX.X.XX
        ruleNum = num1 * 1000 + num2 * 100 + num3
        if self.settings.verify:
            self.verify_actual.append('%s:%d %d.%d.%d' % (location.file, location.linenr, num1, num2, num3))
        else:
            errorId = f"{gjb_type}-4.{num1}.{num2}.{num3}" if gjb_type == "5369" else f"{gjb_type}-R-{num1}-{num2}-{num3}"
            gjb_severity = 'Required'
            this_violation = '{}-{}-{}-{}-{}'.format(location.file, location.linenr, location.column, gjb_type, ruleNum)
            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                self.current_json_list.append(GJBResult(location.file, location.linenr, errorId, other_locations))
                cppcheckdata.reportError(location, gjb_severity, "", 'GJB', errorId)
                if gjb_severity not in self.violations:
                    self.violations[gjb_severity] = []
                self.violations[gjb_severity].append('GJB' + gjb_type + '-' + errorId)

    def reportGJBMessageError(self, error_message, num1, num2, num3, gjb_type = '5369'):
        ruleNum = num1 * 1000 + num2 * 100 + num3
        errorId = f"{gjb_type}-4.{num1}.{num2}.{num3}" if gjb_type == "5369" else f"{gjb_type}-R-{num1}-{num2}-{num3}"
        gjb_severity = 'Required'
        cppcheck_severity = 'style'
        if self.severity:
            cppcheck_severity = self.severity
        this_violation = str(ruleNum)
        if this_violation not in self.existing_violations:
            self.existing_violations.add(this_violation)
            split_messages = error_message.split(":", 3)
            location = cppcheckdata.LocationByDetail(split_messages[0], int(split_messages[1]), int(split_messages[2]))
            self.current_json_list.append(GJBResult(location.file, location.linenr, errorId))
            cppcheckdata.reportError(location, cppcheck_severity, split_messages[3], 'GJB', errorId, gjb_severity)
            if gjb_severity not in self.violations:
                self.violations[gjb_severity] = []
            self.violations[gjb_severity].append('GJB' + gjb_type + '-' + errorId)

    def reportCXXError(self, location, num1, num2, num3, other_locations = None):
        ruleNum = num1 * 1000 + num2 * 100 + num3
        if self.settings.verify:
            self.verify_actual.append('%s:%d %d.%d.%d' % (location.file, location.linenr, num1, num2, num3))
        else:
            errorId = f"cxx2008-{num1}.{num2}.{num3}"
            cxx_severity = 'Required'
            this_violation = '{}-{}-{}-{}'.format(location.file, location.linenr, location.column, ruleNum)
            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                self.current_json_list.append(CXXResult(location.file, location.linenr, errorId, other_locations))
                cppcheckdata.reportError(location, cxx_severity, "", "misra", errorId)
                if cxx_severity not in self.violations:
                    self.violations[cxx_severity] = []
                self.violations[cxx_severity].append('misra' + "-" + errorId)


    def reportCWEError(self, location, ruleNum, other_locations = None):
        if self.settings.verify:
            self.verify_actual.append('%s:%d %d' % (location.file, location.linenr, ruleNum))
        else:
            errorId = f"CWE-{ruleNum}"
            cwe_severity = 'Required'
            this_violation = '{}-{}-{}-{}'.format(location.file, location.linenr, location.column, ruleNum)
            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                self.current_json_list.append(CWEResult(location.file, location.linenr, errorId, other_locations))
                cppcheckdata.reportError(location, cwe_severity, "", "CWE", errorId)
                if cwe_severity not in self.violations:
                    self.violations[cwe_severity] = []
                self.violations[cwe_severity].append(errorId)

    def reportGoogleCPPError(self, location, ruleNum, other_locations = None):
        if self.settings.verify:
            self.verify_actual.append('%s:%d %d' % (location.file, location.linenr, ruleNum))
        else:
            errorId = f"g-{ruleNum}"
            google_cpp_severity = 'Required'
            this_violation = '{}-{}-{}-{}'.format(location.file, location.linenr, location.column, ruleNum)
            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                self.current_json_list.append(GoogleCPPResult(location.file, location.linenr, errorId, other_locations))
                cppcheckdata.reportError(location, google_cpp_severity, "", "GoogleCPP", errorId)
                if google_cpp_severity not in self.violations:
                    self.violations[google_cpp_severity] = []
                self.violations[google_cpp_severity].append(errorId)

    def reportAUTOSARError(self, location, num1, num2, num3, other_locations = None):
        ruleNum = num1 * 10000 + num2 * 100 + num3
        if self.settings.verify:
            self.verify_actual.append('%s:%d %d.%d.%d' % (location.file, location.linenr, num1, num2, num3))
        else:
            errorId = f"A-{num1}.{num2}.{num3}"
            autosar_severity = 'Required'
            this_violation = '{}-{}-{}-{}'.format(location.file, location.linenr, location.column, ruleNum)
            # If this is new violation then record it and show it. If not then
            # skip it since it has already been displayed.
            if not this_violation in self.existing_violations:
                self.existing_violations.add(this_violation)
                self.current_json_list.append(CXXResult(location.file, location.linenr, errorId, other_locations))
                cppcheckdata.reportError(location, autosar_severity, "", "autosar", errorId)
                if autosar_severity not in self.violations:
                    self.violations[autosar_severity] = []
                self.violations[autosar_severity].append('autosar' + "-" + errorId)

    def loadRuleTexts(self, filename):
        num1 = 0
        num2 = 0
        appendixA = False
        ruleText = False
        expect_more = False

        Rule_pattern = re.compile(r'^Rule ([0-9]+).([0-9]+)')
        severity_pattern = re.compile(r'.*[ ]*(Advisory|Required|Mandatory)$')
        xA_Z_pattern = re.compile(r'^[#A-Z].*')
        a_z_pattern = re.compile(r'^[a-z].*')
        # Try to detect the file encoding
        file_stream = None
        encodings = ['ascii', 'utf-8', 'windows-1250', 'windows-1252']
        for e in encodings:
            try:
                file_stream = codecs.open(filename, 'r', encoding=e)
                file_stream.readlines()
                file_stream.seek(0)
            except UnicodeDecodeError:
                file_stream = None
            else:
                break
        if not file_stream:
            print('Could not find a suitable codec for "' + filename + '".')
            print('If you know the codec please report it to the developers so the list can be enhanced.')
            print('Trying with default codec now and ignoring errors if possible ...')
            try:
                file_stream = open(filename, 'rt', errors='ignore')
            except TypeError:
                # Python 2 does not support the errors parameter
                file_stream = open(filename, 'rt')

        rule = None
        have_severity = False
        severity_loc = 0

        for line in file_stream:

            line = line.replace('\r', '').replace('\n', '')

            if not appendixA:
                if line.find('Appendix A') >= 0 and line.find('Summary of guidelines') >= 10:
                    appendixA = True
                continue
            if line.find('Appendix B') >= 0:
                break
            if len(line) == 0:
                continue

            # Parse rule declaration.
            res = Rule_pattern.match(line)

            if res:
                have_severity = False
                expect_more = False
                severity_loc = 0
                num1 = int(res.group(1))
                num2 = int(res.group(2))
                rule = Rule(num1, num2)

            if not have_severity and rule is not None:
                res = severity_pattern.match(line)

                if res:
                    rule.misra_severity = res.group(1)
                    have_severity = True
                else:
                    severity_loc += 1

                # Only look for severity on the Rule line
                # or the next non-blank line after
                # If it's not in either of those locations then
                # assume a severity was not provided.

                if severity_loc < 2:
                    continue
                else:
                    rule.misra_severity = ''
                    have_severity = True

            if rule is None:
                continue

            # Parse continuing of rule text.
            if expect_more:
                if a_z_pattern.match(line):
                    self.ruleTexts[rule.num].text += ' ' + line
                    continue

                expect_more = False
                continue

            # Parse beginning of rule text.
            if xA_Z_pattern.match(line):
                rule.text = line
                self.ruleTexts[rule.num] = rule
                expect_more = True

    def verifyRuleTexts(self):
        """Prints rule numbers without rule text."""
        rule_texts_rules = []
        for rule_num in self.ruleTexts:
            rule = self.ruleTexts[rule_num]
            rule_texts_rules.append(str(rule.num1) + '.' + str(rule.num2))

        all_rules = list(getAddonRules() + getCppcheckRules())

        missing_rules = list(set(all_rules) - set(rule_texts_rules))
        if len(missing_rules) == 0:
            print("Rule texts are correct.")
        else:
            print("Missing rule texts: " + ', '.join(missing_rules))

    def printStatus(self, *args, **kwargs):
        if not self.settings.quiet:
            print(*args, **kwargs)

    def executeCheck(self, rule_num, check_function, *args):
        """Execute check function for a single MISRA rule.

        :param rule_num: Number of rule in hundreds format
        :param check_function: Check function to execute
        :param args: Check function arguments
        """
        if not self.isRuleGloballySuppressed(rule_num):
            check_function(*args)

    def executeGJBCheck(self, check_function, *args):
        check_function(*args)

    def executeMisraCXXCheck(self, check_function, *args):
        check_function(*args)

    def executeCWECheck(self, check_function, *args):
        check_function(*args)

    def executeGoogleCPPCheck(self, check_function, *args):
        check_function(*args)

    def executeNaiveSystemsCheck(self, check_function, *args):
        check_function(*args)

    def executeAUTOSARCheck(self, check_function, *args):
        check_function(*args)

    def parseDump(self, dumpfile, check_rules, output_dir, create_dump_error=None):
        global _checkRule_2_4, _checkRule_2_5, _checkRule_5_7
        def fillVerifyExpected(verify_expected, tok):
            """Add expected suppressions to verify_expected list."""
            rule_re = re.compile(r'[0-9]+\.[0-9]+')
            if tok.str.startswith('//') and 'TODO' not in tok.str:
                for word in tok.str[2:].split(' '):
                    if rule_re.match(word):
                        verify_expected.append('%s:%d %s' % (tok.file, tok.linenr, word))

        data = cppcheckdata.parsedump(dumpfile)

        typeBits['CHAR'] = data.platform.char_bit
        typeBits['SHORT'] = data.platform.short_bit
        typeBits['INT'] = data.platform.int_bit
        typeBits['LONG'] = data.platform.long_bit
        typeBits['LONG_LONG'] = data.platform.long_long_bit
        typeBits['POINTER'] = data.platform.pointer_bit

        if self.settings.verify:
            # Add suppressions from the current file
            for tok in data.rawTokens:
                fillVerifyExpected(self.verify_expected, tok)
            # Add suppressions from the included headers
            include_re = re.compile(r'^#include [<"]([a-zA-Z0-9]+[a-zA-Z\-_./\\0-9]*)[">]$')
            dump_dir = os.path.dirname(data.filename)
            for conf in data.configurations:
                for directive in conf.directives:
                    m = re.match(include_re, directive.str)
                    if not m:
                        continue
                    header_dump_path = os.path.join(dump_dir, m.group(1) + '.dump')
                    if not os.path.exists(header_dump_path):
                        continue
                    header_data = cppcheckdata.parsedump(header_dump_path)
                    for tok in header_data.rawTokens:
                        fillVerifyExpected(self.verify_expected, tok)
        else:
            self.printStatus('Checking ' + dumpfile + '...')

        rules_list = set(check_rules.split(","))
        # data.iterconfigurations() will be empty if there is only #endif in the code
        if "misra_c_2012/rule_20_5" in rules_list or check_rules == "all":
            self.executeCheck(2005, self.misra_20_5, data)

        if create_dump_error is not None and (
            "misra_c_2012/rule_20_6" in rules_list or check_rules == "all"
        ):
            self.executeCheck(2006, self.misra_20_6, create_dump_error)

        if "misra_cpp_2008/rule_16_1_1" in rules_list or check_rules == "all":
            self.executeMisraCXXCheck(self.misra_cpp_16_1_1, data.rawTokens)

        for cfgNumber, cfg in enumerate(data.iterconfigurations()):
            if not self.settings.quiet:
                self.printStatus('Checking %s, config %s...' % (dumpfile, cfg.name))
            # judge whether enum is used to define Boolean
            for scope in cfg.scopes:
                if scope.type == "Enum" and scope.className and scope.className.find('Anonymous') == -1 and isBooleanEnum(scope):
                    scope.setIsBooleanEnumTrue()
            if "misra_c_2012/rule_1_4" in rules_list or check_rules == "all":
                self.executeCheck(104, self.misra_1_4, cfg, data.rawTokens)
            if "misra_c_2012/rule_2_3" in rules_list or check_rules == "all":
                self.executeCheck(203, self.misra_2_3, dumpfile, cfg.typedefInfo)
            if "misra_c_2012/rule_2_4" in rules_list or check_rules == "all":
                _checkRule_2_4 = True
                self.executeCheck(204, self.misra_2_4, dumpfile, cfg)
            if "misra_c_2012/rule_2_5" in rules_list or check_rules == "all":
                _checkRule_2_5 = True
                self.executeCheck(205, self.misra_2_5, dumpfile, cfg)
            if "misra_c_2012/rule_2_6" in rules_list or check_rules == "all":
                self.executeCheck(206, self.misra_2_6, cfg)
            if "misra_c_2012/rule_2_7" in rules_list or check_rules == "all":
                self.executeCheck(207, self.misra_2_7, cfg)
            # data.rawTokens is same for all configurations
            if cfgNumber == 0:
                if "misra_c_2012/rule_3_1" in rules_list or check_rules == "all":
                    self.executeCheck(301, self.misra_3_1, data.rawTokens)
                if "misra_c_2012/rule_3_2" in rules_list or check_rules == "all":
                    self.executeCheck(302, self.misra_3_2, data.rawTokens)
                if "misra_c_2012/rule_4_1" in rules_list or check_rules == "all":
                    self.executeCheck(401, self.misra_4_1, data.rawTokens)
                if "misra_c_2012/rule_4_2" in rules_list or check_rules == "all":
                    self.executeCheck(402, self.misra_4_2, data.rawTokens)
            if "misra_c_2012/rule_5_1" in rules_list or check_rules == "all":
                self.executeCheck(501, self.misra_5_1, cfg)
            if "misra_c_2012/rule_5_2" in rules_list or check_rules == "all":
                self.executeCheck(502, self.misra_5_2, cfg)
            if "misra_c_2012/rule_5_3" in rules_list or check_rules == "all":
                self.executeCheck(503, self.misra_5_3, cfg)
            if "misra_c_2012/rule_5_4" in rules_list or check_rules == "all":
                self.executeCheck(504, self.misra_5_4, cfg)
            if "misra_c_2012/rule_5_5" in rules_list or check_rules == "all":
                self.executeCheck(505, self.misra_5_5, cfg)
            if "misra_c_2012/rule_5_6" in rules_list or check_rules == "all":
                self.executeCheck(506, self.misra_5_6, dumpfile, cfg.typedefInfo)
            if "misra_c_2012/rule_5_7" in rules_list or check_rules == "all":
                _check_rule_5_7 = True
                self.executeCheck(507, self.misra_5_7, dumpfile, cfg)
            if "misra_c_2012/rule_5_8" in rules_list or check_rules == "all":
                self.executeCheck(508, self.misra_5_8, dumpfile, cfg)
            if "misra_c_2012/rule_5_9" in rules_list or check_rules == "all":
                self.executeCheck(509, self.misra_5_9, dumpfile, cfg)
            if "misra_c_2012/rule_6_1" in rules_list or check_rules == "all":
                self.executeCheck(601, self.misra_6_1, cfg)
            if "misra_c_2012/rule_6_2" in rules_list or check_rules == "all":
                self.executeCheck(602, self.misra_6_2, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_7_1" in rules_list or check_rules == "all"
            ):
                self.executeCheck(701, self.misra_7_1, data.rawTokens)
            if "misra_c_2012/rule_7_2" in rules_list or check_rules == "all":
                self.executeCheck(702, self.misra_7_2, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_7_3" in rules_list or check_rules == "all"
            ):
                self.executeCheck(703, self.misra_7_3, data.rawTokens)
            if "misra_c_2012/rule_7_4" in rules_list or check_rules == "all":
                self.executeCheck(704, self.misra_7_4, cfg)
            if "misra_c_2012/rule_8_1" in rules_list or check_rules == "all":
                self.executeCheck(801, self.misra_8_1, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_8_2" in rules_list or check_rules == "all"
            ):
                self.executeCheck(802, self.misra_8_2, cfg, data.rawTokens)
            if "misra_c_2012/rule_8_4" in rules_list or check_rules == "all":
                self.executeCheck(804, self.misra_8_4, cfg)
            if "misra_c_2012/rule_8_5" in rules_list or check_rules == "all":
                self.executeCheck(805, self.misra_8_5, dumpfile, cfg)
            if "misra_c_2012/rule_8_6" in rules_list or check_rules == "all":
                self.executeCheck(806, self.misra_8_6, dumpfile, cfg)
            if "misra_c_2012/rule_8_7" in rules_list or check_rules == "all":
                self.executeCheck(807, self.misra_8_7, dumpfile, cfg)
            if "misra_c_2012/rule_8_8" in rules_list or check_rules == "all":
                self.executeCheck(808, self.misra_8_8, cfg)
            if "misra_c_2012/rule_8_9" in rules_list or check_rules == "all":
                self.executeCheck(809, self.misra_8_9, cfg)
            if "misra_c_2012/rule_8_10" in rules_list or check_rules == "all":
                self.executeCheck(810, self.misra_8_10, cfg)
            if "misra_c_2012/rule_8_11" in rules_list or check_rules == "all":
                self.executeCheck(811, self.misra_8_11, cfg)
            if "misra_c_2012/rule_8_12" in rules_list or check_rules == "all":
                self.executeCheck(812, self.misra_8_12, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_8_14" in rules_list or check_rules == "all"
            ):
                self.executeCheck(814, self.misra_8_14, data.rawTokens)
            if "misra_c_2012/rule_9_2" in rules_list or check_rules == "all":
                self.executeCheck(902, self.misra_9_2, cfg)
            if "misra_c_2012/rule_9_3" in rules_list or check_rules == "all":
                self.executeCheck(903, self.misra_9_3, cfg)
            if "misra_c_2012/rule_9_4" in rules_list or check_rules == "all":
                self.executeCheck(904, self.misra_9_4, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_9_5" in rules_list or check_rules == "all"
            ):
                self.executeCheck(905, self.misra_9_5, cfg, data.rawTokens)
            if "misra_c_2012/rule_10_1" in rules_list or check_rules == "all":
                self.executeCheck(1001, self.misra_10_1, cfg)
            if "misra_c_2012/rule_10_2" in rules_list or check_rules == "all":
                self.executeCheck(1002, self.misra_10_2, cfg)
            if "misra_c_2012/rule_10_3" in rules_list or check_rules == "all":
                self.executeCheck(1003, self.misra_10_3, cfg)
            if "misra_c_2012/rule_10_4" in rules_list or check_rules == "all":
                self.executeCheck(1004, self.misra_10_4, cfg)
            if "misra_c_2012/rule_10_5" in rules_list or check_rules == "all":
                self.executeCheck(1005, self.misra_10_5, cfg)
            if "misra_c_2012/rule_10_6" in rules_list or check_rules == "all":
                self.executeCheck(1006, self.misra_10_6, cfg)
            if "misra_c_2012/rule_10_7" in rules_list or check_rules == "all":
                self.executeCheck(1007, self.misra_10_7, cfg)
            if "misra_c_2012/rule_10_8" in rules_list or check_rules == "all":
                self.executeCheck(1008, self.misra_10_8, cfg)
            if "misra_c_2012/rule_11_1" in rules_list or check_rules == "all":
                self.executeCheck(1101, self.misra_11_1, cfg)
            if "misra_c_2012/rule_11_2" in rules_list or check_rules == "all":
                self.executeCheck(1102, self.misra_11_2, cfg)
            if "misra_c_2012/rule_11_3" in rules_list or check_rules == "all":
                self.executeCheck(1103, self.misra_11_3, cfg)
            if "misra_c_2012/rule_11_4" in rules_list or check_rules == "all":
                self.executeCheck(1104, self.misra_11_4, cfg)
            if "misra_c_2012/rule_11_5" in rules_list or check_rules == "all":
                self.executeCheck(1105, self.misra_11_5, cfg)
            if "misra_c_2012/rule_11_6" in rules_list or check_rules == "all":
                self.executeCheck(1106, self.misra_11_6, cfg)
            if "misra_c_2012/rule_11_7" in rules_list or check_rules == "all":
                self.executeCheck(1107, self.misra_11_7, cfg)
            if "misra_c_2012/rule_11_8" in rules_list or check_rules == "all":
                self.executeCheck(1108, self.misra_11_8, cfg)
            if "misra_c_2012/rule_11_9" in rules_list or check_rules == "all":
                self.executeCheck(1109, self.misra_11_9, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_12_1" in rules_list or check_rules == "all"
            ):
                self.executeCheck(1201, self.misra_12_1_sizeof, data.rawTokens)
            if "misra_c_2012/rule_12_1" in rules_list or check_rules == "all":
                self.executeCheck(1201, self.misra_12_1, cfg)
            if "misra_c_2012/rule_12_2" in rules_list or check_rules == "all":
                self.executeCheck(1202, self.misra_12_2, cfg)
            if "misra_c_2012/rule_12_3" in rules_list or check_rules == "all":
                self.executeCheck(1203, self.misra_12_3, cfg)
            if "misra_c_2012/rule_12_4" in rules_list or check_rules == "all":
                self.executeCheck(1204, self.misra_12_4, cfg)
            if "misra_c_2012/rule_12_5" in rules_list or check_rules == "all":
                self.executeCheck(1205, self.misra_12_5, cfg)
            if "misra_c_2012/rule_13_1" in rules_list or check_rules == "all":
                self.executeCheck(1301, self.misra_13_1, cfg)
            if "misra_c_2012/rule_13_3" in rules_list or check_rules == "all":
                self.executeCheck(1303, self.misra_13_3, cfg)
            if "misra_c_2012/rule_13_4" in rules_list or check_rules == "all":
                self.executeCheck(1304, self.misra_13_4, cfg)
            if "misra_c_2012/rule_13_5" in rules_list or check_rules == "all":
                self.executeCheck(1305, self.misra_13_5, cfg)
            if "misra_c_2012/rule_13_6" in rules_list or check_rules == "all":
                self.executeCheck(1306, self.misra_13_6, cfg)
            if "misra_c_2012/rule_14_1" in rules_list or check_rules == "all":
                self.executeCheck(1401, self.misra_14_1, cfg)
            if "misra_c_2012/rule_14_2" in rules_list or check_rules == "all":
                self.executeCheck(1402, self.misra_14_2, cfg)
            if "misra_c_2012/rule_14_4" in rules_list or check_rules == "all":
                self.executeCheck(1404, self.misra_14_4, cfg)
            if "misra_c_2012/rule_15_1" in rules_list or check_rules == "all":
                self.executeCheck(1501, self.misra_15_1, cfg)
            if "misra_c_2012/rule_15_2" in rules_list or check_rules == "all":
                self.executeCheck(1502, self.misra_15_2, cfg)
            if "misra_c_2012/rule_15_3" in rules_list or check_rules == "all":
                self.executeCheck(1503, self.misra_15_3, cfg)
            if "misra_c_2012/rule_15_4" in rules_list or check_rules == "all":
                self.executeCheck(1504, self.misra_15_4, cfg)
            if "misra_c_2012/rule_15_5" in rules_list or check_rules == "all":
                self.executeCheck(1505, self.misra_15_5, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_15_6" in rules_list or check_rules == "all"
            ):
                self.executeCheck(1506, self.misra_15_6, data.rawTokens)
            if "misra_c_2012/rule_15_7" in rules_list or check_rules == "all":
                self.executeCheck(1507, self.misra_15_7, cfg, data.rawTokens)
            if "misra_c_2012/rule_16_1" in rules_list or check_rules == "all":
                self.executeCheck(1601, self.misra_16_1, cfg)
            if "misra_c_2012/rule_16_2" in rules_list or check_rules == "all":
                self.executeCheck(1602, self.misra_16_2, cfg)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_16_3" in rules_list or check_rules == "all"
            ):
                self.executeCheck(1603, self.misra_16_3, data.rawTokens)
            if "misra_c_2012/rule_16_4" in rules_list or check_rules == "all":
                self.executeCheck(1604, self.misra_16_4, cfg)
            if "misra_c_2012/rule_16_5" in rules_list or check_rules == "all":
                self.executeCheck(1605, self.misra_16_5, cfg)
            if "misra_c_2012/rule_16_6" in rules_list or check_rules == "all":
                self.executeCheck(1606, self.misra_16_6, cfg)
            if "misra_c_2012/rule_16_7" in rules_list or check_rules == "all":
                self.executeCheck(1607, self.misra_16_7, cfg)
            if "misra_c_2012/rule_17_1" in rules_list or check_rules == "all":
                self.executeCheck(1701, self.misra_17_1, cfg)
            if "misra_c_2012/rule_17_2" in rules_list or check_rules == "all":
                self.executeCheck(1702, self.misra_17_2, cfg)
            if "misra_c_2012/rule_17_4" in rules_list or check_rules == "all":
                self.executeCheck(1704, self.misra_17_4, cfg, create_dump_error)
            if cfgNumber == 0 and (
                "misra_c_2012/rule_17_6" in rules_list or check_rules == "all"
            ):
                self.executeCheck(1706, self.misra_17_6, data.rawTokens)
            if "misra_c_2012/rule_17_7" in rules_list or check_rules == "all":
                self.executeCheck(1707, self.misra_17_7, cfg)
            if "misra_c_2012/rule_17_8" in rules_list or check_rules == "all":
                self.executeCheck(1708, self.misra_17_8, cfg)
            if "misra_c_2012/rule_18_4" in rules_list or check_rules == "all":
                self.executeCheck(1804, self.misra_18_4, cfg)
            if "misra_c_2012/rule_18_5" in rules_list or check_rules == "all":
                self.executeCheck(1805, self.misra_18_5, cfg)
            if "misra_c_2012/rule_18_7" in rules_list or check_rules == "all":
                self.executeCheck(1807, self.misra_18_7, cfg)
            if "misra_c_2012/rule_18_8" in rules_list or check_rules == "all":
                self.executeCheck(1808, self.misra_18_8, cfg)
            if "misra_c_2012/rule_19_2" in rules_list or check_rules == "all":
                self.executeCheck(1902, self.misra_19_2, data.rawTokens)
            if "misra_c_2012/rule_20_1" in rules_list or check_rules == "all":
                self.executeCheck(2001, self.misra_20_1, cfg)
            if "misra_c_2012/rule_20_2" in rules_list or check_rules == "all":
                self.executeCheck(2002, self.misra_20_2, cfg)
            if "misra_c_2012/rule_20_3" in rules_list or check_rules == "all":
                self.executeCheck(2003, self.misra_20_3, cfg)
            if "misra_c_2012/rule_20_4" in rules_list or check_rules == "all":
                self.executeCheck(2004, self.misra_20_4, cfg)
            if "misra_c_2012/rule_20_5" in rules_list or check_rules == "all":
                self.executeCheck(2005, self.misra_20_5, cfg)
            if "misra_c_2012/rule_20_7" in rules_list or check_rules == "all":
                self.executeCheck(2007, self.misra_20_7, cfg)
            if "misra_c_2012/rule_20_8" in rules_list or check_rules == "all":
                self.executeCheck(2008, self.misra_20_8, cfg)
            if "misra_c_2012/rule_20_9" in rules_list or check_rules == "all":
                self.executeCheck(2009, self.misra_20_9, cfg)
            if "misra_c_2012/rule_20_10" in rules_list or check_rules == "all":
                self.executeCheck(2010, self.misra_20_10, cfg)
            if "misra_c_2012/rule_20_11" in rules_list or check_rules == "all":
                self.executeCheck(2011, self.misra_20_11, cfg)
            if "misra_c_2012/rule_20_12" in rules_list or check_rules == "all":
                self.executeCheck(2012, self.misra_20_12, cfg)
            if "misra_c_2012/rule_20_13" in rules_list or check_rules == "all":
                self.executeCheck(2013, self.misra_20_13, cfg)
            if "misra_c_2012/rule_20_14" in rules_list or check_rules == "all":
                self.executeCheck(2014, self.misra_20_14, cfg)
            if "misra_c_2012/rule_21_1" in rules_list or check_rules == "all":
                self.executeCheck(2101, self.misra_21_1, cfg)
            if "misra_c_2012/rule_21_2" in rules_list or check_rules == "all":
                self.executeCheck(2102, self.misra_21_2, cfg)
            if "misra_c_2012/rule_21_3" in rules_list or check_rules == "all":
                self.executeCheck(2103, self.misra_21_3, cfg)
            if "misra_c_2012/rule_21_4" in rules_list or check_rules == "all":
                self.executeCheck(2104, self.misra_21_4, cfg)
            if "misra_c_2012/rule_21_5" in rules_list or check_rules == "all":
                self.executeCheck(2105, self.misra_21_5, cfg)
            if "misra_c_2012/rule_21_6" in rules_list or check_rules == "all":
                self.executeCheck(2106, self.misra_21_6, cfg)
            if "misra_c_2012/rule_21_7" in rules_list or check_rules == "all":
                self.executeCheck(2107, self.misra_21_7, cfg)
            if "misra_c_2012/rule_21_8" in rules_list or check_rules == "all":
                self.executeCheck(2108, self.misra_21_8, cfg)
            if "misra_c_2012/rule_21_9" in rules_list or check_rules == "all":
                self.executeCheck(2109, self.misra_21_9, cfg)
            if "misra_c_2012/rule_21_10" in rules_list or check_rules == "all":
                self.executeCheck(2110, self.misra_21_10, cfg)
            if "misra_c_2012/rule_21_11" in rules_list or check_rules == "all":
                self.executeCheck(2111, self.misra_21_11, cfg)
            if "misra_c_2012/rule_21_12" in rules_list or check_rules == "all":
                self.executeCheck(2112, self.misra_21_12, cfg)
            if "misra_c_2012/rule_21_14" in rules_list or check_rules == "all":
                self.executeCheck(2114, self.misra_21_14, cfg)
            if "misra_c_2012/rule_21_15" in rules_list or check_rules == "all":
                self.executeCheck(2115, self.misra_21_15, cfg)
            if "misra_c_2012/rule_21_16" in rules_list or check_rules == "all":
                self.executeCheck(2116, self.misra_21_16, cfg)
            if "misra_c_2012/rule_21_19" in rules_list or check_rules == "all":
                self.executeCheck(2119, self.misra_21_19, cfg)
            if "misra_c_2012/rule_21_20" in rules_list or check_rules == "all":
                self.executeCheck(2120, self.misra_21_20, cfg)
            if "misra_c_2012/rule_21_21" in rules_list or check_rules == "all":
                self.executeCheck(2121, self.misra_21_21, cfg)
            # 22.4 is already covered by Cppcheck writeReadOnlyFile
            if "misra_c_2012/rule_22_5" in rules_list or check_rules == "all":
                self.executeCheck(2205, self.misra_22_5, cfg)
            if "misra_c_2012/rule_22_7" in rules_list or check_rules == "all":
                self.executeCheck(2207, self.misra_22_7, cfg)
            if "misra_c_2012/rule_22_8" in rules_list or check_rules == "all":
                self.executeCheck(2208, self.misra_22_8, cfg)
            if "misra_c_2012/rule_22_9" in rules_list or check_rules == "all":
                self.executeCheck(2209, self.misra_22_9, cfg)
            if "misra_c_2012/rule_22_10" in rules_list or check_rules == "all":
                self.executeCheck(2210, self.misra_22_10, cfg)
            # gjb5369 rules
            if "gjb5369/rule_1_1_5" in rules_list:
                self.executeGJBCheck(self.gjb5369_1_1_5, cfg)
            if "gjb5369/rule_1_1_9" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_1_1_9, cfg)
            if "gjb5369/rule_2_1_3" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_2_1_3, data.rawTokens)
            if "gjb5369/rule_6_1_3" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_3, cfg)
            if "gjb5369/rule_6_1_5" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_5, cfg)
            if "gjb5369/rule_6_1_7" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_7, cfg)
            if "gjb5369/rule_6_1_10" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_10, cfg)
            if "gjb5369/rule_6_1_11" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_11, cfg)
            if "gjb5369/rule_6_1_17" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_17, cfg)
            if "gjb5369/rule_6_1_18" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_6_1_18, cfg)
            if "gjb5369/rule_7_1_1" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_7_1_1, cfg)
            if "gjb5369/rule_7_1_2" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_7_1_2, cfg)
            if "gjb5369/rule_7_1_3" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_7_1_3, cfg)
            if "gjb5369/rule_7_1_8" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_7_1_8, cfg)
            if cfgNumber == 0 and (
                "gjb5369/rule_8_1_3" in rules_list or check_rules == "all"
            ):
                self.executeGJBCheck(self.gjb5369_8_1_3, data.rawTokens)
            if "gjb5369/rule_10_1_1" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_10_1_1, data.rawTokens)
            if "gjb5369/rule_13_1_3" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_13_1_3, cfg)
            if "gjb5369/rule_14_1_1" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_14_1_1, cfg)
            if "gjb5369/rule_15_1_1" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_15_1_1, cfg)
            if "gjb5369/rule_15_1_2" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_15_1_2, cfg)
            if "gjb5369/rule_15_1_5" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb5369_15_1_5, cfg)
            # gjb8114 rules
            if "gjb8114/rule_1_1_6" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_1_6, cfg)
            if "gjb8114/rule_1_1_7" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_1_7, cfg)
            if "gjb8114/rule_1_1_17" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_1_17, cfg)
            if "gjb8114/rule_1_1_19" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_1_19, cfg)
            if "gjb8114/rule_1_1_22" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_1_22, cfg)
            if "gjb8114/rule_1_1_23" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_1_23, cfg)
            if "gjb8114/rule_1_4_2" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_4_2, cfg, data.rawTokens)
            if "gjb8114/rule_1_4_5" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_4_5, cfg)
            if "gjb8114/rule_1_4_6" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_4_6, cfg)
            if "gjb8114/rule_1_4_7" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_4_7, data.rawTokens)
            if "gjb8114/rule_1_6_7" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_6_7, cfg, create_dump_error)
            if "gjb8114/rule_1_6_9" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_6_9, cfg)
            if "gjb8114/rule_1_6_18" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_6_18, cfg)
            if "gjb8114/rule_1_7_11" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_7_11, cfg)
            if "gjb8114/rule_1_7_12" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_7_12, cfg)
            if cfgNumber == 0 and (
                "gjb8114/rule_1_8_4" in rules_list or check_rules == "all"
            ):
                self.executeGJBCheck(self.gjb8114_1_8_4, data.rawTokens)
            if "gjb8114/rule_1_8_5" in rules_list or check_rules == "all":
                self.executeGJBCheck(self.gjb8114_1_8_5, cfg)

            if "misra_cpp_2008/rule_2_7_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_2_7_1, cfg, data.rawTokens)
            if "misra_cpp_2008/rule_3_3_2" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_3_3_2, cfg, data.rawTokens)
            if "misra_cpp_2008/rule_3_9_3" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_3_9_3, cfg)
            if "misra_cpp_2008/rule_5_0_20" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_5_0_20, cfg)
            if "misra_cpp_2008/rule_5_0_21" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_5_0_21, cfg)
            if "misra_cpp_2008/rule_6_2_3" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_6_2_3, data.rawTokens)
            if "misra_cpp_2008/rule_6_3_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_6_3_1, cfg, data.rawTokens)
            if "misra_cpp_2008/rule_6_4_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_6_4_1, cfg, data.rawTokens)
            if "misra_cpp_2008/rule_6_4_5" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_6_4_5, cfg, data.rawTokens)
            if "misra_cpp_2008/rule_6_4_6" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_6_4_6, cfg)
            if "misra_cpp_2008/rule_6_4_8" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_6_4_8, cfg)
            if "misra_cpp_2008/rule_7_4_2" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_7_4_2, cfg)
            if "misra_cpp_2008/rule_8_5_3" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_8_5_3, cfg)
            if "misra_cpp_2008/rule_16_0_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_0_1, cfg, data.rawTokens)
            if "misra_cpp_2008/rule_16_0_2" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_0_2, data.rawTokens)
            if "misra_cpp_2008/rule_16_0_8" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_0_8, cfg)
            if "misra_cpp_2008/rule_16_2_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_2_1, cfg)
            if "misra_cpp_2008/rule_16_2_2" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_2_2, cfg)
            if "misra_cpp_2008/rule_16_2_4" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_2_4, cfg)
            if "misra_cpp_2008/rule_16_3_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_16_3_1, cfg)
            if "misra_cpp_2008/rule_17_0_3" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_17_0_3, cfg)
            if "misra_cpp_2008/rule_18_0_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_18_0_1, cfg)
            if "misra_cpp_2008/rule_27_0_1" in rules_list or check_rules == "all":
                self.executeMisraCXXCheck(self.misra_cpp_27_0_1, cfg)

            if "cwe/cwe_483" in rules_list or check_rules == "all":
                self.executeCWECheck(self.cwe_483, cfg)

            if "googlecpp/g1206" in rules_list or check_rules == "all":
                self.executeGoogleCPPCheck(self.g1206, cfg, data.rawTokens)
            if "googlecpp/g1213" in rules_list or check_rules == "all":
                self.executeGoogleCPPCheck(self.g1213, cfg)
            if "googlecpp/g1214" in rules_list or check_rules == "all":
                self.executeGoogleCPPCheck(self.g1214, cfg)
            if "googlecpp/g1215" in rules_list or check_rules == "all":
                self.executeGoogleCPPCheck(self.g1215, cfg)

            if "naivesystems/C7966" in rules_list or check_rules == "all":
                self.executeNaiveSystemsCheck(self.naive_systems_C7966, cfg)

            if "autosar/rule_A16_0_1" in rules_list or check_rules == "all":
                self.executeAUTOSARCheck(self.rule_A16_0_1, cfg, data.rawTokens)
            if "autosar/rule_A16_2_1" in rules_list or check_rules == "all":
                self.executeAUTOSARCheck(self.rule_A16_2_1, cfg)
            if "autosar/rule_A16_6_1" in rules_list or check_rules == "all":
                self.executeAUTOSARCheck(self.rule_A16_6_1, cfg)
            if "autosar/rule_A16_7_1" in rules_list or check_rules == "all":
                self.executeAUTOSARCheck(self.rule_A16_7_1, cfg)
            if "autosar/rule_A18_0_3" in rules_list or check_rules == "all":
                self.executeAUTOSARCheck(self.rule_A18_0_3, cfg)

    def analyse_ctu_info(self, ctu_info_files):
        all_typedef_info = []
        all_tagname_info = []
        all_macro_info = []
        all_external_identifiers_decl = {}
        all_external_identifiers_def = {}
        all_internal_identifiers = {}
        all_local_identifiers = {}
        all_usage_count = {}

        from cppcheckdata import Location

        def is_different_location(loc1, loc2):
            return loc1['file'] != loc2['file'] or loc1['line'] != loc2['line']

        for filename in ctu_info_files:
            for line in open(filename, 'rt'):
                if not line.startswith('{'):
                    continue

                s = json.loads(line)
                summary_type = s['summary']
                summary_data = s['data']

                if summary_type == 'MisraTypedefInfo':
                    for new_typedef_info in summary_data:
                        found = False
                        for old_typedef_info in all_typedef_info:
                            if old_typedef_info['name'] == new_typedef_info['name']:
                                found = True
                                if is_different_location(old_typedef_info, new_typedef_info):
                                    self.reportError(Location(old_typedef_info), 5, 6)
                                    self.reportError(Location(new_typedef_info), 5, 6)
                                else:
                                    if new_typedef_info['used']:
                                        old_typedef_info['used'] = True
                                break
                        if not found:
                            all_typedef_info.append(new_typedef_info)

                if summary_type == 'MisraTagName':
                    for new_tagname_info in summary_data:
                        found = False
                        for old_tagname_info in all_tagname_info:
                            if old_tagname_info['name'] == new_tagname_info['name']:
                                found = True
                                if is_different_location(old_tagname_info, new_tagname_info) and _checkRule_5_7:
                                    self.reportError(Location(old_tagname_info), 5, 7)
                                    self.reportError(Location(new_tagname_info), 5, 7)
                                else:
                                    if new_tagname_info['used']:
                                        old_tagname_info['used'] = True
                                break
                        if not found:
                            all_tagname_info.append(new_tagname_info)

                if summary_type == 'MisraMacro':
                    for new_macro in summary_data:
                        found = False
                        for old_macro in all_macro_info:
                            if old_macro['name'] == new_macro['name']:
                                found = True
                                if new_macro['used']:
                                    old_macro['used'] = True
                                break
                        if not found:
                            all_macro_info.append(new_macro)

                if summary_type == 'MisraExternalIdentifiers':
                    for s in summary_data:
                        is_declaration = s['decl']
                        if is_declaration:
                            all_external_identifiers = all_external_identifiers_decl
                        else:
                            all_external_identifiers = all_external_identifiers_def

                        name = s['name']
                        if name in all_external_identifiers and is_different_location(s, all_external_identifiers[name]):
                            num = 5 if is_declaration else 6
                            self.reportError(Location(s), 8, num)
                            self.reportError(Location(all_external_identifiers[name]), 8, num)
                        all_external_identifiers[name] = s

                if summary_type == 'MisraInternalIdentifiers':
                    for s in summary_data:
                        if s['name'] in all_internal_identifiers:
                            self.reportError(Location(s), 5, 9)
                            self.reportError(Location(all_internal_identifiers[s['name']]), 5, 9)
                        all_internal_identifiers[s['name']] = s

                if summary_type == 'MisraLocalIdentifiers':
                    for s in summary_data:
                        all_local_identifiers[s['name']] = s

                if summary_type == 'MisraUsage':
                    for s in summary_data:
                        if s in all_usage_count:
                            all_usage_count[s] += 1
                        else:
                            all_usage_count[s] = 1

        for ti in all_typedef_info:
            if not ti['used']:
                self.reportError(Location(ti), 2, 3)

        if _checkRule_2_4:
            for ti in all_tagname_info:
                if not ti['used']:
                    self.reportError(Location(ti), 2, 4)

        if _checkRule_2_5:
            for m in all_macro_info:
                if not m['used']:
                    if m['in_include_guards']:
                        self.reportError(Location(m), 2, 5, optional_flag = True)
                    else:
                        self.reportError(Location(m), 2, 5)

        all_external_identifiers = all_external_identifiers_decl
        all_external_identifiers.update(all_external_identifiers_def)
        for name, external_identifier in all_external_identifiers.items():
            internal_identifier = all_internal_identifiers.get(name)
            if internal_identifier:
                self.reportError(Location(internal_identifier), 5, 8)
                self.reportError(Location(external_identifier), 5, 8)

            local_identifier = all_local_identifiers.get(name)
            if local_identifier:
                self.reportError(Location(local_identifier), 5, 8)
                self.reportError(Location(external_identifier), 5, 8)

        for name, count in all_usage_count.items():
            #print('%s:%i' % (name, count))
            if count != 1:
                continue
            if name in all_external_identifiers:
                self.reportError(Location(all_external_identifiers[name]), 8, 7)

RULE_TEXTS_HELP = '''Path to text file of MISRA rules

If you have the tool 'pdftotext' you might be able
to generate this textfile with such command:

    pdftotext MISRA_C_2012.pdf MISRA_C_2012.txt

Otherwise you can more or less copy/paste the chapter
Appendix A Summary of guidelines
from the MISRA pdf. You can buy the MISRA pdf from
http://www.misra.org.uk/

Format:

<..arbitrary text..>
Appendix A Summary of guidelines
Rule 1.1 Required
Rule text for 1.1
continuation of rule text for 1.1
Rule 1.2 Mandatory
Rule text for 1.2
continuation of rule text for 1.2
<...>

'''

SUPPRESS_RULES_HELP = '''MISRA rules to suppress (comma-separated)

For example, if you'd like to suppress rules 15.1, 11.3,
and 20.13, run:

    python misra.py --suppress-rules 15.1,11.3,20.13 ...

'''


def get_args_parser():
    """Generates list of command-line arguments acceptable by misra.py script."""
    parser = cppcheckdata.ArgumentParser()
    parser.add_argument("--rule-texts", type=str, help=RULE_TEXTS_HELP)
    parser.add_argument("--verify-rule-texts",
                        help="Verify that all supported rules texts are present in given file and exit.",
                        action="store_true")
    parser.add_argument("--suppress-rules", type=str, help=SUPPRESS_RULES_HELP)
    parser.add_argument("--no-summary", help="Hide summary of violations", action="store_true")
    parser.add_argument("--show-suppressed-rules", help="Print rule suppression list", action="store_true")
    parser.add_argument("-P", "--file-prefix", type=str, help="Prefix to strip when matching suppression file rules")
    parser.add_argument("-generate-table", help=argparse.SUPPRESS, action="store_true")
    parser.add_argument("-verify", help=argparse.SUPPRESS, action="store_true")
    parser.add_argument("--severity", type=str, help="Set a custom severity string, for example 'error' or 'warning'. ")
    parser.add_argument("--create-dump-error", type=str, help="Error message from the cppcheck binary when creating a dump file")
    parser.add_argument("--check_rules", type=str, default="all", help="Rules to be analyzed, split by comma, default all")
    parser.add_argument("--output_dir", type=str, default="/output", help="Absolute path to the directory of results files")
    return parser


def main():
    parser = get_args_parser()
    args = parser.parse_args()
    settings = MisraSettings(args)
    checker = MisraChecker(settings)

    if args.generate_table:
        generateTable()
        sys.exit(0)

    if args.rule_texts:
        filename = os.path.expanduser(args.rule_texts)
        filename = os.path.normpath(filename)
        if not os.path.isfile(filename):
            print('Fatal error: file is not found: ' + filename)
            sys.exit(1)
        checker.loadRuleTexts(filename)
        if args.verify_rule_texts:
            checker.verifyRuleTexts()
            sys.exit(0)

    if args.verify_rule_texts and not args.rule_texts:
        print("Error: Please specify rule texts file with --rule-texts=<file>")
        sys.exit(1)

    if args.suppress_rules:
        checker.setSuppressionList(args.suppress_rules)

    if args.file_prefix:
        checker.setFilePrefix(args.file_prefix)

    dump_files, ctu_info_files = cppcheckdata.get_files(args)

    if (not dump_files) and (not ctu_info_files):
        if not args.quiet:
            print("No input files.")
        sys.exit(0)

    if args.severity:
        checker.setSeverity(args.severity)

    for item in dump_files:
        checker.parseDump(item, args.check_rules, args.output_dir, args.create_dump_error)

        if settings.verify:
            verify_expected = checker.get_verify_expected()
            verify_actual = checker.get_verify_actual()

            exitCode = 0
            for expected in verify_expected:
                if expected not in verify_actual:
                    print('Expected but not seen: ' + expected)
                    exitCode = 1
            for actual in verify_actual:
                if actual not in verify_expected:
                    print('Not expected: ' + actual)
                    exitCode = 1

            # Existing behavior of verify mode is to exit
            # on the first un-expected output.
            # TODO: Is this required? or can it be moved to after
            # all input files have been processed
            if exitCode != 0:
                sys.exit(exitCode)

    checker.analyse_ctu_info(ctu_info_files)

    if settings.verify:
        sys.exit(exitCode)

    number_of_violations = len(checker.get_violations())
    if number_of_violations > 0:
        if settings.show_summary:
            print("\nMISRA rules violations found:\n\t%s\n" % (
                "\n\t".join(["%s: %d" % (viol, len(checker.get_violations(viol))) for viol in
                             checker.get_violation_types()])))

            rules_violated = {}
            for severity, ids in checker.get_violations():
                for misra_id in ids:
                    rules_violated[misra_id] = rules_violated.get(misra_id, 0) + 1
            print("MISRA rules violated:")
            convert = lambda text: int(text) if text.isdigit() else text
            misra_sort = lambda key: [convert(c) for c in re.split(r'[\.-]([0-9]*)', key)]
            for misra_id in sorted(rules_violated.keys(), key=misra_sort):
                res = re.match(r'misra-c2012-([0-9]+)\\.([0-9]+)', misra_id)
                if res is None:
                    num = 0
                else:
                    num = int(res.group(1)) * 100 + int(res.group(2))
                severity = '-'
                if num in checker.ruleTexts:
                    severity = checker.ruleTexts[num].cppcheck_severity
                print("\t%15s (%s): %d" % (misra_id, severity, rules_violated[misra_id]))

    if args.show_suppressed_rules:
        checker.showSuppressedRules()

    try:
        result_file = os.path.join(args.output_dir, "cppcheck_out.json")
        file = open(result_file, "w")
        json.dump(checker.current_json_list, file, default=vars, indent=4)
        file.close()
    except:
        logging.warning("write json result file failed")

if __name__ == '__main__':
    main()
    sys.exit(cppcheckdata.EXIT_CODE)
