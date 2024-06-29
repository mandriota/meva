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

#include "intratypes.h"

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

#define WARNING(...)                                                           \
  {                                                                            \
    fprintf(stderr, CLR_WRN_MSG "WARNING" CLR_RESET ": " __VA_ARGS__);         \
    fflush(stderr);                                                            \
  }

#define WARNING_INT_TO_CMX(reason)                                             \
  {                                                                            \
    WARNING("number was converted into complex due to " reason "\n");          \
    WARNING("this will result in accuracy reduction\n");                       \
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

#define MAX(a, b) (a >= b ? a : b)

#define ABS(a) (a >= 0 ? a : -a)

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

static inline size_t align(size_t sz, size_t alignment) {
  return sz + (alignment - ((sz - 1) & (alignment - 1))) - 1;
}

//=:util:encoding

unt_t encode_symbol_c(char c);

char decode_symbol_c(char c);

char *decode_symbol(char *dst, char *dst_end, unt_t src);

//=:runtime

char *int_stringify(char *dst, char *dst_end, int_t num);

typedef union {
  cmx_t c;
  int_t i;
  unt_t u;
  bol_t b;
} Primitive;

//=:runtime:operators

static inline Node_Type add_int(Primitive *rt, int_t a, int_t b) {
  if ((a < 0) == (b < 0) && INT_T_MAX - ABS(a) < ABS(b)) {
    rt->c = (cmx_t)a + b;
    return NT_PRIM_CMX;
  }

  rt->i = a + b;
  return NT_PRIM_INT;
}

static inline Node_Type sub_int(Primitive *rt, int_t a, int_t b) {
  return add_int(rt, a, -b);
}

static inline Node_Type mul_int(Primitive *rt, int_t a, int_t b) {
  if (INT_T_MAX / b < a && b != 0) {
    rt->c = (cmx_t)a * b;
    return NT_PRIM_CMX;
  }

  rt->i = a * b;
  return NT_PRIM_INT;
}

Node_Type pow_int(Primitive *rt, int_t base, int_t expo);

Node_Type fac_int(Primitive *rt, int_t base, int_t step);

Node_Type fac_cmx(Primitive *rt, cmx_t base, int_t step);

Node_Type subfac_int(Primitive *rt, int_t base);

Node_Type subfac_cmx(Primitive *rt, cmx_t base);

//=:intratypes:stringify

static inline const char *tt_stringify(Token_Type tt) {
  switch (tt) {
    STRINGIFY_CASE(TT_ILL)
    STRINGIFY_CASE(TT_EOS)
    STRINGIFY_CASE(TT_SYM)
    STRINGIFY_CASE(TT_INT)
    STRINGIFY_CASE(TT_CMX)
    STRINGIFY_CASE(TT_FAL)
    STRINGIFY_CASE(TT_TRU)
    STRINGIFY_CASE(TT_LET)
    STRINGIFY_CASE(TT_AND)
    STRINGIFY_CASE(TT_ORR)
    STRINGIFY_CASE(TT_GRE)
    STRINGIFY_CASE(TT_LES)
    STRINGIFY_CASE(TT_GEQ)
    STRINGIFY_CASE(TT_LEQ)
    STRINGIFY_CASE(TT_EQU)
    STRINGIFY_CASE(TT_NEQ)
    STRINGIFY_CASE(TT_ADD)
    STRINGIFY_CASE(TT_SUB)
    STRINGIFY_CASE(TT_MUL)
    STRINGIFY_CASE(TT_QUO)
    STRINGIFY_CASE(TT_MOD)
    STRINGIFY_CASE(TT_POW)
    STRINGIFY_CASE(TT_NOT)
    STRINGIFY_CASE(TT_FAC)
    STRINGIFY_CASE(TT_LP0)
    STRINGIFY_CASE(TT_RP0)
    STRINGIFY_CASE(TT_ABS)
    STRINGIFY_CASE(TT_EOX)
  }

  return STRINGIFY(INVALID_TT);
}

static inline const char *nt_stringify(Node_Type nt) {
  switch (nt) {
    STRINGIFY_CASE(NT_PRIM_SYM)
    STRINGIFY_CASE(NT_PRIM_INT)
    STRINGIFY_CASE(NT_PRIM_CMX)
    STRINGIFY_CASE(NT_PRIM_BOL)
    STRINGIFY_CASE(NT_BIOP_LET)
    STRINGIFY_CASE(NT_BIOP_AND)
    STRINGIFY_CASE(NT_BIOP_ORR)
    STRINGIFY_CASE(NT_BIOP_GRE)
    STRINGIFY_CASE(NT_BIOP_LES)
    STRINGIFY_CASE(NT_BIOP_GEQ)
    STRINGIFY_CASE(NT_BIOP_LEQ)
    STRINGIFY_CASE(NT_BIOP_EQU)
    STRINGIFY_CASE(NT_BIOP_NEQ)
    STRINGIFY_CASE(NT_BIOP_ADD)
    STRINGIFY_CASE(NT_BIOP_SUB)
    STRINGIFY_CASE(NT_BIOP_MUL)
    STRINGIFY_CASE(NT_BIOP_QUO)
    STRINGIFY_CASE(NT_BIOP_MOD)
    STRINGIFY_CASE(NT_BIOP_POW)
    STRINGIFY_CASE(NT_BIOP_FAC)
    STRINGIFY_CASE(NT_UNOP_ABS)
    STRINGIFY_CASE(NT_UNOP_NOT)
    STRINGIFY_CASE(NT_UNOP_NOP)
    STRINGIFY_CASE(NT_UNOP_NEG)
    STRINGIFY_CASE(NT_FUNC)
    STRINGIFY_CASE(NT_CALL)
    STRINGIFY_CASE(NT_CALL_ANON)
  }

  return STRINGIFY(INVALID_NT);
}
