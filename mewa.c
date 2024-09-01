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
#include "intratypes.h"
#include "util.h"

#include <assert.h>
#include <complex.h>
#include <limits.h>
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
  Primitive pm;
} Lexer;

int_t lx_read_integer(Lexer *lx, int_t *mnt, int_t *exp) {
  *mnt = 0;
  *exp = 0;

  int_t pow10 = 1;

  bool overflow = false;

  while (is_digit(lx->rd.cch) && !overflow) {
    *mnt = *mnt * 10 + lx->rd.cch - '0';
    pow10 *= 10;
    overflow = pow10 > INT_T_MAX / 10;

    rd_next_char(&lx->rd);
  }

  if (!overflow)
    return pow10;

  while (is_digit(lx->rd.cch)) {
    ++*exp;
    rd_next_char(&lx->rd);
  }

  return pow10;
}

void lx_next_token_number(Lexer *lx) {
  lx->tt = TT_ILL;

  int_t decimal_log10, mnt, exp;
  while (lx->rd.cch == '0')
    rd_next_char(&lx->rd);
  int_t integer_log10 = lx_read_integer(lx, &mnt, &exp);

  if (lx->rd.cch == '.') {
    rd_next_char(&lx->rd);

    lx->pm.c = (double)mnt * pow(10, (double)exp);
    decimal_log10 = lx_read_integer(lx, &mnt, &exp);
    if (decimal_log10 == 1 && integer_log10 == 1)
      return;
    lx->pm.c += (double)mnt / decimal_log10;
    lx->tt = TT_CMX;
  } else if (exp != 0) {
    WARNING_INT_TO_CMX("integer overflow");

    lx->tt = TT_CMX;
    lx->pm.c = (double)mnt * pow(10, (double)exp);
  } else {
    lx->tt = TT_INT;
    lx->pm.i = mnt;
  }

  if (lx->rd.cch != 'i') {
    rd_prev(&lx->rd);
    return;
  }

  if (lx->tt == TT_CMX) {
    lx->pm.c = lx->pm.c * I;
    return;
  }

  lx->pm.c = lx->pm.i * I;
  lx->tt = TT_CMX;
}

void lx_next_token_symbol(Lexer *lx) {
  lx->tt = TT_ILL;
  lx->pm.s = 0;

  unsigned bit_off = 0;

  while (is_letter(lx->rd.cch) || is_digit(lx->rd.cch)) {
    lx->pm.s |= encode_symbol_c(lx->rd.cch) << bit_off;
    bit_off += 6;
    if (bit_off > SYM_T_BITSIZE) {
      ERROR("identifier is too long (> %lu)\n", SYM_T_BITSIZE / 6);
      return;
    }
    rd_next_char(&lx->rd);
  }

  rd_prev(&lx->rd);
  lx->tt = TT_SYM;
}

void lx_next_token_factorial(Lexer *lx) {
  lx->tt = TT_FAC;

  for (lx->pm.u = 0; lx->rd.cch == '!'; ++lx->pm.u)
    rd_next_char(&lx->rd);

  if (lx->pm.u == 1 && lx->rd.cch == '=') {
    lx->tt = TT_NEQ;
    return;
  }

  if (lx->pm.u == 1)
    lx->tt = TT_NOT;
  rd_prev(&lx->rd);
}

#define LX_TRY_C(on_success_tt, c, ...)                                        \
  {                                                                            \
    if (lx->rd.cch == c) {                                                     \
      lx->tt = on_success_tt;                                                  \
      __VA_ARGS__;                                                             \
      return;                                                                  \
    }                                                                          \
  }

#define LX_LOOKUP(on_failure_tt, consumer)                                     \
  {                                                                            \
    rd_next_char(&lx->rd);                                                     \
    consumer;                                                                  \
    rd_prev(&lx->rd);                                                          \
    lx->tt = on_failure_tt;                                                    \
    break;                                                                     \
  }

