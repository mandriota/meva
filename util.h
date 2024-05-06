/******************************************************************************\
*                                                                              *
*    Mewa. Math EWAluator.                                                     *
*    Copyright (C) 2024 Mark Mandriota                                         *
*                                                                              *
*    This program is free software: you can redistribute it and/or modify      *
*    it under the terms of the GNU General Public License as published by      *
*    the Free Software Foundation, either version 3 of the License, or         *
*    (at your option) any later version.                                       *
*                                                                              *
*    This program is distributed in the hope that it will be useful,           *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*    GNU General Public License for more details.                              *
*                                                                              *
*    You should have received a copy of the GNU General Public License         *
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.    *
*                                                                              *
\******************************************************************************/

#ifndef UTIL_H
#define UTIL_H

#include "config.h"

#include <complex.h>
#include <limits.h>
#include <stdbool.h> // IWYU pragma: keep
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tgmath.h> // IWYU pragma: keep
// IWYU pragma: no_include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

//=:util
//           _   _ _
//          | | (_) |
//     _   _| |_ _| |
//    | | | | __| | |
//    | |_| | |_| | |
//     \__,_|\__|_|_|

//=:util:error_handling
#define FATAL(...)                                                             \
  {                                                                            \
    fprintf(stderr, CLR_ERR_MSG "FATAL" CLR_RESET ": " __VA_ARGS__);           \
    exit(EXIT_FAILURE);                                                        \
  }

#define PFATAL(s)                                                              \
  {                                                                            \
    perror(s);                                                                 \
    exit(EXIT_FAILURE);                                                        \
  }

#define ERROR(...)                                                             \
  {                                                                            \
    fprintf(stderr, CLR_ERR_MSG "ERROR" CLR_RESET ": " __VA_ARGS__);           \
    fflush(stderr);                                                            \
  }

#define TRY(type, expr)                                                        \
  {                                                                            \
    type err = expr;                                                           \
    if (err != type##_NOERROR)                                                 \
      return err;                                                              \
  }

//=:util:debug
#ifdef NDEBUG
#define DBG_PRINT(...)
#define DBG_FATAL(...)
#define DBG(x)
#else
#define DBG_PRINT(...)                                                         \
  {                                                                            \
    fprintf(stderr, CLR_INF_MSG "INFO" CLR_RESET ": " __VA_ARGS__);            \
    fflush(stderr);                                                            \
  }
#define DBG_FATAL(...) FATAL(__VA_ARGS__)
#define DBG(x) x
#endif

//=:util:other
#define STRINGIFY(name) #name

#define STRINGIFY_CASE(name)                                                   \
  case name:                                                                   \
    return STRINGIFY(name);

#define EXEC_CASE(value, expr)                                                 \
  case value:                                                                  \
    expr;                                                                      \
    break;

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <memory.h>
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

//=:util:ascii
static inline bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n';
}

static inline bool is_lower(char c) { return c >= 'a' && c <= 'z'; }

static inline bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

static inline bool is_letter(char c) {
  return is_lower(c) || is_upper(c) || c == '_';
}

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

//=:util:data_structures
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} StringBuffer;

//=:util:types
typedef double complex cmx_t;

typedef double flt_t;

#ifdef BITINT_MAXWIDTH
typedef _BitInt(BITINT_MAXWIDTH) int_t;

typedef unsigned _BitInt(BITINT_MAXWIDTH) unt_t;
#else
typedef int64_t int_t;

typedef uint64_t unt_t;
#endif

typedef bool bol_t;

#define INT_T_MAX ((int_t)(((unt_t)1 << (sizeof(int_t) * 8 - 1)) - 1))

#define ENC_OFF ('Z' - 'A' + 1)
#endif

//=:util:memory
size_t align(size_t sz, size_t alignment);

//=:util:encoding
unt_t encode_symbol_c(char c);

char decode_symbol_c(char c);

char *decode_symbol(char *dst, char *dst_end, unt_t src);

//=:runtime
//                      _   _
//                     | | (_)
//     _ __ _   _ _ __ | |_ _ _ __ ___   ___
//    | '__| | | | '_ \| __| | '_ ` _ \ / _	;
//    | |  | |_| | | | | |_| | | | | | |  __/
//    |_|   \__,_|_| |_|\__|_|_| |_| |_|\___|

char *int_stringify(char *dst, char *dst_end, int_t num);

typedef union {
  cmx_t n_cmx;
  flt_t n_flt;
  int_t n_int;
  unt_t n_unt;
  bol_t n_bol;
} Primitive;

int_t pow_int(int_t base, int_t expo);

int_t fac_int(int_t base, int_t step);

flt_t fac_flt(flt_t base, flt_t step);

#if '\x12\x34\x56\x78' == 0x12345678
#define USE_BIG_ENDIAN
#endif

typedef union {
  flt_t lit;
#ifdef USE_BIG_ENDIAN
  struct {
    unt_t mant : DBL_MANT_DIG - 1;
    unt_t expo : sizeof(flt_t) * 8 - DBL_MANT_DIG;
    unt_t sign : 1;
  };
#else
  struct {
    unt_t sign : 1;
    unt_t expo : sizeof(flt_t) * 8 - DBL_MANT_DIG;
    unt_t mant : DBL_MANT_DIG - 1;
  };
#endif
} IEEE754_flt_t;

bol_t is_almost_equal_flt(flt_t x, flt_t y);

bol_t is_almost_equal_cmx(cmx_t x, cmx_t y);
