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
#include <stdbool.h> // IWYU pragma: keep
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tgmath.h> // IWYU pragma: keep
// IWYU pragma: no_include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

//=:util:error_handling

#define FATAL(...)                                                   \
  {                                                                  \
    fprintf(stderr, CLR_ERR_MSG "FATAL" CLR_RESET ": " __VA_ARGS__); \
    exit(EXIT_FAILURE);                                              \
  }

#define PFATAL(s)       \
  {                     \
    perror(s);          \
    exit(EXIT_FAILURE); \
  }

#define ERROR(...)                                                   \
  {                                                                  \
    fprintf(stderr, CLR_ERR_MSG "ERROR" CLR_RESET ": " __VA_ARGS__); \
    fflush(stderr);                                                  \
  }

#define WARNING(...)                                                   \
  {                                                                    \
    fprintf(stderr, CLR_WRN_MSG "WARNING" CLR_RESET ": " __VA_ARGS__); \
    fflush(stderr);                                                    \
  }

#define TRY(type, expr)        \
  {                            \
    type err = expr;           \
    if (err != type##_NOERROR) \
      return err;              \
  }

//=:util:debug

#ifdef NDEBUG
#define DBG_PRINT(...)
#define DBG_FATAL(...)
#define DBG(x)
#else
#define DBG_PRINT(...)                                              \
  {                                                                 \
    fprintf(stderr, CLR_INF_MSG "INFO" CLR_RESET ": " __VA_ARGS__); \
    fflush(stderr);                                                 \
  }
#define DBG_FATAL(...) FATAL(__VA_ARGS__)
#define DBG(x) x
#endif

//=:util:other

#define STRINGIFY(name) #name

#define STRINGIFY_CASE(name) \
  case name:                 \
    return STRINGIFY(name);

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <memory.h>
ssize_t getline(char **restrict lineptr, size_t *restrict n,
                FILE *restrict stream) {
  if (*lineptr == NULL) {
    *n = 512;
    *lineptr = (char *)malloc(*n * sizeof(char));
    if (*lineptr == NULL)
      return -1;
  }

  size_t cp = 0;
  char cc = 0;

  do {
    cc = (*lineptr)[cp] = getc(stream);

    ++cp;
    if (cp >= *n) {
      *n *= 2;
      *lineptr = (char *)realloc(*lineptr, *n);
      if (*lineptr == NULL)
        return -1;
    }
  } while (cc != '\n' && !ferror(stream));

  if (ferror(stream) && !feof(stream))
    return -1;

  return cp;
}
#endif

#define MAX(a, b) (a >= b ? a : b)

#define ABS(a) (a >= 0 ? a : -a)

//=:util:ascii

static inline bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n'; }

static inline bool is_lower(char c) { return c >= 'a' && c <= 'z'; }

static inline bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

static inline bool is_letter(char c) { return is_lower(c) || is_upper(c) || c == '_'; }

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

//=:util:data_structures

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} String_Buffer;

//=:util:types

typedef double complex cmx_t;

typedef uint64_t sym_t;

typedef bool bol_t;

#define SYM_T_BITSIZE (sizeof(sym_t) * 8)

#define ENC_OFF ('Z' - 'A' + 1)

//=:util:encoding

char encode_symbol_c(char c) {
  if (is_upper(c))
    return (c - 'A') + 1;
  if (is_lower(c))
    return (c - 'a') + ENC_OFF + 1;
  if (c == '_')
    return ENC_OFF * 2 + 1;
  if (is_digit(c))
    return (c - '0') + ENC_OFF * 2 + 2;

  DBG_FATAL("symbol's character (%d) is out of range", c);
  return 0;
}

char decode_symbol_c(char c) {
  if (c >= 1 && c <= ENC_OFF)
    return (c + 'A') - 1;
  if (c >= ENC_OFF + 1 && c <= ENC_OFF * 2)
    return (c + 'a') - ENC_OFF - 1;
  if (c == ENC_OFF * 2 + 1)
    return '_';
  if (c >= ENC_OFF * 2 + 2 && c <= ENC_OFF * 2 + 12)
    return (c + '0') - ENC_OFF * 2 - 2;

  DBG_FATAL("symbol's character (%d) is out of range", c);
  return 0;
}

char *decode_symbol(char *dst, char *dst_end, sym_t src) {
  char *p, c;

  for (p = dst; src && p < dst_end; ++p) {
    c = src & ((1 << 6) - 1);
    *p = decode_symbol_c(c);
    src >>= 6;
  }

  if (src != 0)
    FATAL("buffer capacity is not enough\n");

  return p;
}

//=:lexer:tokens

