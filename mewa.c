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
*                                                                              *
*               :::   :::  :::::::::::::       :::    :::                      *
*             :+:+: :+:+: :+:       :+:       :+:  :+: :+:                     *
*           +:+ +:+:+ +:++:+       +:+       +:+ +:+   +:+                     *
*          +#+  +:+  +#++#++:++#  +#+  +:+  +#++#++:++#++:                     *
*         +#+       +#++#+       +#+ +#+#+ +#++#+     +#+                      *
*        #+#       #+##+#        #+#+# #+#+# #+#     #+#                       *
*       ###       #############  ###   ###  ###     ###                        *
*                                                                              *
*                                                                              *
\******************************************************************************/

//=:includes
#include "config.h"

#include "hmap.h"
#include "util.h"

#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stdbool.h> // IWYU pragma: keep
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>

#ifdef HAVE_LIBREADLINE
#include <readline/history.h> // IWYU pragma: keep
#include <readline/readline.h>
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <io.h>
#define isatty(h) _isatty(h)
#else
#define isatty(h) 1
#warning cannot find isatty implementation, \
therefore, Mewa will always start REPL \
when no command line arguments are passed
#endif

//=:config:invariant

_Static_assert(INTERNAL_READING_BUF_SIZE > 0,
               "INTERNAL_READING_BUF_SIZE must be at least 1");

_Static_assert(NODE_BUF_SIZE > 0, "NODE_BUF_SIZE must be at least 1");

_Static_assert(GLOBAL_SCOPE_CAPACITY >= 4, "not enough capacity for builtins");

//=:reader:reader

typedef struct {
  String_Buffer page;

  FILE *src;

  size_t ptr;
  size_t mrk;
  size_t row;
  size_t col;

  char cch;

  bool eof;
  bool eos;
  bool eoi;
  bool prv;
} Reader;

void rd_reset_counters(Reader *rd) {
  rd->ptr = 0;
  rd->mrk = 0;
  rd->row = 0;
  rd->col = 0;
  rd->eof = false;
  rd->eos = false;
  rd->eoi = false;
  rd->prv = false;
}

void rd_prev(Reader *rd) {
  rd->prv = rd->mrk == SIZE_MAX || rd->mrk != rd->ptr;
}

void rd_next_page(Reader *rd) {
  rd->ptr = 0;
  rd->mrk = SIZE_MAX;

  if (rd->src == NULL) {
    rd->eos = rd->eof = rd->eoi;
    return;
  }

  rd->page.len = fread(rd->page.data, sizeof(char), rd->page.cap, rd->src);
  if (ferror(rd->src))
    PFATAL("cannot read file\n");

  rd->eof = rd->page.len < rd->page.cap;
  if ((rd->eos = !rd->page.len))
    rd->cch = '\0';
}

void rd_next_char(Reader *rd) {
  if (rd->prv) {
    rd->prv = false;
    return;
  }

  ++rd->col;
  if (rd->page.data[rd->ptr] == '\n') {
    rd->col = 0;
    ++rd->row;
  }
  ++rd->ptr;

  if (rd->ptr >= rd->page.len || (rd->src == NULL && !rd->eoi)) {
    if (rd->eof || (rd->src == NULL && rd->eoi)) {
      rd->eos = true;
      rd->cch = '\0';
      return;
    }
    rd_next_page(rd);
    rd->eoi = true;
  }

  rd->cch = rd->page.data[rd->ptr];
}

void rd_skip_whitespaces(Reader *rd) {
  while (is_whitespace(rd->cch))
    rd_next_char(rd);
}

void rd_skip_line(Reader *rd) {
  while (rd->cch != '\0' && rd->cch != '\n')
    rd_next_char(rd);
}

//=:lexer:lexer

typedef struct {
  Reader rd;

  Token_Type tt;
  float rel_err;
  Primitive pm;
} Lexer;

double lx_read_integer(Lexer *lx, double *log10, float *rel_err) {
  long long test_integer = 0;
  double integer = 0;
  *log10 = 0;

  while (is_digit(lx->rd.cch)) {
    test_integer = test_integer * 10 + lx->rd.cch - '0';
    integer = integer * 10 + lx->rd.cch - '0';
    *log10 += 1;

    rd_next_char(&lx->rd);
  }

  if (rel_err != NULL && integer < ldexp(1, 63) && integer != 0)
    *rel_err = fabs((((long long)integer) - test_integer) / integer);

  return integer;
}

void lx_next_token_number(Lexer *lx) {
  lx->tt = TT_ILL;

  lx->rel_err = 0;

  double decimal, decimal_log10, integer_log10;
  lx->pm.c = lx_read_integer(lx, &integer_log10, &lx->rel_err);

  if (lx->rd.cch == '.') {
    rd_next_char(&lx->rd);

    decimal = lx_read_integer(lx, &decimal_log10, NULL);
    if (decimal_log10 == 0 && integer_log10 == 0)
      return;
    lx->pm.c += (double)decimal / pow(10, decimal_log10);

    if (decimal != 0)
      lx->rel_err += (float)((nextafter(creal(lx->pm.c), INFINITY) - creal(lx->pm.c)) / creal(lx->pm.c));
  }

  DBG_PRINT("rel_err: %e\n", lx->rel_err);

  //  lx->rel_err = pow(10, -15);

  lx->tt = TT_CMX;

  if (lx->rd.cch == 'i') {
    lx->pm.c = lx->pm.c * I;
  } else {
    rd_prev(&lx->rd);
  }
}

void lx_next_token_symbol(Lexer *lx) {
  lx->tt = TT_ILL;
  lx->pm.s = 0;

  unsigned bit_off = 0;

  do {
    lx->pm.s |= (sym_t)encode_symbol_c(lx->rd.cch) << bit_off;
    bit_off += 6;
    if (bit_off > SYM_T_BITSIZE) {
      ERROR("identifier is too long (> %lu)\n", SYM_T_BITSIZE / 6);
      return;
    }
    rd_next_char(&lx->rd);
  } while (is_letter(lx->rd.cch) || is_digit(lx->rd.cch));

  rd_prev(&lx->rd);
  lx->tt = TT_SYM;
}