void lx_next_token(Lexer *lx) {
  rd_next_char(&lx->rd);
  rd_skip_whitespaces(&lx->rd);

  lx->rd.mrk = lx->rd.ptr;
  lx->tt = TT_ILL;

  switch (lx->rd.cch) {
    EXEC_CASE('+', lx->tt = TT_ADD)
    EXEC_CASE('-', lx->tt = TT_SUB)
    EXEC_CASE('*', lx->tt = TT_MUL)
    EXEC_CASE('/', lx->tt = TT_QUO)
    EXEC_CASE('%', lx->tt = TT_MOD)
    EXEC_CASE('^', lx->tt = TT_POW)
    EXEC_CASE('(', lx->tt = TT_LP0)
    EXEC_CASE(')', lx->tt = TT_RP0)
    EXEC_CASE(';', lx->tt = TT_XPC)
    EXEC_CASE('\0', lx->tt = TT_EOS)
    EXEC_CASE('!', lx_next_token_factorial(lx))
    EXEC_CASE('&', LX_LOOKUP(TT_ILL, LX_TRY_C(TT_AND, '&', )))
    EXEC_CASE('|', LX_LOOKUP(TT_ABS, LX_TRY_C(TT_ORR, '|', )))
    EXEC_CASE('>', LX_LOOKUP(TT_GRE, LX_TRY_C(TT_GEQ, '=', )))
    EXEC_CASE('<', LX_LOOKUP(TT_LES, LX_TRY_C(TT_LEQ, '=', )))
    EXEC_CASE('=', LX_LOOKUP(TT_LET, LX_TRY_C(TT_EQU, '=', )))
    EXEC_CASE('\'',
              LX_LOOKUP(TT_ILL, LX_TRY_C(TT_FAL, 'f', ) LX_TRY_C(TT_TRU, 't', )
                                    LX_TRY_C(TT_CMX, 'i', lx->pm.c = I)))
  default:
    if (is_digit(lx->rd.cch) || lx->rd.cch == '.') {
      lx_next_token_number(lx);
    } else if (is_letter(lx->rd.cch))
      lx_next_token_symbol(lx);
  }
}

//=:parser:nodes

typedef uint32_t Node_Index;

typedef struct Node Node; // IWYU pragma: keep

typedef struct {
  Node_Index nhs;
} Un_Op;

typedef struct {
  Node_Index lhs, rhs;
} Bi_Op;

struct Node {
  Node_Type type;

  union {
    Primitive pm;
    Un_Op up;
    Bi_Op bp;
  } as;
};

typedef struct {
  Node_Index node;
  Node_Index depth;
} Stack_Emu_El_nd_tree_print;

void nd_tree_print_cmx(cmx_t cmx) {
  if (creal(cmx) != 0 && cimag(cmx) != 0) {
    printf(CLR_PRIM "%lf %lfi\n" CLR_RESET, creal(cmx), cimag(cmx));
  } else if (creal(cmx) == 0 && cimag(cmx) == 0) {
    printf(CLR_PRIM "0\n" CLR_RESET);
  } else if (creal(cmx) != 0) {
    printf(CLR_PRIM "%lf\n" CLR_RESET, creal(cmx));
  } else if (cimag(cmx) != 0)
    printf(CLR_PRIM "%lfi\n" CLR_RESET, cimag(cmx));
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
        printf(CLR_PRIM "%.*s" CLR_RESET "\n", ptr_off, dst);
        goto while2_final;
      case NT_PRIM_INT:
        ptr = int_stringify(dst, &dst[sizeof dst - 1], nodes[node].as.pm.i);
        ptr_off = &dst[sizeof dst] - ptr;
        printf(CLR_PRIM "%.*s" CLR_RESET "\n", ptr_off, ptr);
        goto while2_final;
      case NT_PRIM_CMX:
        nd_tree_print_cmx(nodes[node].as.pm.c);
        goto while2_final;
      case NT_PRIM_BOL:
        if (nodes[node].as.pm.b) {
          printf(CLR_PRIM "true\n" CLR_RESET);
        } else
          printf(CLR_PRIM "false\n" CLR_RESET);
        goto while2_final;
      case NT_BIOP_LET:
      case NT_BIOP_AND:
      case NT_BIOP_ORR:
      case NT_BIOP_GRE:
      case NT_BIOP_LES:
      case NT_BIOP_GEQ:
      case NT_BIOP_LEQ:
      case NT_BIOP_EQU:
      case NT_BIOP_NEQ:
      case NT_BIOP_ADD:
      case NT_BIOP_SUB:
      case NT_BIOP_MUL:
      case NT_BIOP_QUO:
      case NT_BIOP_MOD:
      case NT_BIOP_POW:
      case NT_BIOP_XPC:
      case NT_BIOP_FAC:
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
      case NT_FUNC:
      case NT_CALL:
      case NT_CALL_ANON:
        FATAL("currently not implimented\n");
      }
    }

    printf("%*s...\n", depth * 2, "");

  while2_final:
    --len;
    node = stack_emu[len].node;
    depth = stack_emu[len].depth;
  } while (len != 0);
}