typedef enum {
  TT_ILL = -1,
  TT_EOS,

  TT_SYM,
  TT_CMX,

  TT_LET,

  TT_GRE,
  TT_LES,
  TT_GEQ,
  TT_LEQ,
  TT_EQU,
  TT_NEQ,

  TT_ADD,
  TT_SUB,
  TT_APX,

  TT_MUL,
  TT_QUO,
  TT_MOD,

  TT_POW,

  TT_XPC,
  TT_SPZ,

  TT_NEG,
  TT_NOP,
  TT_NOT,
  TT_FAC,

  TT_LP0,
  TT_RP0,

  TT_ABS,
} Token_Type;

//=:lexer:tokens:stringify

static inline const char *tt_stringify(Token_Type tt) {
  switch (tt) {
    STRINGIFY_CASE(TT_ILL)
    STRINGIFY_CASE(TT_EOS)
    STRINGIFY_CASE(TT_SYM)
    STRINGIFY_CASE(TT_CMX)
    STRINGIFY_CASE(TT_LET)
    STRINGIFY_CASE(TT_GRE)
    STRINGIFY_CASE(TT_LES)
    STRINGIFY_CASE(TT_GEQ)
    STRINGIFY_CASE(TT_LEQ)
    STRINGIFY_CASE(TT_EQU)
    STRINGIFY_CASE(TT_NEQ)
    STRINGIFY_CASE(TT_ADD)
    STRINGIFY_CASE(TT_SUB)
    STRINGIFY_CASE(TT_APX)
    STRINGIFY_CASE(TT_MUL)
    STRINGIFY_CASE(TT_QUO)
    STRINGIFY_CASE(TT_MOD)
    STRINGIFY_CASE(TT_POW)
    STRINGIFY_CASE(TT_XPC)
    STRINGIFY_CASE(TT_SPZ)
    STRINGIFY_CASE(TT_NEG)
    STRINGIFY_CASE(TT_NOP)
    STRINGIFY_CASE(TT_NOT)
    STRINGIFY_CASE(TT_FAC)
    STRINGIFY_CASE(TT_LP0)
    STRINGIFY_CASE(TT_RP0)
    STRINGIFY_CASE(TT_ABS)
  }

  return STRINGIFY(INVALID_TT);
}

//=:parser:nodes

typedef enum {
  NT_PRIM_SYM = TT_SYM,
  NT_PRIM_CMX = TT_CMX,

  NT_BIOP_LET = TT_LET,

  NT_BIOP_GRE = TT_GRE,
  NT_BIOP_LES = TT_LES,
  NT_BIOP_GEQ = TT_GEQ,
  NT_BIOP_LEQ = TT_LEQ,
  NT_BIOP_EQU = TT_EQU,
  NT_BIOP_NEQ = TT_NEQ,

  NT_BIOP_ADD = TT_ADD,
  NT_BIOP_SUB = TT_SUB,
  NT_BIOP_APX = TT_APX,

  NT_BIOP_MUL = TT_MUL,
  NT_BIOP_QUO = TT_QUO,
  NT_BIOP_MOD = TT_MOD,

  NT_BIOP_POW = TT_POW,

  NT_BIOP_XPC = TT_XPC,
  NT_BIOP_SPZ = TT_SPZ,

  NT_BIOP_FAC = TT_FAC,

  NT_UNOP_ABS = TT_ABS,

  NT_UNOP_NOT,
  NT_UNOP_NOP,
  NT_UNOP_NEG,

  NT_CALL,
} Node_Type;

//=:parser:nodes:stringify

static inline const char *nt_stringify(Node_Type nt) {
  switch (nt) {
    STRINGIFY_CASE(NT_PRIM_SYM)
    STRINGIFY_CASE(NT_PRIM_CMX)
    STRINGIFY_CASE(NT_BIOP_LET)
    STRINGIFY_CASE(NT_BIOP_GRE)
    STRINGIFY_CASE(NT_BIOP_LES)
    STRINGIFY_CASE(NT_BIOP_GEQ)
    STRINGIFY_CASE(NT_BIOP_LEQ)
    STRINGIFY_CASE(NT_BIOP_EQU)
    STRINGIFY_CASE(NT_BIOP_NEQ)
    STRINGIFY_CASE(NT_BIOP_ADD)
    STRINGIFY_CASE(NT_BIOP_SUB)
    STRINGIFY_CASE(NT_BIOP_APX)
    STRINGIFY_CASE(NT_BIOP_MUL)
    STRINGIFY_CASE(NT_BIOP_QUO)
    STRINGIFY_CASE(NT_BIOP_MOD)
    STRINGIFY_CASE(NT_BIOP_POW)
    STRINGIFY_CASE(NT_BIOP_XPC)
    STRINGIFY_CASE(NT_BIOP_SPZ)
    STRINGIFY_CASE(NT_BIOP_FAC)
    STRINGIFY_CASE(NT_UNOP_ABS)
    STRINGIFY_CASE(NT_UNOP_NOT)
    STRINGIFY_CASE(NT_UNOP_NOP)
    STRINGIFY_CASE(NT_UNOP_NEG)
    STRINGIFY_CASE(NT_CALL)
  }

  return STRINGIFY(INVALID_NT);
}