void lx_next_token_factorial(Lexer *lx, bool whitespace_prefix) {
  lx->tt = TT_ILL;

  unsigned c = 0;
  for (; lx->rd.cch == '!'; ++c)
    rd_next_char(&lx->rd);

  lx->pm.c = (double)c;

  if (c == 1 && lx->rd.cch == '=') {
    lx->tt = TT_NEQ;
    return;
  }

  if (c == 1 && !is_whitespace(lx->rd.cch) && lx->rd.cch != '\0') {
    lx->tt = TT_NOT;
  } else if (!whitespace_prefix && lx->rd.ptr != 1) {
    lx->tt = TT_FAC;
  }

  rd_prev(&lx->rd);
}

#define LX_TRY_C(on_success_tt, fn, ...) \
  {                                      \
    if (fn) {                            \
      lx->tt = on_success_tt;            \
      __VA_ARGS__;                       \
      return;                            \
    }                                    \
  }

#define LX_LOOKUP(on_failure_tt, consumer) \
  {                                        \
    rd_next_char(&lx->rd);                 \
    consumer;                              \
    rd_prev(&lx->rd);                      \
    lx->tt = on_failure_tt;                \
  }

void lx_next_token(Lexer *lx) {
  rd_next_char(&lx->rd);
  bool whitespace_prefix = is_whitespace(lx->rd.cch);
  rd_skip_whitespaces(&lx->rd);

  lx->rd.mrk = lx->rd.ptr;
  lx->tt = TT_ILL;

  switch (lx->rd.cch) {
  case '+':  LX_LOOKUP(lx->tt = TT_NOP, LX_TRY_C(TT_ADD, is_whitespace(lx->rd.cch), ) LX_TRY_C(TT_APX, lx->rd.cch == '/', )); break;
  case '-':  LX_LOOKUP(lx->tt = TT_NEG, LX_TRY_C(TT_SPZ, lx->rd.cch == '>', ) LX_TRY_C(TT_SUB, is_whitespace(lx->rd.cch), )); break;
  case '*':  lx->tt = TT_MUL; break;
  case '/':  lx->tt = TT_QUO; break;
  case '%':  lx->tt = TT_MOD; break;
  case '^':  lx->tt = TT_POW; break;
  case '(':  lx->tt = TT_LP0; break;
  case ')':  lx->tt = TT_RP0; break;
  case ';':  lx->tt = TT_XPC; break;
  case '\0': lx->tt = TT_EOS; break;
  case '!':  lx_next_token_factorial(lx, whitespace_prefix); break;
  case '|':  lx->tt = TT_ABS; break;
  case '>':  LX_LOOKUP(TT_GRE, LX_TRY_C(TT_GEQ, lx->rd.cch == '=', )); break;
  case '<':  LX_LOOKUP(TT_LES, LX_TRY_C(TT_LEQ, lx->rd.cch == '=', )); break;
  case '=':  LX_LOOKUP(TT_LET, LX_TRY_C(TT_EQU, lx->rd.cch == '=', )); break;
  case 'i':
    lx->tt = TT_CMX;
    lx->pm.c = I;
    break;
  default:
    if (is_digit(lx->rd.cch) || lx->rd.cch == '.') {
      lx_next_token_number(lx);
    } else if (is_letter(lx->rd.cch)) {
      lx_next_token_symbol(lx);
    }
  }
}

//=:parser:nodes

typedef uint32_t Node_Index;

typedef struct {
  Node_Index nhs;
} Un_Op;

typedef struct {
  Node_Index lhs, rhs;
} Bi_Op;

typedef struct Node {
  Node_Type type : 16;
  float rel_err;

  union {
    Primitive pm;
    Un_Op up;
    Bi_Op bp;
  } as;
} Node;

typedef struct {
  Node_Index node;
  Node_Index depth;
} Stack_Emu_El_nd_tree_print;

void nd_tree_print_cmx(cmx_t cmx, float rel_err) {
  if (creal(cmx) != 0 && cimag(cmx) != 0) {
    printf(CLR_PRIM "%lf %lfi", creal(cmx), cimag(cmx));
  } else if (creal(cmx) == 0 && cimag(cmx) == 0) {
    printf(CLR_PRIM "0");
  } else if (creal(cmx) != 0) {
    printf(CLR_PRIM "%lf", creal(cmx));
  } else if (cimag(cmx) != 0)
    printf(CLR_PRIM "%lfi", cimag(cmx));

  if (rel_err != 0) {
    double abs_err = (double)rel_err * fabs(cmx);
    printf(
        CLR_RESET " +/- " CLR_PRIM "%f" CLR_RESET
                  "*" CLR_PRIM "10" CLR_RESET "^" CLR_PRIM "%f\n" CLR_RESET,
        abs_err / pow(10, floor(log10(abs_err))), floor(log10(abs_err)));
  } else {
    printf("\n" CLR_RESET);
  }
}

void nd_tree_print_prb(cmx_t cmx) {
  if (creal(cmx) == 0) {
    printf(CLR_PRIM "false");
  } else if (creal(cmx) == 1) {
    printf(CLR_PRIM "true");
  } else if (creal(cmx) != 0) {
    printf(CLR_PRIM "%lf", creal(cmx));
  }

  printf("\n" CLR_RESET);
}

