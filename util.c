#include "util.h"

#include <assert.h>
#include <math.h>

//=:util
//           _   _ _
//          | | (_) |
//     _   _| |_ _| |
//    | | | | __| | |
//    | |_| | |_| | |
//     \__,_|\__|_|_|

//=:util:encoding
unt_t encode_symbol_c(char c) {
  if (IS_UPPER(c))
    return (c - 'A') + 1;
  if (IS_LOWER(c))
    return (c - 'a') + ENC_OFF + 1;
  if (c == '_')
    return ENC_OFF * 2 + 1;
  if (IS_DIGIT(c))
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
//                      _   _
//                     | | (_)
//     _ __ _   _ _ __ | |_ _ _ __ ___   ___
//    | '__| | | | '_ \| __| | '_ ` _ \ / _	\
//    | |  | |_| | | | | |_| | | | | | |  __/
//    |_|   \__,_|_| |_|\__|_|_| |_| |_|\___|

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
int_t pow_int(int_t base, int_t expo) {
  if (expo == 0)
    return 1;
  if (expo == 1)
    return base;

  int_t rt = 1;
  while (expo > 0) {
    if (expo % 2 == 1)
      rt *= base;
    expo /= 2;
    base *= base;
  }
  return rt;
}

int_t fac_int(int_t base, int_t step) {
  if (base == 0)
    return 1;

  int_t rt = 1;
  for (int_t i = base; i >= 2; i -= step)
    rt *= i;

  return rt;
}

flt_t fac_flt_util(flt_t i, flt_t step) {
  flt_t rt = 0;

  for (int_t j = 1; j <= step; ++j)
    rt += cosl(acosl(cosl(2 * j * M_PI / step)) * i);

  return rt / step;
}

flt_t fac_flt(flt_t base, flt_t step) {
  flt_t rt = powl(step, base / step) * tgammal(1 + base / step);

  for (int_t i = 1; i < step; ++i)
    rt *= powl(powl(step, (step - i) / step) / tgammal(i / step),
               fac_flt_util(base - i, step));

  return rt;
}