#define nd_tree_print(nodes, node, depth, depth_max)                           \
  {                                                                            \
    Stack_Emu_El_nd_tree_print stack_emu[depth_max - depth + 1];               \
    nd_tree_print(stack_emu, nodes, node, depth, depth_max);                   \
  }

//=:parser:priorities

typedef enum {
  PT_SKIP_RP0,
  PT_XPC,
  PT_LET0,
  PT_LET1,
  PT_TEST,
  PT_ORR,
  PT_AND,
  PT_ADD_SUB,
  PT_MUL_QUO_MOD,
  PT_NOT_NOP_NEG,
  PT_POW0,
  PT_POW1,
  PT_FAC,
  PT_PRIM,
} Priority;

bool pt_includes_tt(Priority pt, Token_Type tt) {
  switch (pt) {
  case PT_XPC:
    return tt == TT_XPC;
  case PT_LET0:
    return tt == TT_LET;
  case PT_TEST:
    return tt == TT_GRE || tt == TT_LES || tt == TT_GEQ || tt == TT_LEQ ||
           tt == TT_EQU || tt == TT_NEQ;
  case PT_ORR:
    return tt == TT_ORR;
  case PT_AND:
    return tt == TT_AND;
  case PT_ADD_SUB:
    return tt == TT_ADD || tt == TT_SUB;
  case PT_MUL_QUO_MOD:
    return tt == TT_MUL || tt == TT_QUO || tt == TT_MOD;
  case PT_NOT_NOP_NEG:
    return tt == TT_NOT || tt == TT_SUB || tt == TT_ADD;
  case PT_POW0:
    return tt == TT_POW;
  case PT_FAC:
    return tt == TT_NOT || tt == TT_FAC;
  default:
    DBG_FATAL("%s: unknown priority: %d\n", __func__, pt);
    return false;
  }
}

bool pt_rl_biop(Priority pt) { return pt == PT_LET0 || pt == PT_POW0; }

//=:parser:errors

typedef enum {
  PR_ERR_NOERROR,
  PR_ERR_GENERAL,
  PR_ERR_PAREN_NOT_OPENED,
  PR_ERR_PAREN_NOT_CLOSED,
  PR_ERR_TOKEN_UNEXPECTED,
} PR_ERR;

const char *pr_err_stringify(PR_ERR pr_err) {
  switch (pr_err) {
    STRINGIFY_CASE(PR_ERR_NOERROR)
    STRINGIFY_CASE(PR_ERR_GENERAL)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_OPENED)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_CLOSED)
    STRINGIFY_CASE(PR_ERR_TOKEN_UNEXPECTED)
  }

  return STRINGIFY(INVALID_PR_ERR);
}

//=:parser:parser

typedef struct {
  Lexer lx;

  ssize_t p0c;
  bool abs;

  Node_Index len;
  Node_Index cap;
  Node nodes[];
} Parser;

Node_Index pr_nd_alloc(Parser *pr) {
  if (pr->len + 1 >= pr->cap)
    FATAL("not enough memory");

  ++pr->len;
  return pr->len - 1;
}

PR_ERR pr_call(Parser *pr, Node_Index *node, Priority pt);

PR_ERR pr_next_prim_node(Parser *pr, Node_Index *node, Priority pt) {
  switch (pr->lx.tt) {
  case TT_SYM:
    pr->nodes[*node].type = NT_PRIM_SYM;
    pr->nodes[*node].as.pm.s = pr->lx.pm.s;
    lx_next_token(&pr->lx);
    break;
  case TT_INT:
    pr->nodes[*node].type = NT_PRIM_INT;
    pr->nodes[*node].as.pm.i = pr->lx.pm.i;
    lx_next_token(&pr->lx);
    break;
  case TT_CMX:
    pr->nodes[*node].type = NT_PRIM_CMX;
    pr->nodes[*node].as.pm.c = pr->lx.pm.c;
    lx_next_token(&pr->lx);
    break;
  case TT_TRU:
  case TT_FAL:
    pr->nodes[*node].type = NT_PRIM_BOL;
    pr->nodes[*node].as.pm.b = pr->lx.tt == TT_TRU;
    lx_next_token(&pr->lx);
    break;
  case TT_ABS:
    if (pr->abs)
      return PR_ERR_TOKEN_UNEXPECTED;
    pr->abs = true;
    pr->nodes[*node].type = NT_UNOP_ABS;
    pr->nodes[*node].as.up.nhs = pr_nd_alloc(pr);
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
                            NT_UNOP_NEG * (pr->lx.tt == TT_SUB) +
                            NT_UNOP_NOP * (pr->lx.tt == TT_ADD);

    pr->nodes[*node].as.up.nhs = pr_nd_alloc(pr);

    lx_next_token(&pr->lx);

    node = &pr->nodes[*node].as.up.nhs;
  }

  return pr_call(pr, node, pt);
}