//=:parser:tokens:map

Node_Type tt_to_biop_nd(Token_Type tt) {
  switch (tt) {
  case TT_LET: return NT_BIOP_LET;
  case TT_GRE: return NT_BIOP_GRE;
  case TT_LES: return NT_BIOP_LES;
  case TT_GEQ: return NT_BIOP_GEQ;
  case TT_LEQ: return NT_BIOP_LEQ;
  case TT_EQU: return NT_BIOP_EQU;
  case TT_NEQ: return NT_BIOP_NEQ;
  case TT_ADD:
  case TT_NOP: return NT_BIOP_ADD;
  case TT_SUB:
  case TT_NEG: return NT_BIOP_SUB;
  case TT_APX: return NT_BIOP_APX;
  case TT_MUL: return NT_BIOP_MUL;
  case TT_QUO: return NT_BIOP_QUO;
  case TT_MOD: return NT_BIOP_MOD;
  case TT_POW: return NT_BIOP_POW;
  case TT_XPC: return NT_BIOP_XPC;
  case TT_SPZ: return NT_BIOP_SPZ;
  case TT_FAC: return NT_BIOP_FAC;
  case TT_LP0: return NT_CALL;
  default:
    FATAL("illegal biop token");
  }
}

bool is_unop(Node_Type nt) {
  return nt == NT_UNOP_NOT ||
         nt == NT_UNOP_NEG ||
         nt == NT_UNOP_ABS ||
         nt == NT_UNOP_NOP;
}

//=:runtime

typedef union {
  cmx_t c;
  sym_t s;
} Primitive;

//=:runtime:assertions
#define ASSERT_NON_NEG_INT(x, fn, what, rt)        \
  if (x < 0 && fmod(-x, 1) <= MAX_DIFF_ABS) {      \
    WARNING(fn " of negative integer " what "\n"); \
    return rt;                                     \
  }

#define ASSERT_IMG_ZER(x, fn)                                                \
  if (cimag(x) != 0) {                                                       \
    WARNING(fn                                                               \
            " of a number with imaginary part != 0 is not implemented yet"); \
    return NAN;                                                              \
  }

//=:runtime:operators

double contains_interval(double a, double a_re, double b, double b_re) {
  double al = (1 - a_re) * a, ah = (1 + a_re) * a;
  double bl = (1 - b_re) * b, bh = (1 + b_re) * b;

  if ((al >= bl && ah <= bh) || (bl >= al && bh <= ah))
    return 1;

  if (ah < bl || bl > ah)
    return 0;

  if (ah > bh)
    return (bh - al) / (ah - bl);

  return (ah - bl) / (bh - al);
}

cmx_t fac_cmx_helper(cmx_t i, uint64_t step) {
  cmx_t rt = 0;

  for (uint64_t j = 1; j <= step; ++j)
    rt += cos(acos(cos(2 * j * M_PI / step)) * i);

  return rt / step;
}

cmx_t fac_cmx(cmx_t base, cmx_t step) {
  ASSERT_IMG_ZER(base, "factorial");

  double rbase = creal(base);
  double rstep = creal(step);
  uint64_t ustep = (uint64_t)step;

  ASSERT_NON_NEG_INT(rbase, "factorial", "is equal to infinity", INFINITY);

  cmx_t rt = pow(rstep, rbase / rstep) * tgamma(1 + rbase / rstep);

  for (uint64_t i = 1; i < ustep; ++i)
    rt *= pow(pow(rstep, (rstep - i) / rstep) / tgamma(i / rstep),
              fac_cmx_helper(rbase - i, ustep));

  return rt;
}

enum {
  GAMMA_LOWER_QUO_E_ITER = 1 << 6,
};

cmx_t gamma_lower_quo_e(double s) {
  cmx_t rt = 0;

  for (int i = 0; i <= GAMMA_LOWER_QUO_E_ITER; ++i)
    rt += (i % 2 == 0 ? 1 : -1) * tgamma(s) / (tgamma(s + i + 1));

  return cpow((cmx_t)-1, (cmx_t)s) * rt;
}

cmx_t subfac_cmx(cmx_t base) {
  ASSERT_IMG_ZER(base, "subfactorial");

  double rbase = creal(base);

  ASSERT_NON_NEG_INT(rbase, "subfactorial", "currently is not implemented", NAN);

  if (rbase >= 0 && fmod(rbase, 1) <= MAX_DIFF_ABS)
    return creal(tgamma(rbase + 1) / M_E - gamma_lower_quo_e(rbase + 1));

  return tgamma(rbase + 1) / M_E - gamma_lower_quo_e(base + 1);
}

#endif