void nd_tree_print(Stack_Emu_El_nd_tree_print stack_emu[], Node nodes[static 1],
                   Node_Index node, Node_Index depth, Node_Index depth_max) {
  Node_Index len = 1;

  char dst[48];
  char *ptr;
  int ptr_off;

  Node_Index node_tmp;

  do {
    while (depth < depth_max) {
      printf("%*s", depth * 2, "");
#ifndef NDEBUG
      printf(CLR_INTERNAL "%s" CLR_RESET " (%d) ",
             nt_stringify(nodes[node].type), nodes[node].type);
#endif

      switch (nodes[node].type) {
      case NT_PRIM_SYM:
        ptr = decode_symbol(dst, &dst[sizeof dst - 1], nodes[node].as.pm.s);
        ptr_off = ptr - dst;
        printf(CLR_PRIM "%.*s" CLR_RESET " (%llu)\n", ptr_off, dst,
               nodes[node].as.pm.s);
        goto while2_final;
      case NT_PRIM_CMX:
        nd_tree_print_cmx(nodes[node].as.pm.c, nodes[node].rel_err);
        goto while2_final;
      case NT_PRIM_PRB:
        nd_tree_print_prb(nodes[node].as.pm.c);
        goto while2_final;
      case NT_BIOP_LET:
      case NT_BIOP_GRE:
      case NT_BIOP_LES:
      case NT_BIOP_GEQ:
      case NT_BIOP_LEQ:
      case NT_BIOP_EQU:
      case NT_BIOP_NEQ:
      case NT_BIOP_ADD:
      case NT_BIOP_SUB:
      case NT_BIOP_APX:
      case NT_BIOP_MUL:
      case NT_BIOP_QUO:
      case NT_BIOP_MOD:
      case NT_BIOP_POW:
      case NT_BIOP_XPC:
      case NT_BIOP_SPZ:
      case NT_BIOP_FAC:
      case NT_CALL:
        printf("\n");
        node_tmp = node;
        node = nodes[node_tmp].as.bp.lhs;
        ++depth;
        stack_emu[len].node = nodes[node_tmp].as.bp.rhs;
        stack_emu[len].depth = depth;
        ++len;
        continue;
      case NT_UNOP_ABS:
      case NT_UNOP_NOT:
      case NT_UNOP_NEG:
      case NT_UNOP_NOP:
        printf("\n");
        node = nodes[node].as.up.nhs;
        ++depth;
        continue;
      }
    }

    printf("%*s...\n", depth * 2, "");

  while2_final:
    --len;
    node = stack_emu[len].node;
    depth = stack_emu[len].depth;
  } while (len != 0);
}

#define nd_tree_print(nodes, node, depth, depth_max)             \
  {                                                              \
    Stack_Emu_El_nd_tree_print stack_emu[depth_max - depth + 1]; \
    nd_tree_print(stack_emu, nodes, node, depth, depth_max);     \
  }

//=:parser:priorities

typedef enum {
  PT_SKIP_RP0,
  PT_XPC,
  PT_LET0,
  PT_LET1,
  PT_TEST,
  PT_SPZ0,
  PT_SPZ1,
  PT_ADD_SUB,
  PT_MUL_QUO_MOD,
  PT_NOT_NOP_NEG,
  PT_POW0,
  PT_POW1,
  PT_FAC,
  PT_CAL_APX,
  PT_PRIM,
} Priority;

bool pt_includes_tt(Priority pt, Token_Type tt) {
  switch (pt) {
  case PT_XPC:         return tt == TT_XPC;
  case PT_LET0:        return tt == TT_LET;
  case PT_SPZ0:        return tt == TT_SPZ;
  case PT_TEST:        return tt == TT_GRE || tt == TT_LES || tt == TT_GEQ || tt == TT_LEQ || tt == TT_EQU || tt == TT_NEQ;
  case PT_ADD_SUB:     return tt == TT_ADD || tt == TT_NOP || tt == TT_SUB || tt == TT_NEG;
  case PT_MUL_QUO_MOD: return tt == TT_MUL || tt == TT_QUO || tt == TT_MOD;
  case PT_NOT_NOP_NEG: return tt == TT_NOT || tt == TT_NEG || tt == TT_NOP;
  case PT_POW0:        return tt == TT_POW;
  case PT_FAC:         return tt == TT_NOT || tt == TT_FAC;
  case PT_CAL_APX:     return tt == TT_LP0 || tt == TT_APX;
  default:
    DBG_FATAL("%s: unknown priority: %d\n", __func__, pt);
    return false;
  }
}

bool pt_rl_biop(Priority pt) {
  return pt == PT_LET0 || pt == PT_SPZ0 || pt == PT_POW0;
}

//=:parser:errors

typedef enum {
  PR_ERR_NOERROR,
  PR_ERR_GENERAL,
  PR_ERR_PAREN_NOT_OPENED,
  PR_ERR_PAREN_NOT_CLOSED,
  PR_ERR_TOKEN_UNEXPECTED,
  PR_ERR_MEMORY_NOT_ENOUGH,
} PR_ERR;

const char *pr_err_stringify(PR_ERR pr_err) {
  switch (pr_err) {
    STRINGIFY_CASE(PR_ERR_NOERROR)
    STRINGIFY_CASE(PR_ERR_GENERAL)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_OPENED)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_CLOSED)
    STRINGIFY_CASE(PR_ERR_TOKEN_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_MEMORY_NOT_ENOUGH)
  }

  return STRINGIFY(INVALID_PR_ERR);
}

//=:parser:parser

typedef struct {
  Node_Index lower;
  Node_Index upper;
} Node_Bound;

typedef struct {
  Lexer lx;

  ssize_t p0c;
  bool abs;

  Node_Bound *nodes_obj;
  Node_Index nodes_obj_len;
  Node_Index nodes_obj_cap;
  Node_Index nodes_len;
  Node_Index nodes_cap;
  Node nodes[];
} Parser;

PR_ERR pr_nd_alloc(Parser *pr, Node_Index ptr[static 1]) {
  if (pr->nodes_len + 1 >= pr->nodes_cap)
    return PR_ERR_MEMORY_NOT_ENOUGH;

  ptr[0] = pr->nodes_len;
  ++pr->nodes_len;
  return PR_ERR_NOERROR;
}

