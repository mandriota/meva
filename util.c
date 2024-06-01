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

#include "util.h"
#include "intratypes.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <tgmath.h>

//=:util

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
      if (lineptr == NULL)
        return -1;
    }
  } while (cc != '\n' && !ferror(stream));

  if (ferror(stream) && !feof(stream))
    return -1;

  return cp;
}

//=:util:encoding

unt_t encode_symbol_c(char c) {
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

char *decode_symbol(char *dst, char *dst_end, unt_t src) {
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

//=:runtime

char *int_stringify(char *dst, char *dst_end, int_t num) {
  assert(dst_end >= dst);

  char *p = dst_end + 1;
  int_t n = num;

  if (n == 0) {
    *--p = '0';
    return p;
  }

  while (n) {
    if (p == dst)
      FATAL("buffer capacity is not enough\n");

    *--p = (num < 0 ? -1 : 1) * (n % 10) + '0';
    n /= 10;
  }

  if (num < 0)
    *--p = '-';

  return p;
}

//=:runtime:operators

NodeType pow_int(Primitive *rt, int_t base, int_t expo) {
  rt->n_int = 1;
  NodeType rtt = NT_PRIM_INT;

  if (expo == 0)
    return rtt;
  if (expo == 1) {
    rt->n_int = base;
    return rtt;
  }

  Primitive base_tmp = (Primitive){.n_int = base};
  int_t expo_tmp = expo;

  while (true) {
    if (expo_tmp % 2 == 1)
      rtt = mul_int(rt, rt->n_int, base_tmp.n_int);

    if (rtt != NT_PRIM_INT)
      goto pow_flt;

    expo_tmp /= 2;

    if (expo_tmp == 0)
      break;

    rtt = mul_int(&base_tmp, base_tmp.n_int, base_tmp.n_int);
  }

  return NT_PRIM_INT;

pow_flt:
  rt->n_flt = pow((flt_t)base, (flt_t)expo);
  return NT_PRIM_FLT;
}

NodeType fac_int(Primitive *rt, int_t base, int_t step) {
  if (base < 0) {
    WARNING("factorial of negative integer is equal to infinity\n");
    rt->n_flt = INFINITY;
    return NT_PRIM_FLT;
  }

  if (base == 0)
    return 1;

  rt->n_int = 1;
  NodeType rtt = NT_PRIM_INT;

  int_t i = base;
  for (; i >= 2 && rtt == NT_PRIM_INT; i -= step)
    rtt = mul_int(rt, rt->n_int, i);

  for (; i >= 2; i -= step)
    rt->n_flt *= i;

  return rtt;
}

flt_t fac_flt_helper(flt_t i, flt_t step) {
  flt_t rt = 0;

  for (int_t j = 1; j <= step; ++j)
    rt += cos(acos(cos(2 * j * M_PI / step)) * i);

  return rt / step;
}

#define ASSERT_NOT_NEGATIVE_INT(x, fn, what, rt, action)                       \
  if (base < 0 && fmod(-base, 1) <= MAX_DIFF_ABS) {                            \
    WARNING(fn " of negative integer " what "\n");                             \
    action;                                                                    \
    return rt;                                                                 \
  }

NodeType fac_flt(Primitive *rt, flt_t base, flt_t step) {
  ASSERT_NOT_NEGATIVE_INT(base, "factorial", "is equal to infinity",
                          NT_PRIM_FLT, rt->n_flt = INFINITY);

  rt->n_flt = pow(step, base / step) * tgamma(1 + base / step);

  for (int_t i = 1; i < step; ++i)
    rt->n_flt *= pow(pow(step, (step - i) / step) / tgamma(i / step),
                     fac_flt_helper(base - i, step));

  return NT_PRIM_FLT;
}

NodeType subfac_int(Primitive *rt, int_t base) {
  if (base < 0)
    return subfac_flt(rt, base);

  rt->n_int = 1;
  NodeType rtt = NT_PRIM_INT;

  int_t i = 1;

  for (; i <= base && rtt == NT_PRIM_INT; ++i) {
    rtt = mul_int(rt, i, rt->n_int);
    if (rtt != NT_PRIM_INT)
      continue;
    rtt = add_int(rt, rt->n_int, i % 2 == 0 ? 1 : -1);
  }

  for (; i <= base; ++i)
    rt->n_flt = (i % 2 == 0 ? 1 : -1) + i * rt->n_flt;

  return rtt;
}

enum {
  GAMMA_LOWER_QUO_E_ITER = 1 << 6,
};

cmx_t gamma_lower_quo_e(flt_t s) {
  cmx_t rt = 0;

  for (int i = 0; i <= GAMMA_LOWER_QUO_E_ITER; ++i)
    rt += (i % 2 == 0 ? 1 : -1) * tgamma(s) / (tgamma(s + i + 1));

  return cpow((cmx_t)-1, (cmx_t)s) * rt;
}

NodeType subfac_flt(Primitive *rt, flt_t base) {
  ASSERT_NOT_NEGATIVE_INT(base, "subfactorial", "currently is not implemented",
                          NT_PRIM_FLT, rt->n_flt = NAN);

  if (base >= 0 && fmod(base, 1) <= MAX_DIFF_ABS) {
    rt->n_flt = creal(tgamma(base + 1) / M_E - gamma_lower_quo_e(base + 1));
    return NT_PRIM_FLT;
  }
  rt->n_cmx = tgamma(base + 1) / M_E - gamma_lower_quo_e(base + 1);
  return NT_PRIM_CMX;
}
