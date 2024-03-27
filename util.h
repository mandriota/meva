#ifndef UTIL_H
#define UTIL_H

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

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
  fprintf(stderr, CLR_ERR_MSG "ERROR" CLR_RESET ": " __VA_ARGS__)

#define TRY(prefix, expr)                                                      \
  {                                                                            \
    enum prefix err = expr;                                                    \
    if (err != prefix##_NOERROR)                                               \
      return err;                                                              \
  }

//=:util:debug
#ifdef NDEBUG
#define DBG_PRINT(...)
#define DBG_FATAL(...)
#define DBG(x)
#else
#define DBG_PRINT(...)                                                         \
  fprintf(stderr, CLR_INF_MSG "INFO" CLR_RESET ": " __VA_ARGS__)
#define DBG_FATAL(...) FATAL(__VA_ARGS__)
#define DBG(x) x
#endif

//=:util:other
#define STRINGIFY(name) #name

#define STRINGIFY_CASE(name)                                                   \
  case name:                                                                   \
    return STRINGIFY(name);

//=:util:ascii
#define IS_WHITESPACE(c)                                                       \
  (c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n')

#define IS_LOWER(c) (c >= 'a' && c <= 'z')

#define IS_UPPER(c) (c >= 'A' && c <= 'Z')

#define IS_LETTER(c) (IS_LOWER(c) || IS_UPPER(c) || c == '_')

#define IS_DIGIT(c) (c >= '0' && c <= '9')

//=:util:data_structures
struct StringBuffer {
  char *data;
  size_t len;
  size_t cap;
};

//=:util:types
typedef long double flt_t;

typedef _BitInt(128) int_t;

typedef unsigned _BitInt(128) unt_t;

#define INT_T_MAX ((int_t)(((unt_t)1 << (sizeof(int_t) * 8 - 1)) - 1))

#define ENC_OFF ('Z' - 'A' + 1)
#endif

//=:util:encoding
unt_t encode_symbol_c(char c);

char decode_symbol_c(char c);

char *decode_symbol(char *dst, char *dst_end, unt_t src);

//=:runtime
//                      _   _
//                     | | (_)
//     _ __ _   _ _ __ | |_ _ _ __ ___   ___
//    | '__| | | | '_ \| __| | '_ ` _ \ / _	\
//    | |  | |_| | | | | |_| | | | | | |  __/
//    |_|   \__,_|_| |_|\__|_|_| |_| |_|\___|

char *int_stringify(char *dst, char *dst_end, int_t num);

union Primitive {
  flt_t n_flt;
  int_t n_int;
  unt_t n_unt;
};

int_t pow_int(int_t base, int_t expo);

int_t fac_int(int_t base, int_t step);

flt_t fac_flt(flt_t z, flt_t a);