PR_ERR pr_nd_obj_bound_add(Parser *pr, Node_Index l, Node_Index u) {
  if (pr->nodes_obj_len + 1 >= pr->nodes_obj_cap)
    return PR_ERR_MEMORY_NOT_ENOUGH;

  pr->nodes_obj[pr->nodes_obj_len] = (Node_Bound){l, u};
  ++pr->nodes_obj_len;
  return PR_ERR_NOERROR;
}

PR_ERR pr_call(Parser *pr, Node_Index *node, Priority pt);

PR_ERR pr_next_prim_node(Parser *pr, Node_Index *node, Priority pt) {
  switch (pr->lx.tt) {
  case TT_SYM:
    pr->nodes[*node].type = NT_PRIM_SYM;
    pr->nodes[*node].as.pm.s = pr->lx.pm.s;
    lx_next_token(&pr->lx);
    break;
  case TT_CMX:
    pr->nodes[*node].type = NT_PRIM_CMX;
    pr->nodes[*node].as.pm.c = pr->lx.pm.c;
    pr->nodes[*node].rel_err = pr->lx.rel_err;
    lx_next_token(&pr->lx);
    break;
  case TT_ABS:
    if (pr->abs)
      return PR_ERR_TOKEN_UNEXPECTED;
    pr->abs = true;
    pr->nodes[*node].type = NT_UNOP_ABS;
    TRY(PR_ERR, pr_nd_alloc(pr, &pr->nodes[*node].as.up.nhs));
    lx_next_token(&pr->lx);
    return pr_call(pr, &pr->nodes[*node].as.up.nhs, pt);
  case TT_LP0:
    ++pr->p0c;
    lx_next_token(&pr->lx);
    return pr_call(pr, node, pt);
  default:
    return PR_ERR_TOKEN_UNEXPECTED;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_next_unop_node(Parser *pr, Node_Index *node, Priority pt) {
  if (pt_includes_tt(pt, pr->lx.tt)) {
    pr->nodes[*node].type = NT_UNOP_NOT * (pr->lx.tt == TT_NOT) +
                            NT_UNOP_NEG * (pr->lx.tt == TT_NEG) +
                            NT_UNOP_NOP * (pr->lx.tt == TT_NOP);

    TRY(PR_ERR, pr_nd_alloc(pr, &pr->nodes[*node].as.up.nhs));

    lx_next_token(&pr->lx);

    node = &pr->nodes[*node].as.up.nhs;
  }

  return pr_call(pr, node, pt);
}

PR_ERR pr_next_biop_node(Parser *pr, Node_Index *lhs, Priority pt) {
  Node_Index bound_low = pr->nodes_len - 1;

  TRY(PR_ERR, pr_call(pr, lhs, pt));

  Node_Index op, rhs;
  Token_Type op_tt;

  while (pt_includes_tt(pt, pr->lx.tt)) {
    op_tt = pr->lx.tt;
    TRY(PR_ERR, pr_nd_alloc(pr, &rhs));

    if (pr->lx.tt != TT_LP0)
      lx_next_token(&pr->lx);
    TRY(PR_ERR, pr_call(pr, &rhs, pt + pt_rl_biop(pt)));

    TRY(PR_ERR, pr_nd_alloc(pr, &op));
    pr->nodes[op].type = tt_to_biop_nd(op_tt);
    pr->nodes[op].as.bp.lhs = *lhs;
    pr->nodes[op].as.bp.rhs = rhs;

    if (pr->nodes[op].type == TT_SPZ)
      pr_nd_obj_bound_add(pr, bound_low, pr->nodes_len);

    *lhs = op;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_next_biop_fact_node(Parser *pr, Node_Index *lhs, Priority pt) {
  TRY(PR_ERR, pr_call(pr, lhs, pt));

  Node_Index op, rhs;

  if (pt_includes_tt(pt, pr->lx.tt)) {
    TRY(PR_ERR, pr_nd_alloc(pr, &rhs));
    TRY(PR_ERR, pr_nd_alloc(pr, &op));

    pr->nodes[op].type = NT_BIOP_FAC;
    pr->nodes[op].as.bp.lhs = *lhs;
    pr->nodes[op].as.bp.rhs = rhs;
    pr->nodes[rhs].type = NT_PRIM_CMX;
    pr->nodes[rhs].as.pm.c = pr->lx.pm.c;
    pr->nodes[rhs].rel_err = 0;

    lx_next_token(&pr->lx);

    *lhs = op;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_skip_rp0(Parser *pr, Node_Index *node, Priority pt) {
  TRY(PR_ERR, pr_call(pr, node, pt));

  if (pr->lx.tt == TT_RP0) {
    if (--pr->p0c < 0)
      return PR_ERR_PAREN_NOT_OPENED;

    lx_next_token(&pr->lx);
  } else if (pr->lx.tt == TT_ABS) {
    pr->abs = false;
    lx_next_token(&pr->lx);
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_next_node(Parser *pr, Node_Index *node) {
  lx_next_token(&pr->lx);
  TRY(PR_ERR, pr_call(pr, node, 0));

  if (pr->p0c != 0)
    return PR_ERR_PAREN_NOT_CLOSED;

  return PR_ERR_NOERROR;
}

PR_ERR pr_call(Parser *pr, Node_Index *node, Priority pt) {
  switch (pt) {
  case PT_SKIP_RP0:    return pr_next_biop_node(pr, node, PT_XPC);
  case PT_XPC:         return pr_next_biop_node(pr, node, PT_LET0);
  case PT_LET0:        return pr_next_biop_node(pr, node, PT_SPZ0);
  case PT_LET1:        return pr_next_biop_node(pr, node, PT_LET0);
  case PT_SPZ0:        return pr_next_biop_node(pr, node, PT_TEST);
  case PT_SPZ1:        return pr_next_biop_node(pr, node, PT_SPZ0);
  case PT_TEST:        return pr_next_biop_node(pr, node, PT_ADD_SUB);
  case PT_ADD_SUB:     return pr_next_biop_node(pr, node, PT_MUL_QUO_MOD);
  case PT_MUL_QUO_MOD: return pr_next_unop_node(pr, node, PT_NOT_NOP_NEG);
  case PT_NOT_NOP_NEG: return pr_next_biop_node(pr, node, PT_POW0);
  case PT_POW0:        return pr_next_biop_fact_node(pr, node, PT_FAC);
  case PT_POW1:        return pr_next_unop_node(pr, node, PT_NOT_NOP_NEG);
  case PT_FAC:         return pr_next_biop_node(pr, node, PT_CAL_APX);
  case PT_CAL_APX:     return pr_next_prim_node(pr, node, PT_PRIM);
  case PT_PRIM:        return pr_skip_rp0(pr, node, PT_SKIP_RP0);
  }

  DBG_FATAL("%s: unknown priority: %d\n", __func__, pt);
  return false;
}

//=:interpreter:errors

typedef enum {
  IR_ERR_NOERROR,
  IR_ERR_ILL_NT,
  IR_ERR_NUM_ARG_EXPECTED,
  IR_ERR_DIV_BY_ZERO,
  IR_ERR_NOT_DEFINED_FOR_TYPE,
  IR_ERR_NOT_DEFINED_SYMBOL,
  IR_ERR_NOT_IMPLEMENTED,
  IR_ERR_ALLOC_FAILED,
  IR_ERR_STACK_OVERFLOW,
  IR_ERR_STACK_UNDERFLOW,
  IR_ERR_AST_MEMORY_NOT_ENOUGH,
  IR_ERR_SYM_MEMORY_NOT_ENOUGH,
} IR_ERR;

const char *ir_err_stringify(IR_ERR ir_err) {
  switch (ir_err) {
    STRINGIFY_CASE(IR_ERR_NOERROR)
    STRINGIFY_CASE(IR_ERR_ILL_NT)
    STRINGIFY_CASE(IR_ERR_NUM_ARG_EXPECTED)
    STRINGIFY_CASE(IR_ERR_DIV_BY_ZERO)
    STRINGIFY_CASE(IR_ERR_NOT_DEFINED_FOR_TYPE)
    STRINGIFY_CASE(IR_ERR_NOT_DEFINED_SYMBOL)
    STRINGIFY_CASE(IR_ERR_NOT_IMPLEMENTED)
    STRINGIFY_CASE(IR_ERR_ALLOC_FAILED)
    STRINGIFY_CASE(IR_ERR_STACK_OVERFLOW)
    STRINGIFY_CASE(IR_ERR_STACK_UNDERFLOW)
    STRINGIFY_CASE(IR_ERR_AST_MEMORY_NOT_ENOUGH)
    STRINGIFY_CASE(IR_ERR_SYM_MEMORY_NOT_ENOUGH)
  }

  return STRINGIFY(INVALID_IR_ERR);
}

//=:interpreter:interpreter

typedef struct {
  Node_Index cap;
  Node_Index len;
  Node data[];
} Stack_Node;

IR_ERR st_nd_add(Stack_Node *st, Node nd) {
  if (st->len >= st->cap)
    return IR_ERR_STACK_OVERFLOW;

  st->data[st->len] = nd;
  ++st->len;

  return IR_ERR_NOERROR;
}

IR_ERR st_nd_pop(Stack_Node *st, Node *nd) {
  if (st->len == 0)
    return IR_ERR_STACK_UNDERFLOW;

  --st->len;
  *nd = st->data[st->len];

  return IR_ERR_NOERROR;
}

typedef struct {
  Parser *pr;
  Stack_Node *st;

  Map_Entry *gscope;
  size_t gscope_len;
  size_t gscope_cap;
} Interpreter;

IR_ERR ir_assert_type(Node_Type expected, Node_Type actual) {
  if (expected != actual)
    return IR_ERR_NOT_DEFINED_FOR_TYPE;

  return IR_ERR_NOERROR;
}

IR_ERR ir_st_pop_value(Interpreter *ir, Node *nd) {
  TRY(IR_ERR, st_nd_pop(ir->st, nd));

  if (nd->type == NT_PRIM_SYM && !MAP_GET(ir->gscope, ir->gscope_cap, nd->as.pm.s, nd))
    return IR_ERR_NOT_DEFINED_SYMBOL;

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_test_ncmx(Interpreter *ir, Node_Type op, Node nlhs, Node nrhs) {
  double ra, rb;

  cmx_t lhs = nlhs.as.pm.c;
  cmx_t rhs = nrhs.as.pm.c;

  float lhs_re = nlhs.rel_err;
  float rhs_re = nrhs.rel_err;

  if (cimag(lhs) == 0 && cimag(rhs) == 0) {
    ra = creal(lhs);
    rb = creal(rhs);
  } else if (creal(lhs) == 0 && creal(rhs) == 0) {
    ra = cimag(lhs);
    rb = cimag(rhs);
  } else {
    return IR_ERR_NOT_DEFINED_FOR_TYPE;
  }

  double rt;

  switch (op) {
  case NT_BIOP_GRE: rt = ra > rb; break;
  case NT_BIOP_LES: rt = ra < rb; break;
  case NT_BIOP_EQU: rt = contains_interval(ra, lhs_re, rb, rhs_re); break;
  case NT_BIOP_NEQ: rt = 1 - contains_interval(ra, lhs_re, rb, rhs_re); break;
  default:
    return IR_ERR_ILL_NT;
  }

  return st_nd_add(ir->st, (Node){.type = NT_PRIM_PRB, .as.pm.c = rt, .rel_err = 0});
}

IR_ERR ir_biop_exec_ncmx(Interpreter *ir, Node_Type op, Node nlhs, Node nrhs) {
  cmx_t rt;
  float rt_re = 0;

  cmx_t lhs = nlhs.as.pm.c;
  cmx_t rhs = nrhs.as.pm.c;

  float lhs_re = nlhs.rel_err;
  float rhs_re = nrhs.rel_err;

  switch (op) {
  case NT_BIOP_ADD:
    rt = lhs + rhs;
    rt_re = sqrt(pow(lhs_re * lhs, 2) + pow(rhs_re * rhs, 2)) / fabs(rt);
    break;
  case NT_BIOP_SUB:
    rt = lhs - rhs;
    rt_re = sqrt(pow(lhs_re * lhs, 2) + pow(rhs_re * rhs, 2)) / fabs(rt);
    break;
  case NT_BIOP_APX:
    rt = lhs;
    rt_re = rhs / lhs;
    break;
  case NT_BIOP_MUL:
    rt = lhs * rhs;
    rt_re = sqrt(pow(lhs_re, 2) + pow(rhs_re, 2));
    break;
  case NT_BIOP_POW:
    rt = pow(lhs, rhs);
    rt_re = sqrt(pow(rhs * lhs_re, 2) + pow(log(lhs) * rhs_re, 2));
    break;
  case NT_BIOP_FAC:
    rt = fac_cmx(lhs, rhs);
    rt_re = fabs(lhs_re * lhs * log(lhs)) + rhs_re;
    break;
  case NT_BIOP_QUO:
    if (rhs == 0)
      return IR_ERR_DIV_BY_ZERO;

    rt = lhs / rhs;
    rt_re = sqrt(pow(lhs_re, 2) + pow(rhs_re, 2));
    break;
  case NT_BIOP_MOD:
    if (cimag(lhs) != 0 || cimag(rhs) != 0)
      return IR_ERR_NOT_DEFINED_FOR_TYPE;

    rt = fmod(creal(lhs), creal(rhs));
    rt_re = lhs_re + rhs_re;
    break;
  default:
    return ir_biop_exec_test_ncmx(ir, op, nlhs, nrhs);
  }

  return st_nd_add(ir->st, (Node){.type = NT_PRIM_CMX, .as.pm.c = rt, .rel_err = rt_re});
}

enum {
  BUILTIN_CONST_PI = 2282,
  BUILTIN_CONST_E = 31,
  BUILTIN_SQRT = 12241645,
  BUILTIN_CEIL = 10106845,
  BUILTIN_ROUND = 513997420,
  BUILTIN_FLOOR = 749115808,
  BUILTIN_LN = 2598,
  BUILTIN_EXP = 175263,
  BUILTIN_COS = 186973,
  BUILTIN_SIN = 166125,
  BUILTIN_TAN = 165614,
  BUILTIN_COSH = 9099869,
  BUILTIN_SINH = 9079021,
  BUILTIN_TANH = 9078510,
  BUILTIN_ACOS = 11966299,
  BUILTIN_ASIN = 10632027,
  BUILTIN_ATAN = 10599323,
  BUILTIN_ACOSH = 582391643,
  BUILTIN_ASINH = 581057371,
  BUILTIN_ATANH = 581024667,
};

IR_ERR ir_call_exec_builtin_cmx(Interpreter *ir, sym_t fn, cmx_t arg) {
  cmx_t rt;

  switch (fn) {
  case BUILTIN_SQRT:  rt = sqrt(arg); break;
  case BUILTIN_CEIL:  rt = ceil(creal(arg)) + ceil(cimag(arg)) * I; break;
  case BUILTIN_ROUND: rt = round(creal(arg)) + round(cimag(arg)) * I; break;
  case BUILTIN_FLOOR: rt = floor(creal(arg)) + floor(cimag(arg)) * I; break;
  case BUILTIN_LN:    rt = log(arg); break;
  case BUILTIN_EXP:   rt = exp(arg); break;
  case BUILTIN_COS:   rt = cos(arg); break;
  case BUILTIN_SIN:   rt = sin(arg); break;
  case BUILTIN_TAN:   rt = tan(arg); break;
  case BUILTIN_COSH:  rt = cosh(arg); break;
  case BUILTIN_SINH:  rt = sinh(arg); break;
  case BUILTIN_TANH:  rt = tanh(arg); break;
  case BUILTIN_ACOS:  rt = acos(arg); break;
  case BUILTIN_ASIN:  rt = asin(arg); break;
  case BUILTIN_ATAN:  rt = atan(arg); break;
  case BUILTIN_ACOSH: rt = acosh(arg); break;
  case BUILTIN_ASINH: rt = asinh(arg); break;
  case BUILTIN_ATANH: rt = atanh(arg); break;
  default:
    return IR_ERR_NOT_DEFINED_SYMBOL;
  }

  return st_nd_add(ir->st, (Node){.type = NT_PRIM_CMX, .as.pm.c = rt, .rel_err = 0});
}

IR_ERR ir_exec(Interpreter *ir) {
  Node current, lhs, rhs;
  Node_Index tail_mark, head_mark;

  Node_Index pr_nodes_ptr = 0;

  while (pr_nodes_ptr < ir->pr->nodes_len) {
    current = ir->pr->nodes[pr_nodes_ptr];

    switch (current.type) {
    case NT_PRIM_SYM:
    case NT_PRIM_CMX:
      TRY(IR_ERR, st_nd_add(ir->st, current));
      break;
    case NT_UNOP_NOT:
    case NT_UNOP_NEG:
    case NT_UNOP_ABS:
    case NT_UNOP_NOP:
      tail_mark = pr_nodes_ptr;
      if (pr_nodes_ptr + 1 == ir->pr->nodes_len)
        return IR_ERR_NUM_ARG_EXPECTED;
      ++pr_nodes_ptr;

      while (pr_nodes_ptr + 1 < ir->pr->nodes_len && is_unop(ir->pr->nodes[pr_nodes_ptr].type))
        ++pr_nodes_ptr;

      head_mark = pr_nodes_ptr;

      lhs = ir->pr->nodes[pr_nodes_ptr];

      if (lhs.type == NT_PRIM_SYM && !MAP_GET(ir->gscope, ir->gscope_cap, lhs.as.pm.s, &lhs))
        return IR_ERR_NOT_DEFINED_SYMBOL;

      TRY(IR_ERR, ir_assert_type(NT_PRIM_CMX, lhs.type));

      while (pr_nodes_ptr > tail_mark && pr_nodes_ptr <= head_mark) {
        --pr_nodes_ptr;

        switch (ir->pr->nodes[pr_nodes_ptr].type) {
        case NT_UNOP_NOP: break;
        case NT_UNOP_NOT: lhs.as.pm.c = subfac_cmx(lhs.as.pm.c); break;
        case NT_UNOP_NEG: lhs.as.pm.c = -lhs.as.pm.c; break;
        case NT_UNOP_ABS: lhs.as.pm.c = fabs(lhs.as.pm.c); break;
        default:
          return IR_ERR_ILL_NT;
        }
      }

      pr_nodes_ptr = head_mark;
      st_nd_add(ir->st, lhs);
      break;
    case NT_CALL:
      TRY(IR_ERR, ir_st_pop_value(ir, &rhs));
      TRY(IR_ERR, st_nd_pop(ir->st, &lhs));

      TRY(IR_ERR, ir_assert_type(NT_PRIM_SYM, lhs.type));
      TRY(IR_ERR, ir_assert_type(NT_PRIM_CMX, rhs.type));

      TRY(IR_ERR, ir_call_exec_builtin_cmx(ir, lhs.as.pm.s, rhs.as.pm.c));
      break;
    case NT_BIOP_LET:
      TRY(IR_ERR, ir_st_pop_value(ir, &rhs));
      TRY(IR_ERR, st_nd_pop(ir->st, &lhs));

      TRY(IR_ERR, ir_assert_type(NT_PRIM_SYM, lhs.type));

      if (!MAP_SET(ir->gscope, ir->gscope_cap, lhs.as.pm.s, &rhs))
        return IR_ERR_SYM_MEMORY_NOT_ENOUGH;

      break;
    case NT_BIOP_GRE:
    case NT_BIOP_LES:
    case NT_BIOP_GEQ:
    case NT_BIOP_LEQ:
    case NT_BIOP_EQU:
    case NT_BIOP_NEQ:
    case NT_BIOP_ADD:
    case NT_BIOP_SUB:
    case NT_BIOP_APX:
    case NT_BIOP_MUL:
    case NT_BIOP_QUO:
    case NT_BIOP_MOD:
    case NT_BIOP_POW:
    case NT_BIOP_FAC:
      TRY(IR_ERR, ir_st_pop_value(ir, &rhs));
      TRY(IR_ERR, ir_st_pop_value(ir, &lhs));

      if (lhs.type == NT_PRIM_CMX && rhs.type == NT_PRIM_CMX) {
        TRY(IR_ERR, ir_biop_exec_ncmx(ir, current.type, lhs, rhs));
      } else {
        return IR_ERR_NOT_DEFINED_FOR_TYPE;
      }

      break;
    default:
      return IR_ERR_NOT_IMPLEMENTED;
    }

    ++pr_nodes_ptr;
  }

  if (ir->st->len) {
    TRY(IR_ERR, ir_st_pop_value(ir, &current));
    TRY(IR_ERR, st_nd_add(ir->st, current));
  }

  return IR_ERR_NOERROR;
}

//=:user:repl

_Noreturn void repl(Interpreter *ir) {
  Node_Index source;

#ifdef _READLINE_H_
  using_history();
#endif

  while (true) {
#ifdef _READLINE_H_
    if (ir->pr->lx.rd.page.data != NULL)
      free(ir->pr->lx.rd.page.data);
#endif

    source = 0;
    rd_reset_counters(&ir->pr->lx.rd);
    ir->st->len = 0;
    ir->pr->p0c = 0;
    ir->pr->abs = false;
    ir->pr->nodes_len = 1;

#ifdef _READLINE_H_
    if ((ir->pr->lx.rd.page.data = readline(REPL_PROMPT)) == NULL)
      PFATAL("cannot read line\n");

    ir->pr->lx.rd.page.len = SIZE_MAX;
    if (ir->pr->lx.rd.page.data[0] == '\0')
      continue;

    add_history(ir->pr->lx.rd.page.data);
#else
    printf(REPL_PROMPT);
    fflush(stdout);

    ssize_t line_len =
        getline(&ir->pr->lx.rd.page.data, &ir->pr->lx.rd.page.cap, stdin);
    if (line_len == -1)
      FATAL("cannot read line\n");

    ir->pr->lx.rd.page.len = (size_t)line_len;
#endif

    PR_ERR perr = pr_next_node(ir->pr, &source);
    if (perr != PR_ERR_NOERROR && perr != PR_ERR_PAREN_NOT_CLOSED) {
      ERROR("%zu:%zu: " CLR_INTERNAL "%s" CLR_RESET
            " (%d) [token: " CLR_INTERNAL "%s" CLR_RESET " (%d)]\n",
            ir->pr->lx.rd.row, ir->pr->lx.rd.col, pr_err_stringify(perr), perr,
            tt_stringify(ir->pr->lx.tt), ir->pr->lx.tt);
      rd_skip_line(&ir->pr->lx.rd);
      continue;
    }

    if (ir->pr->lx.tt != TT_EOS) {
      ERROR("%zu:%zu: " CLR_INTERNAL "PR_ERR_UNEXPECTED_EXPRESSION" CLR_RESET
            "\n",
            ir->pr->lx.rd.row, ir->pr->lx.rd.col);
      ERROR(CLR_INF_MSG "consider adding ';' between expressions\n" CLR_RESET);
      continue;
    }

#ifndef NDEBUG
    nd_tree_print(ir->pr->nodes, source, SOURCE_INDENTATION,
                  SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

    for (Node_Index i = 0; i < ir->pr->nodes_len; ++i) {
      DBG_PRINT("ir->pr->nodes[%d] = %s, ", i, nt_stringify(ir->pr->nodes[i].type));
      if (ir->pr->nodes[i].type == NT_PRIM_CMX)
        nd_tree_print_cmx(ir->pr->nodes[i].as.pm.c, ir->pr->nodes[i].rel_err);
      printf("\n");
    }

    IR_ERR ierr = ir_exec(ir);
    if (ierr != IR_ERR_NOERROR) {
      ERROR(CLR_INTERNAL "%s" CLR_RESET " (%d)\n", ir_err_stringify(ierr),
            ierr);
      continue;
    }

    printf(REPL_RESULT_PREFIX);
    if (ir->st->len != 0)
      printf("\n");

    if (ir->st->len != 0) {
      nd_tree_print(ir->st->data, 0, SOURCE_INDENTATION,
                    SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
    }

    printf(REPL_RESULT_SUFFIX);
  }
}

//=:user:main

int main(int argc, char *argv[]) {
  Interpreter ir;

  ir.st = malloc(sizeof(Stack_Node) + NODE_BUF_SIZE * sizeof(Node));
  assert(ir.st != NULL && "allocation failed");

  ir.st->cap = NODE_BUF_SIZE;
  ir.st->len = 0;

  ir.pr = malloc(sizeof(Parser) + NODE_BUF_SIZE * sizeof(Node));
  assert(ir.pr != NULL && "allocation failed");

  ir.gscope_cap = GLOBAL_SCOPE_CAPACITY;
  ir.gscope =
      (Map_Entry *)calloc(ir.gscope_cap, sizeof(Map_Entry) + sizeof(Node));
  assert(ir.pr != NULL && "allocation failed");

  MAP_SET(ir.gscope,
          ir.gscope_cap,
          BUILTIN_CONST_PI,
          (&(Node){
              .type = NT_PRIM_CMX,
              .as.pm.c = M_PI,
              .rel_err = (nextafter((double)M_PI, INFINITY) - M_PI) / M_PI,
          }));
  MAP_SET(ir.gscope,
          ir.gscope_cap,
          BUILTIN_CONST_E,
          (&(Node){
              .type = NT_PRIM_CMX,
              .as.pm.c = M_E,
              .rel_err = (nextafter((double)M_E, INFINITY) - M_E) / M_E,
          }));

  *ir.pr = ((Parser){
      .lx.rd =
          {
              .src = NULL,
              .page =
                  {
                      .data = NULL,
                      .len = 0,
                      .cap = 0,
                  },
          },
      .p0c = 0,
      .abs = false,
      .nodes_len = 1,
      .nodes_cap = NODE_BUF_SIZE,
  });

  if (isatty(STDIN_FILENO) && argc == 1)
    repl(&ir);

  if (argc > 3)
    FATAL("too many arguments\n");

  if (argc == 3 && strcmp(argv[1], "-f") == 0) {
    ir.pr->lx.rd.src = fopen(argv[2], "r");
    if (ir.pr->lx.rd.src == NULL)
      PFATAL("failed to open file");

    ir.pr->lx.rd.page.cap = INTERNAL_READING_BUF_SIZE;
    ir.pr->lx.rd.page.data =
        (char *)malloc(ir.pr->lx.rd.page.cap * sizeof(char));
    assert(ir.pr->lx.rd.page.data != NULL && "allocation failed");
  } else if (argc == 2) {
    ir.pr->lx.rd.page.len = ir.pr->lx.rd.page.cap = strlen(argv[1]);
    ir.pr->lx.rd.page.data = argv[1];
  } else {
    ir.pr->lx.rd.src = stdin;

    ir.pr->lx.rd.page.cap = INTERNAL_READING_BUF_SIZE;
    ir.pr->lx.rd.page.data =
        (char *)malloc(ir.pr->lx.rd.page.cap * sizeof(char));
    assert(ir.pr->lx.rd.page.data != NULL && "allocation failed");
  }

  rd_reset_counters(&ir.pr->lx.rd);

  Node_Index source = 0;

  PR_ERR perr = pr_next_node(ir.pr, &source);
  if (perr != PR_ERR_NOERROR)
    FATAL("%zu:%zu: %s (%d) [token: %s (%d)]\n", ir.pr->lx.rd.row,
          ir.pr->lx.rd.col, pr_err_stringify(perr), perr,
          tt_stringify(ir.pr->lx.tt), ir.pr->lx.tt);

  if (ir.pr->lx.tt != TT_EOS) {
    ERROR("%zu:%zu: " CLR_INTERNAL "PR_ERR_UNEXPECTED_EXPRESSION" CLR_RESET
          "\n",
          ir.pr->lx.rd.row, ir.pr->lx.rd.col);
    ERROR(CLR_INF_MSG "consider adding ';' between expressions\n" CLR_RESET);
    exit(1);
  }

  /* for (Node_Index i = 0; i < ir.pr->nodes_len; ++i) { */
  /*   DBG_PRINT("ir.pr->nodes[%d] = %s, ", i, nt_stringify(ir.pr->nodes[i].type)); */
  /*   if (ir.pr->nodes[i].type == NT_PRIM_CMX) */
  /*     nd_tree_print_cmx(ir.pr->nodes[i].as.pm.c, ir.pr->nodes[i].rel_err); */
  /*   printf("\n"); */
  /* } */

#ifndef NDEBUG
  nd_tree_print(ir.pr->nodes, source, SOURCE_INDENTATION,
                SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

  IR_ERR ierr = ir_exec(&ir);
  if (ierr != IR_ERR_NOERROR)
    FATAL("%s (%d)\n", ir_err_stringify(ierr), ierr);

  printf(REPL_RESULT_PREFIX);
  if (ir.st->len != 0) {
    nd_tree_print(ir.st->data, 0, SOURCE_INDENTATION,
                  SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
  }

  printf(REPL_RESULT_SUFFIX);

  if (ir.pr->lx.rd.src != NULL)
    free(ir.pr->lx.rd.page.data);
  free(ir.pr);

  if (argc == 3)
    fclose(ir.pr->lx.rd.src);

  return EXIT_SUCCESS;
}