PR_ERR pr_next_biop_node(Parser *pr, Node_Index *node, Priority pt) {
  TRY(PR_ERR, pr_call(pr, node, pt));

  Node_Index node_tmp;

  while (pt_includes_tt(pt, pr->lx.tt)) {
    node_tmp = pr_nd_alloc(pr);
    pr->nodes[node_tmp].type = (Node_Type)pr->lx.tt;
    pr->nodes[node_tmp].as.bp.lhs = *node;
    pr->nodes[node_tmp].as.bp.rhs = pr_nd_alloc(pr);

    lx_next_token(&pr->lx);
    TRY(PR_ERR,
        pr_call(pr, &pr->nodes[node_tmp].as.bp.rhs, pt + pt_rl_biop(pt)));

    *node = node_tmp;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_next_biop_fact_node(Parser *pr, Node_Index *node, Priority pt) {
  TRY(PR_ERR, pr_call(pr, node, pt));

  Node_Index node_tmp, r_arg;

  if (pt_includes_tt(pt, pr->lx.tt)) {
    node_tmp = pr_nd_alloc(pr);
    pr->nodes[node_tmp].type = NT_BIOP_FAC;
    pr->nodes[node_tmp].as.bp.lhs = *node;

    r_arg = pr->nodes[node_tmp].as.bp.rhs = pr_nd_alloc(pr);
    pr->nodes[r_arg].type = NT_PRIM_INT;
    pr->nodes[r_arg].as.pm.i = pr->lx.pm.i;
    lx_next_token(&pr->lx);

    *node = node_tmp;
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

  return PR_ERR_NOERROR;
}

PR_ERR pr_call(Parser *pr, Node_Index *node, Priority pt) {
  switch (pt) {
  case PT_SKIP_RP0:
    return pr_next_biop_node(pr, node, PT_XPC);
  case PT_XPC:
    return pr_next_biop_node(pr, node, PT_LET0);
  case PT_LET0:
    return pr_next_biop_node(pr, node, PT_ORR);
  case PT_LET1:
    return pr_next_biop_node(pr, node, PT_LET0);
  case PT_ORR:
    return pr_next_biop_node(pr, node, PT_AND);
  case PT_AND:
    return pr_next_biop_node(pr, node, PT_TEST);
  case PT_TEST:
    return pr_next_biop_node(pr, node, PT_ADD_SUB);
  case PT_ADD_SUB:
    return pr_next_biop_node(pr, node, PT_MUL_QUO_MOD);
  case PT_MUL_QUO_MOD:
    return pr_next_unop_node(pr, node, PT_NOT_NOP_NEG);
  case PT_NOT_NOP_NEG:
    return pr_next_biop_node(pr, node, PT_POW0);
  case PT_POW0:
    return pr_next_biop_fact_node(pr, node, PT_FAC);
  case PT_POW1:
    return pr_next_unop_node(pr, node, PT_NOT_NOP_NEG);
  case PT_FAC:
    return pr_next_prim_node(pr, node, PT_PRIM);
  case PT_PRIM:
    return pr_skip_rp0(pr, node, PT_SKIP_RP0);
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
  }

  return STRINGIFY(INVALID_IR_ERR);
}

//=:interpreter:interpreter

typedef struct {
  Parser *pr;
  Map_Entry *global;
  size_t global_cap;
  size_t nodes_ptr;
  size_t nodes_cap;
  Node nodes[];
} Interpreter;

IR_ERR ir_exec(Interpreter *ir, Node_Index src, bool sym_exec);

Node_Index ir_nd_alloc(Interpreter *ir) {
  if (ir->nodes_ptr + 1 >= ir->nodes_cap)
    FATAL("not enough memory");

  ++ir->nodes_ptr;
  return ir->nodes_ptr - 1;
}

//=:interpreter:unop

IR_ERR ir_unop_exec_nint(Interpreter *ir, Node_Type op, int_t nhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_INT;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOP, )
    EXEC_CASE(NT_UNOP_NOT, ir->nodes[ir->nodes_ptr].type =
                               subfac_int(&ir->nodes[ir->nodes_ptr].as.pm, nhs))
    EXEC_CASE(NT_UNOP_NEG, ir->nodes[ir->nodes_ptr].as.pm.i = -nhs)
    EXEC_CASE(NT_UNOP_ABS,
              ir->nodes[ir->nodes_ptr].as.pm.i = nhs < 0 ? -nhs : nhs)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec_ncmx(Interpreter *ir, Node_Type op, cmx_t nhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_CMX;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOP, )
    EXEC_CASE(NT_UNOP_NOT, ir->nodes[ir->nodes_ptr].type =
                               subfac_cmx(&ir->nodes[ir->nodes_ptr].as.pm, nhs))
    EXEC_CASE(NT_UNOP_NEG, ir->nodes[ir->nodes_ptr].as.pm.c = -nhs)
  case NT_UNOP_ABS:
    ir->nodes[ir->nodes_ptr].type = NT_PRIM_CMX;
    ir->nodes[ir->nodes_ptr].as.pm.c = fabs(nhs);
    break;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec_bool(Interpreter *ir, Node_Type op, bol_t nhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOT, ir->nodes[ir->nodes_ptr].as.pm.b = !nhs)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec(Interpreter *ir, Node_Index src) {
  TRY(IR_ERR, ir_exec(ir, ir->pr->nodes[src].as.up.nhs, true));
  Node_Type node_a_type = ir->nodes[ir->nodes_ptr].type;
  Primitive node_a_value = ir->nodes[ir->nodes_ptr].as.pm;

  switch (node_a_type) {
  case NT_PRIM_BOL:
    return ir_unop_exec_bool(ir, ir->pr->nodes[src].type, node_a_value.b);
  case NT_PRIM_INT:
    return ir_unop_exec_nint(ir, ir->pr->nodes[src].type, node_a_value.i);
  case NT_PRIM_CMX:
    return ir_unop_exec_ncmx(ir, ir->pr->nodes[src].type, node_a_value.c);
  default:
    return IR_ERR_NUM_ARG_EXPECTED;
  }
}

//=:interpreter:biop

IR_ERR ir_biop_exec_test_nint(Interpreter *ir, Node_Type op, int_t lhs,
                              int_t rhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_BIOP_GRE, ir->nodes[ir->nodes_ptr].as.pm.b = lhs > rhs)
    EXEC_CASE(NT_BIOP_LES, ir->nodes[ir->nodes_ptr].as.pm.b = lhs < rhs)
    EXEC_CASE(NT_BIOP_GEQ, ir->nodes[ir->nodes_ptr].as.pm.b = lhs >= rhs)
    EXEC_CASE(NT_BIOP_LEQ, ir->nodes[ir->nodes_ptr].as.pm.b = lhs <= rhs)
    EXEC_CASE(NT_BIOP_EQU, ir->nodes[ir->nodes_ptr].as.pm.b = lhs == rhs)
    EXEC_CASE(NT_BIOP_NEQ, ir->nodes[ir->nodes_ptr].as.pm.b = lhs != rhs)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_nint(Interpreter *ir, Node_Type op, int_t lhs, int_t rhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_INT;

  switch (op) {
    EXEC_CASE(NT_BIOP_ADD, ir->nodes[ir->nodes_ptr].type = add_int(
                               &ir->nodes[ir->nodes_ptr].as.pm, lhs, rhs))
    EXEC_CASE(NT_BIOP_SUB, ir->nodes[ir->nodes_ptr].type = sub_int(
                               &ir->nodes[ir->nodes_ptr].as.pm, lhs, rhs))
    EXEC_CASE(NT_BIOP_MUL, ir->nodes[ir->nodes_ptr].type = mul_int(
                               &ir->nodes[ir->nodes_ptr].as.pm, lhs, rhs))
    EXEC_CASE(NT_BIOP_FAC, ir->nodes[ir->nodes_ptr].type = fac_int(
                               &ir->nodes[ir->nodes_ptr].as.pm, lhs, rhs))
    EXEC_CASE(NT_BIOP_MOD, ir->nodes[ir->nodes_ptr].as.pm.i = lhs % rhs)
  case NT_BIOP_QUO:
    if (rhs == 0)
      return IR_ERR_DIV_BY_ZERO;

    if (lhs % rhs != 0) {
      ir->nodes[ir->nodes_ptr].type = NT_PRIM_CMX;
      ir->nodes[ir->nodes_ptr].as.pm.c = (double)lhs / rhs;
    } else
      ir->nodes[ir->nodes_ptr].as.pm.i = lhs / rhs;

    break;
  case NT_BIOP_POW:
    if (rhs < 0) {
      ir->nodes[ir->nodes_ptr].type = NT_PRIM_CMX;
      ir->nodes[ir->nodes_ptr].as.pm.c = pow((double)lhs, (double)rhs);
    } else
      ir->nodes[ir->nodes_ptr].type =
          pow_int(&ir->nodes[ir->nodes_ptr].as.pm, lhs, rhs);

    break;
  default:
    return ir_biop_exec_test_nint(ir, op, lhs, rhs);
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_test_ncmx(Interpreter *ir, Node_Type op, cmx_t lhs,
                              cmx_t rhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_BOL;

  double ra, rb;

  if (cimag(lhs) == 0 && cimag(rhs) == 0) {
    ra = creal(lhs);
    rb = creal(rhs);
  } else if (creal(lhs) == 0 && creal(rhs) == 0) {
    ra = cimag(lhs);
    rb = cimag(rhs);
  } else
    return IR_ERR_NOT_DEFINED_FOR_TYPE;

  switch (op) {
    EXEC_CASE(NT_BIOP_GRE, ir->nodes[ir->nodes_ptr].as.pm.b = ra > rb)
    EXEC_CASE(NT_BIOP_LES, ir->nodes[ir->nodes_ptr].as.pm.b = ra < rb)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_ncmx(Interpreter *ir, Node_Type op, cmx_t lhs, cmx_t rhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_CMX;

  switch (op) {
    EXEC_CASE(NT_BIOP_ADD, ir->nodes[ir->nodes_ptr].as.pm.c = lhs + rhs)
    EXEC_CASE(NT_BIOP_SUB, ir->nodes[ir->nodes_ptr].as.pm.c = lhs - rhs)
    EXEC_CASE(NT_BIOP_MUL, ir->nodes[ir->nodes_ptr].as.pm.c = lhs * rhs)
    EXEC_CASE(NT_BIOP_POW, ir->nodes[ir->nodes_ptr].as.pm.c = pow(lhs, rhs))
    EXEC_CASE(NT_BIOP_FAC, ir->nodes[ir->nodes_ptr].type = fac_cmx(
                               &ir->nodes[ir->nodes_ptr].as.pm, lhs, rhs))
  case NT_BIOP_QUO:
    if (rhs == 0)
      return IR_ERR_DIV_BY_ZERO;

    ir->nodes[ir->nodes_ptr].as.pm.c = lhs / rhs;
    break;
  case NT_BIOP_MOD:
    if (cimag(lhs) != 0 || cimag(rhs) != 0)
      return IR_ERR_NOT_DEFINED_FOR_TYPE;

    ir->nodes[ir->nodes_ptr].as.pm.c = fmod(creal(lhs), creal(rhs));
    break;
  default:
    return ir_biop_exec_test_ncmx(ir, op, lhs, rhs);
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_bool(Interpreter *ir, Node_Type op, bol_t lhs, bol_t rhs) {
  ir->nodes[ir->nodes_ptr].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_BIOP_ORR, ir->nodes[ir->nodes_ptr].as.pm.b = lhs || rhs)
    EXEC_CASE(NT_BIOP_AND, ir->nodes[ir->nodes_ptr].as.pm.b = lhs && rhs)
    EXEC_CASE(NT_BIOP_EQU, ir->nodes[ir->nodes_ptr].as.pm.b = lhs == rhs)
    EXEC_CASE(NT_BIOP_NEQ, ir->nodes[ir->nodes_ptr].as.pm.b = lhs != rhs)
  case NT_BIOP_GRE:
  case NT_BIOP_LES:
  case NT_BIOP_GEQ:
  case NT_BIOP_LEQ:
    return IR_ERR_NOT_DEFINED_FOR_TYPE;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec(Interpreter *ir, Node_Index src) {
  TRY(IR_ERR, ir_exec(ir, ir->pr->nodes[src].as.bp.lhs,
                      ir->pr->nodes[src].type != NT_BIOP_LET));
  Node lhs = {.type = ir->nodes[ir->nodes_ptr].type,
              .as.pm = ir->nodes[ir->nodes_ptr].as.pm};

  if (ir->pr->nodes[src].type == NT_BIOP_XPC)
    ir_nd_alloc(ir);

  TRY(IR_ERR, ir_exec(ir, ir->pr->nodes[src].as.bp.rhs, true));
  Node rhs = {.type = ir->nodes[ir->nodes_ptr].type,
              .as.pm = ir->nodes[ir->nodes_ptr].as.pm};

  if (ir->pr->nodes[src].type == NT_BIOP_XPC)
    return IR_ERR_NOERROR;

  if (ir->pr->nodes[src].type == NT_BIOP_LET && lhs.type == NT_PRIM_SYM) {
    MAP_SET(ir->global, ir->global_cap, lhs.as.pm.s, &rhs);
    return IR_ERR_NOERROR;
  }

  if (lhs.type == NT_PRIM_CMX && rhs.type == NT_PRIM_INT) {
    rhs.type = NT_PRIM_CMX;
    rhs.as.pm.c = rhs.as.pm.i;
  } else if (lhs.type == NT_PRIM_INT && rhs.type == NT_PRIM_CMX) {
    lhs.type = NT_PRIM_CMX;
    lhs.as.pm.c = lhs.as.pm.i;
  }

  if (lhs.type == NT_PRIM_CMX)
    return ir_biop_exec_ncmx(ir, ir->pr->nodes[src].type, lhs.as.pm.c,
                             rhs.as.pm.c);

  if (lhs.type == NT_PRIM_INT)
    return ir_biop_exec_nint(ir, ir->pr->nodes[src].type, lhs.as.pm.i,
                             rhs.as.pm.i);

  if (lhs.type == NT_PRIM_BOL || rhs.type == NT_PRIM_BOL) {
    if (lhs.type != rhs.type)
      return IR_ERR_NOT_DEFINED_FOR_TYPE;

    return ir_biop_exec_bool(ir, ir->pr->nodes[src].type, lhs.as.pm.b,
                             rhs.as.pm.b);
  }

  return IR_ERR_NOT_IMPLEMENTED;
}

//=:interpreter:interpreter

IR_ERR ir_exec(Interpreter *ir, Node_Index src, bool sym_exec) {
  switch (ir->pr->nodes[src].type) {
  case NT_PRIM_SYM:
    if (sym_exec) {
      if (!MAP_GET(ir->global, ir->global_cap, ir->pr->nodes[src].as.pm.s,
                   &ir->nodes[ir->nodes_ptr]))
        return IR_ERR_NOT_DEFINED_SYMBOL;
      return IR_ERR_NOERROR;
    }
    // TODO: replace with [[fallthrough]]; (supported only from c23)
    ir->nodes[ir->nodes_ptr] = ir->pr->nodes[src];
    return IR_ERR_NOERROR;
  case NT_PRIM_INT:
  case NT_PRIM_CMX:
  case NT_PRIM_BOL:
    ir->nodes[ir->nodes_ptr] = ir->pr->nodes[src];
    return IR_ERR_NOERROR;
  case NT_UNOP_NOT:
  case NT_UNOP_NOP:
  case NT_UNOP_NEG:
  case NT_UNOP_ABS:
    return ir_unop_exec(ir, src);
  case NT_BIOP_LET:
  case NT_BIOP_AND:
  case NT_BIOP_ORR:
  case NT_BIOP_GRE:
  case NT_BIOP_LES:
  case NT_BIOP_GEQ:
  case NT_BIOP_LEQ:
  case NT_BIOP_EQU:
  case NT_BIOP_NEQ:
  case NT_BIOP_ADD:
  case NT_BIOP_SUB:
  case NT_BIOP_MUL:
  case NT_BIOP_QUO:
  case NT_BIOP_MOD:
  case NT_BIOP_POW:
  case NT_BIOP_XPC:
  case NT_BIOP_FAC:
    return ir_biop_exec(ir, src);
  case NT_FUNC:
  case NT_CALL:
  case NT_CALL_ANON:
    return IR_ERR_NOT_IMPLEMENTED;
  }

  return IR_ERR_ILL_NT;
}

//=:user:repl

_Noreturn void repl(Interpreter *ir) {
  Node_Index source = 0;

#ifdef _READLINE_H_
  using_history();
#endif

  while (true) {
#ifdef _READLINE_H_
    if (ir->pr->lx.rd.page.data != NULL)
      free(ir->pr->lx.rd.page.data);
#endif

    ir->nodes_ptr = 0;
    rd_reset_counters(&ir->pr->lx.rd);
    ir->pr->p0c = 0;
    ir->pr->abs = false;

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
    if (perr != PR_ERR_NOERROR) {
      ERROR("%zu:%zu: " CLR_INTERNAL "%s" CLR_RESET
            " (%d) [token: " CLR_INTERNAL "%s" CLR_RESET " (%d)]\n",
            ir->pr->lx.rd.row, ir->pr->lx.rd.col, pr_err_stringify(perr), perr,
            tt_stringify(ir->pr->lx.tt), ir->pr->lx.tt);
      rd_skip_line(&ir->pr->lx.rd);
      continue;
    }

#ifndef NDEBUG
    nd_tree_print(ir->pr->nodes, source, SOURCE_INDENTATION,
                  SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

    IR_ERR ierr = ir_exec(ir, source, true);
    if (ierr != IR_ERR_NOERROR) {
      ERROR(CLR_INTERNAL "%s" CLR_RESET " (%d)\n", ir_err_stringify(ierr),
            ierr);
      continue;
    }

    for (size_t i = 0; i <= ir->nodes_ptr; ++i) {
      printf(REPL_RESULT_PREFIX);
      nd_tree_print(ir->nodes, i, RESULT_INDENTATION,
                    RESULT_INDENTATION + RESULT_MAX_DEPTH);
    }

    printf(REPL_RESULT_SUFFIX);
  }
}

//=:user:main

int main(int argc, char *argv[]) {
  Interpreter *ir = malloc(sizeof(Interpreter) + NODE_BUF_SIZE * sizeof(Node));
  assert(ir != NULL && "allocation failed");

  ir->nodes_cap = NODE_BUF_SIZE;
  ir->nodes_ptr = 0;

  ir->pr = malloc(sizeof(Parser) + NODE_BUF_SIZE * sizeof(Node));
  assert(ir->pr != NULL && "allocation failed");

  ir->global_cap = GLOBAL_SCOPE_CAPACITY;
  ir->global =
      (Map_Entry *)calloc(ir->global_cap, sizeof(Map_Entry) + sizeof(Node));
  assert(ir->pr != NULL && "allocation failed");

  *ir->pr = ((Parser){
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
      .len = 1,
      .cap = NODE_BUF_SIZE,
  });

  if (isatty(STDIN_FILENO) && argc == 1)
    repl(ir);

  if (argc > 2)
    FATAL("too many arguments\n");

  if (argc == 2) {
    ir->pr->lx.rd.page.len = ir->pr->lx.rd.page.cap = strlen(argv[1]);
    ir->pr->lx.rd.page.data = argv[1];
  } else {
    ir->pr->lx.rd.src = stdin;

    ir->pr->lx.rd.page.cap = INTERNAL_READING_BUF_SIZE;
    ir->pr->lx.rd.page.data =
        (char *)malloc(ir->pr->lx.rd.page.cap * sizeof(char));
    assert(ir->pr->lx.rd.page.data != NULL && "allocation failed");
  }

  rd_reset_counters(&ir->pr->lx.rd);

  Node_Index source = 0;

  PR_ERR perr = pr_next_node(ir->pr, &source);
  if (perr != PR_ERR_NOERROR)
    FATAL("%zu:%zu: %s (%d) [token: %s (%d)]\n", ir->pr->lx.rd.row,
          ir->pr->lx.rd.col, pr_err_stringify(perr), perr,
          tt_stringify(ir->pr->lx.tt), ir->pr->lx.tt);

#ifndef NDEBUG
  nd_tree_print(ir->pr->nodes, source, SOURCE_INDENTATION,
                SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

  IR_ERR ierr = ir_exec(ir, source, true);
  if (ierr != IR_ERR_NOERROR)
    FATAL("%s (%d)\n", ir_err_stringify(ierr), ierr);

  for (size_t i = 0; i <= ir->nodes_ptr; ++i) {
    printf(REPL_RESULT_PREFIX);
    nd_tree_print(ir->nodes, i, RESULT_INDENTATION,
                  RESULT_INDENTATION + RESULT_MAX_DEPTH);
  }

  printf(REPL_RESULT_SUFFIX);

  if (ir->pr->lx.rd.src != NULL)
    free(ir->pr->lx.rd.page.data);
  free(ir->pr);

  return EXIT_SUCCESS;
}
