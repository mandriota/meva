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
//     _            _           _
//    (_)          | |         | |
//     _ _ __   ___| |_   _  __| | ___  ___
//    | | '_ \ / __| | | | |/ _` |/ _ \/ __|
//    | | | | | (__| | |_| | (_| |  __/\__ ;
//    |_|_| |_|\___|_|\__,_|\__,_|\___||___/

#include "config.h"

#include "util.h"

#include <assert.h>
#include <stdbool.h> // IWYU pragma: keep
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
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

//=:reader
//                        _
//                       | |
//     _ __ ___  __ _  __| | ___ _ __
//    | '__/ _ \/ _` |/ _` |/ _ \ '__|
//    | | |  __/ (_| | (_| |  __/ |
//    |_|  \___|\__,_|\__,_|\___|_|

typedef struct {
  StringBuffer page;

  FILE *src;

  size_t ptr;
  ssize_t mrk;
  size_t row, col;

  char cc;

  bool eof, eos;
  bool eoi;
  bool prv;
} Reader;

void rd_reset_counters(Reader *rd) {
  rd->ptr = rd->mrk = 0;
  rd->row = rd->col = 0;
  rd->eof = rd->eos = false;
  rd->eoi = false;
  rd->prv = false;
}

void rd_prev(Reader *rd) {
  rd->prv = rd->mrk == -1 || (size_t)rd->mrk != rd->ptr;
}

void rd_next_page(Reader *rd) {
  rd->ptr = 0;
  rd->mrk = -1;

  if (rd->src == NULL) {
    rd->eos = rd->eof = rd->eoi;
    return;
  }

  rd->page.len = fread(rd->page.data, sizeof(char), rd->page.cap, rd->src);
  if (ferror(rd->src))
    PFATAL("cannot read file\n");

  rd->eof = rd->page.len < rd->page.cap;
  if ((rd->eos = !rd->page.len))
    rd->cc = '\0';
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
      rd->cc = '\0';
      return;
    }
    rd_next_page(rd);
    rd->eoi = true;
  }

  rd->cc = rd->page.data[rd->ptr];
}

void rd_skip_whitespaces(Reader *rd) {
  while (is_whitespace(rd->cc))
    rd_next_char(rd);
}

void rd_skip_line(Reader *rd) {
  while (rd->cc != '\0' && rd->cc != '\n')
    rd_next_char(rd);
}

//=:lexer
//     _
//    | |
//    | | _____  _____ _ __
//    | |/ _ \ \/ / _ \ '__|
//    | |  __/>  <  __/ |
//    |_|\___/_/\_\___|_|

//=:lexer:tokens
typedef enum {
  TT_ILL = -1,
  TT_EOS = 0,

  TT_SYM,
  TT_INT,
  TT_FLT,
  TT_CMX,
  TT_FAL,
  TT_TRU,

  TT_LET,

  TT_AND,
  TT_ORR,

  TT_GRE,
  TT_LES,
  TT_GEQ,
  TT_LEQ,
  TT_EQU,
  TT_NEQ,

  TT_ADD,
  TT_SUB,

  TT_MUL,
  TT_QUO,
  TT_MOD,

  TT_POW,

  TT_NOT,
  TT_FAC,

  TT_LP0,
  TT_RP0,

  TT_ABS,

  TT_EOX,
} TokenType;

const char *tt_stringify(TokenType tt) {
  switch (tt) {
    STRINGIFY_CASE(TT_ILL)
    STRINGIFY_CASE(TT_EOS)
    STRINGIFY_CASE(TT_SYM)
    STRINGIFY_CASE(TT_INT)
    STRINGIFY_CASE(TT_FLT)
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

//=:lexer:lexer
typedef struct {
  Reader rd;

  TokenType tt;
  Primitive pm;
} Lexer;

int_t lx_read_integer(Lexer *lx, int_t *mnt, int_t *exp) {
  *mnt = 0;
  *exp = 0;

  int_t pow10 = 1;

  bool overflow = false;

  while (is_digit(lx->rd.cc)) {
    if (!overflow) {
      *mnt = *mnt * 10 + lx->rd.cc - '0';
      pow10 *= 10;
      overflow = pow10 > INT_T_MAX / 10;
    } else {
      DBG_PRINT("a part of number was cut due to overflow\n");
      ++*exp;
    }

    rd_next_char(&lx->rd);
  }

  return pow10;
}

void lx_next_token_number(Lexer *lx) {
  lx->tt = TT_ILL;

  int_t decimal_log10, mnt, exp;
  lx_read_integer(lx, &mnt, &exp);

  if (lx->rd.cc == '.') {
    lx->tt = TT_FLT;
    rd_next_char(&lx->rd);

    lx->pm.n_flt = (flt_t)mnt * pow(10, (flt_t)exp);
    decimal_log10 = lx_read_integer(lx, &mnt, &exp);
    lx->pm.n_flt += (flt_t)mnt / decimal_log10;
  } else if (exp != 0) {
    lx->tt = TT_FLT;
    lx->pm.n_flt = (flt_t)mnt * pow(10, (flt_t)exp);
  } else if (lx->rd.cc != 'i' || mnt != 0) {
    lx->tt = TT_INT;
    lx->pm.n_int = mnt;
  }

  if (lx->rd.cc == 'i') {
    if (lx->tt == TT_ILL) {
      lx->pm.n_cmx = I;
    } else if (lx->tt == TT_FLT) {
      lx->pm.n_cmx = lx->pm.n_flt * I;
    } else if (lx->tt == TT_INT)
      lx->pm.n_cmx = lx->pm.n_int * I;

    lx->tt = TT_CMX;
    return;
  }

  rd_prev(&lx->rd);
}

void lx_next_token_symbol(Lexer *lx) {
  lx->tt = TT_SYM;
  lx->pm.n_unt = 0;

  int bit_off = 0;

  while (is_letter(lx->rd.cc) || is_digit(lx->rd.cc)) {
    lx->pm.n_unt |= encode_symbol_c(lx->rd.cc) << bit_off;
    bit_off += 6;
    rd_next_char(&lx->rd);
  }

  rd_prev(&lx->rd);
}

void lx_next_token_factorial(Lexer *lx) {
  lx->tt = TT_FAC;

  for (lx->pm.n_unt = 0; lx->rd.cc == '!'; ++lx->pm.n_unt)
    rd_next_char(&lx->rd);

  if (lx->pm.n_unt == 1 && lx->rd.cc == '=') {
    lx->tt = TT_NEQ;
    return;
  }

  if (lx->pm.n_unt == 1)
    lx->tt = TT_NOT;
  rd_prev(&lx->rd);
}

#define LX_TRY_C(c, token_type)                                                \
  {                                                                            \
    if (lx->rd.cc == c) {                                                      \
      lx->tt = token_type;                                                     \
      return;                                                                  \
    }                                                                          \
  }

#define LX_CONSUME_C_OR_RET_TT(failure_token_type, consumer)                   \
  {                                                                            \
    rd_next_char(&lx->rd);                                                     \
    consumer;                                                                  \
    rd_prev(&lx->rd);                                                          \
    lx->tt = failure_token_type;                                               \
    break;                                                                     \
  }

void lx_next_token(Lexer *lx) {
  rd_next_char(&lx->rd);
  rd_skip_whitespaces(&lx->rd);

  lx->rd.mrk = lx->rd.ptr;
  lx->tt = TT_ILL;

  switch (lx->rd.cc) {
    EXEC_CASE('+', lx->tt = TT_ADD)
    EXEC_CASE('-', lx->tt = TT_SUB)
    EXEC_CASE('*', lx->tt = TT_MUL)
    EXEC_CASE('/', lx->tt = TT_QUO)
    EXEC_CASE('%', lx->tt = TT_MOD)
    EXEC_CASE('^', lx->tt = TT_POW)
    EXEC_CASE('(', lx->tt = TT_LP0)
    EXEC_CASE(')', lx->tt = TT_RP0)
    EXEC_CASE(';', lx->tt = TT_EOX)
    EXEC_CASE('\0', lx->tt = TT_EOS)
    EXEC_CASE('!', lx_next_token_factorial(lx))
    EXEC_CASE('&', LX_CONSUME_C_OR_RET_TT(TT_ILL, LX_TRY_C('&', TT_AND)))
    EXEC_CASE('|', LX_CONSUME_C_OR_RET_TT(TT_ABS, LX_TRY_C('|', TT_ORR)))
    EXEC_CASE('>', LX_CONSUME_C_OR_RET_TT(TT_GRE, LX_TRY_C('=', TT_GEQ)))
    EXEC_CASE('<', LX_CONSUME_C_OR_RET_TT(TT_LES, LX_TRY_C('=', TT_LEQ)))
    EXEC_CASE('=', LX_CONSUME_C_OR_RET_TT(TT_LET, LX_TRY_C('=', TT_EQU)))
    EXEC_CASE('\'', LX_CONSUME_C_OR_RET_TT(TT_ILL, LX_TRY_C('f', TT_FAL)
                                                       LX_TRY_C('t', TT_TRU)))
  default:
    if (is_digit(lx->rd.cc) || lx->rd.cc == '.' || lx->rd.cc == 'i') {
      lx_next_token_number(lx);
    } else if (is_letter(lx->rd.cc))
      lx_next_token_symbol(lx);
  }
}

//=:parser
//     _ __   __ _ _ __ ___  ___ _ __
//    | '_ \ / _` | '__/ __|/ _ \ '__|
//    | |_) | (_| | |  \__ \  __/ |
//    | .__/ \__,_|_|  |___/\___|_|
//    | |
//    |_|

//=:parser:nodes
typedef enum {
  NT_PRIM_SYM = TT_SYM,
  NT_PRIM_INT = TT_INT,
  NT_PRIM_FLT = TT_FLT,
  NT_PRIM_CMX = TT_CMX,
  NT_PRIM_BOL,

  NT_BIOP_LET = TT_LET,

  NT_BIOP_AND = TT_AND,
  NT_BIOP_ORR = TT_ORR,

  NT_BIOP_GRE = TT_GRE,
  NT_BIOP_LES = TT_LES,
  NT_BIOP_GEQ = TT_GEQ,
  NT_BIOP_LEQ = TT_LEQ,
  NT_BIOP_EQU = TT_EQU,
  NT_BIOP_NEQ = TT_NEQ,

  NT_BIOP_ADD = TT_ADD,
  NT_BIOP_SUB = TT_SUB,

  NT_BIOP_MUL = TT_MUL,
  NT_BIOP_QUO = TT_QUO,
  NT_BIOP_MOD = TT_MOD,

  NT_BIOP_POW = TT_POW,

  NT_BIOP_FAC = TT_FAC,

  NT_UNOP_ABS = TT_ABS,

  NT_UNOP_NOT,
  NT_UNOP_NOP,
  NT_UNOP_NEG,

  NT_FUNC,
  NT_CALL,
  NT_CALL_ANON,
} NodeType;

const char *nt_stringify(NodeType nt) {
  switch (nt) {
    STRINGIFY_CASE(NT_PRIM_SYM)
    STRINGIFY_CASE(NT_PRIM_INT)
    STRINGIFY_CASE(NT_PRIM_FLT)
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

typedef uint32_t NodeIndex;

typedef struct Node Node; // IWYU pragma: keep

typedef struct {
  NodeIndex arg;
} UnOp;

typedef struct {
  NodeIndex l_arg, r_arg;
} BiOp;

struct Node {
  NodeType type;

  union {
    Primitive pm;
    UnOp up;
    BiOp bp;
  } as;
};

typedef struct {
  NodeIndex node;
  NodeIndex depth;
} StackEmuEl_nd_tree_print;

void nd_tree_print_cmx(cmx_t cmx) {
  if (creal(cmx) != 0 && cimag(cmx) != 0) {
    printf(CLR_PRIM "%Lf %Lfi\n" CLR_RESET, creal(cmx), cimag(cmx));
  } else if (creal(cmx) == 0 && cimag(cmx) == 0) {
    printf(CLR_PRIM "0\n" CLR_RESET);
  } else if (creal(cmx) != 0) {
    printf(CLR_PRIM "%Lf\n" CLR_RESET, creal(cmx));
  } else if (cimag(cmx) != 0)
    printf(CLR_PRIM "%Lfi\n" CLR_RESET, cimag(cmx));
}

void nd_tree_print(StackEmuEl_nd_tree_print stack_emu[], Node nodes[],
                   NodeIndex node, NodeIndex depth, NodeIndex depth_max) {
  NodeIndex len = 1;

  char dst[48];
  char *ptr;
  int ptr_off;

  NodeIndex node_tmp;

  do {
    while (depth < depth_max) {
      printf("%*s", depth * 2, "");
#ifndef NDEBUG
      printf(CLR_INTERNAL "%s" CLR_RESET " (%d) ",
             nt_stringify(nodes[node].type), nodes[node].type);
#endif

      switch (nodes[node].type) {
      case NT_PRIM_SYM:
        ptr = decode_symbol(dst, &dst[sizeof dst - 1], nodes[node].as.pm.n_unt);
        ptr_off = ptr - dst;
        printf(CLR_PRIM "%.*s" CLR_RESET "\n", ptr_off, dst);
        goto while2_final;
      case NT_PRIM_INT:
        ptr = int_stringify(dst, &dst[sizeof dst - 1], nodes[node].as.pm.n_int);
        ptr_off = &dst[sizeof dst] - ptr;
        printf(CLR_PRIM "%.*s" CLR_RESET "\n", ptr_off, ptr);
        goto while2_final;
      case NT_PRIM_FLT:
        printf(CLR_PRIM "%Lf\n" CLR_RESET, nodes[node].as.pm.n_flt);
        goto while2_final;
      case NT_PRIM_CMX:
        nd_tree_print_cmx(nodes[node].as.pm.n_cmx);
        goto while2_final;
      case NT_PRIM_BOL:
        if (nodes[node].as.pm.n_bol)
          printf(CLR_PRIM "true\n" CLR_RESET);
        else
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
      case NT_BIOP_FAC:
        printf("\n");
        node_tmp = node;
        node = nodes[node_tmp].as.bp.l_arg;
        ++depth;
        stack_emu[len].node = nodes[node_tmp].as.bp.r_arg;
        stack_emu[len].depth = depth;
        ++len;
        continue;
      case NT_UNOP_ABS:
      case NT_UNOP_NOT:
      case NT_UNOP_NEG:
      case NT_UNOP_NOP:
        printf("\n");
        node = nodes[node].as.up.arg;
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
    StackEmuEl_nd_tree_print stack_emu[depth_max - depth + 1];                 \
    nd_tree_print(stack_emu, nodes, node, depth, depth_max);                   \
  }

//=:parser:priority
typedef enum {
  PT_SKIP_RP0,
  PT_LET0,
  PT_LET1,
  PT_ORR,
  PT_AND,
  PT_CMP,
  PT_ADD_SUB,
  PT_MUL_QUO_MOD,
  PT_NOT_NOP_NEG,
  PT_POW0,
  PT_POW1,
  PT_FAC,
  PT_PRIM,
} Priority;

bool pt_includes_tt(Priority pt, TokenType tt) {
  switch (pt) {
  case PT_LET0:
    return tt == TT_LET;
  case PT_ORR:
    return tt == TT_ORR;
  case PT_AND:
    return tt == TT_AND;
  case PT_CMP:
    return tt == TT_GRE || tt == TT_LES || tt == TT_GEQ || tt == TT_LEQ ||
           tt == TT_EQU || tt == TT_NEQ;
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

//=:parser:parser
typedef struct {
  Lexer lx;

  ssize_t p0c;
  bool abs;

  NodeIndex len;
  NodeIndex cap;
  Node nodes[];
} Parser;

NodeIndex pr_nd_alloc(Parser *pr) {
  if (pr->len + 1 >= pr->cap)
    FATAL("not enough memory");

  ++pr->len;
  return pr->len - 1;
}

typedef enum {
  PR_ERR_NOERROR,
  PR_ERR_GENERAL,
  PR_ERR_PAREN_NOT_OPENED,
  PR_ERR_PAREN_NOT_CLOSED,
  PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_END_OF_EXPRESSION_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_RIGHT_PAREN_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_ABSOLUTE_UNEXPECTED,
  PR_ERR_TOKEN_UNEXPECTED,
} PR_ERR;

const char *pr_err_stringify(PR_ERR pr_err) {
  switch (pr_err) {
    STRINGIFY_CASE(PR_ERR_NOERROR)
    STRINGIFY_CASE(PR_ERR_GENERAL)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_OPENED)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_CLOSED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_END_OF_EXPRESSION_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_RIGHT_PAREN_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_ABSOLUTE_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_TOKEN_UNEXPECTED)
  }

  return STRINGIFY(INVALID_PR_ERR);
}

PR_ERR pr_call(Parser *pr, NodeIndex *node, Priority pt);

PR_ERR pr_next_prim_node(Parser *pr, NodeIndex *node, Priority pt) {
  switch (pr->lx.tt) {
  case TT_ILL:
    return PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED;
  case TT_EOS:
    return PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED;
  case TT_SYM:
    pr->nodes[*node].type = NT_PRIM_SYM;
    pr->nodes[*node].as.pm.n_unt = pr->lx.pm.n_unt;
    lx_next_token(&pr->lx);
    break;
  case TT_INT:
    pr->nodes[*node].type = NT_PRIM_INT;
    pr->nodes[*node].as.pm.n_int = pr->lx.pm.n_int;
    lx_next_token(&pr->lx);
    break;
  case TT_FLT:
    pr->nodes[*node].type = NT_PRIM_FLT;
    pr->nodes[*node].as.pm.n_flt = pr->lx.pm.n_flt;
    lx_next_token(&pr->lx);
    break;
  case TT_CMX:
    pr->nodes[*node].type = NT_PRIM_CMX;
    pr->nodes[*node].as.pm.n_cmx = pr->lx.pm.n_cmx;
    lx_next_token(&pr->lx);
    break;
  case TT_TRU:
  case TT_FAL:
    pr->nodes[*node].type = NT_PRIM_BOL;
    pr->nodes[*node].as.pm.n_bol = pr->lx.tt == TT_TRU;
    lx_next_token(&pr->lx);
    break;
  case TT_ABS:
    if (pr->abs)
      return PR_ERR_ARGUMENT_EXPECTED_ABSOLUTE_UNEXPECTED;
    pr->abs = true;
    pr->nodes[*node].type = NT_UNOP_ABS;
    pr->nodes[*node].as.up.arg = pr_nd_alloc(pr);
    lx_next_token(&pr->lx);
    return pr_call(pr, &pr->nodes[*node].as.up.arg, pt);
  case TT_LP0:
    ++pr->p0c;
    lx_next_token(&pr->lx);
    return pr_call(pr, node, pt);
  case TT_RP0:
    return PR_ERR_ARGUMENT_EXPECTED_RIGHT_PAREN_UNEXPECTED;
  case TT_EOX:
    return PR_ERR_ARGUMENT_EXPECTED_END_OF_EXPRESSION_UNEXPECTED;
  default:
    return PR_ERR_TOKEN_UNEXPECTED;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_next_unop_node(Parser *pr, NodeIndex *node, Priority pt) {
  if (pt_includes_tt(pt, pr->lx.tt)) {
    pr->nodes[*node].type = NT_UNOP_NOT * (pr->lx.tt == TT_NOT) +
                            NT_UNOP_NEG * (pr->lx.tt == TT_SUB) +
                            NT_UNOP_NOP * (pr->lx.tt == TT_ADD);

    pr->nodes[*node].as.up.arg = pr_nd_alloc(pr);

    lx_next_token(&pr->lx);

    node = &pr->nodes[*node].as.up.arg;
  }

  return pr_call(pr, node, pt);
}

PR_ERR pr_next_biop_node(Parser *pr, NodeIndex *node, Priority pt) {
  TRY(PR_ERR, pr_call(pr, node, pt));

  NodeIndex node_tmp;

  while (pt_includes_tt(pt, pr->lx.tt)) {
    node_tmp = pr_nd_alloc(pr);
    pr->nodes[node_tmp].type = (NodeType)pr->lx.tt;
    pr->nodes[node_tmp].as.bp.l_arg = *node;
    pr->nodes[node_tmp].as.bp.r_arg = pr_nd_alloc(pr);

    lx_next_token(&pr->lx);
    TRY(PR_ERR,
        pr_call(pr, &pr->nodes[node_tmp].as.bp.r_arg, pt + pt_rl_biop(pt)));

    *node = node_tmp;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_next_biop_fact_node(Parser *pr, NodeIndex *node, Priority pt) {
  TRY(PR_ERR, pr_call(pr, node, pt));

  NodeIndex node_tmp, r_arg;

  if (pt_includes_tt(pt, pr->lx.tt)) {
    node_tmp = pr_nd_alloc(pr);
    pr->nodes[node_tmp].type = NT_BIOP_FAC;
    pr->nodes[node_tmp].as.bp.l_arg = *node;

    r_arg = pr->nodes[node_tmp].as.bp.r_arg = pr_nd_alloc(pr);
    pr->nodes[r_arg].type = NT_PRIM_INT;
    pr->nodes[r_arg].as.pm.n_int = pr->lx.pm.n_int;
    lx_next_token(&pr->lx);

    *node = node_tmp;
  }

  return PR_ERR_NOERROR;
}

PR_ERR pr_skip_rp0(Parser *pr, NodeIndex *node, Priority pt) {
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

PR_ERR pr_next_node(Parser *pr, NodeIndex *node) {
  lx_next_token(&pr->lx);
  TRY(PR_ERR, pr_call(pr, node, 0));

  return PR_ERR_NOERROR;
}

PR_ERR pr_call(Parser *pr, NodeIndex *node, Priority pt) {
  switch (pt) {
  case PT_SKIP_RP0:
    return pr_next_biop_node(pr, node, PT_LET0);
  case PT_LET0:
    return pr_next_biop_node(pr, node, PT_ORR);
  case PT_LET1:
    return pr_next_biop_node(pr, node, PT_LET0);
  case PT_ORR:
    return pr_next_biop_node(pr, node, PT_AND);
  case PT_AND:
    return pr_next_biop_node(pr, node, PT_CMP);
  case PT_CMP:
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

//=:interpreter
//     _       _                           _
//    (_)     | |                         | |
//     _ _ __ | |_ ___ _ __ _ __  _ __ ___| |_ ___ _ __
//    | | '_ \| __/ _ \ '__| '_ \| '__/ _ \ __/ _ \ '__|
//    | | | | | ||  __/ |  | |_) | | |  __/ ||  __/ |
//    |_|_| |_|\__\___|_|  | .__/|_|  \___|\__\___|_|
//                         | |
//                         |_|

typedef struct {
  Parser *pr;
  Node nodes[1];
} Interpreter;

typedef enum {
  IR_ERR_NOERROR,
  IR_ERR_ILL_NT,
  IR_ERR_NUM_ARG_EXPECTED,
  IR_ERR_DIV_BY_ZERO,
  IR_ERR_NOT_DEFINED_FOR_TYPE,
  IR_ERR_NOT_IMPLEMENTED,
} IR_ERR;

const char *ir_err_stringify(IR_ERR ir_err) {
  switch (ir_err) {
    STRINGIFY_CASE(IR_ERR_NOERROR)
    STRINGIFY_CASE(IR_ERR_ILL_NT)
    STRINGIFY_CASE(IR_ERR_NUM_ARG_EXPECTED)
    STRINGIFY_CASE(IR_ERR_DIV_BY_ZERO)
    STRINGIFY_CASE(IR_ERR_NOT_DEFINED_FOR_TYPE)
    STRINGIFY_CASE(IR_ERR_NOT_IMPLEMENTED)
  }

  return STRINGIFY(INVALID_IR_ERR);
}

IR_ERR ir_exec(Interpreter *ir, NodeIndex src);

IR_ERR ir_unop_exec_n_int(Interpreter *ir, NodeType op, Primitive a) {
  ir->nodes[0].type = NT_PRIM_INT;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOP, )
    EXEC_CASE(NT_UNOP_NEG, ir->nodes[0].as.pm.n_int = -a.n_int)
    EXEC_CASE(NT_UNOP_ABS,
              ir->nodes[0].as.pm.n_int = a.n_int < 0 ? -a.n_int : a.n_int)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec_n_flt(Interpreter *ir, NodeType op, Primitive a) {
  ir->nodes[0].type = NT_PRIM_FLT;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOP, )
    EXEC_CASE(NT_UNOP_NEG, ir->nodes[0].as.pm.n_flt = -a.n_flt)
    EXEC_CASE(NT_UNOP_ABS, ir->nodes[0].as.pm.n_flt = fabs(a.n_flt))
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec_n_cmx(Interpreter *ir, NodeType op, Primitive a) {
  ir->nodes[0].type = NT_PRIM_CMX;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOP, )
    EXEC_CASE(NT_UNOP_NEG, ir->nodes[0].as.pm.n_cmx = -a.n_cmx)
  case NT_UNOP_ABS:
    ir->nodes[0].type = NT_PRIM_FLT;
    ir->nodes[0].as.pm.n_flt = cabs(a.n_cmx);
    break;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec_n_bol(Interpreter *ir, NodeType op, Primitive a) {
  ir->nodes[0].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_UNOP_NOT, ir->nodes[0].as.pm.n_bol = !a.n_bol)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_unop_exec(Interpreter *ir, NodeIndex src) {
  TRY(IR_ERR, ir_exec(ir, ir->pr->nodes[src].as.up.arg));
  NodeType node_a_type = ir->nodes[0].type;
  Primitive node_a_value = ir->nodes[0].as.pm;

  switch (node_a_type) {
  case NT_PRIM_BOL:
    return ir_unop_exec_n_bol(ir, ir->pr->nodes[src].type, node_a_value);
  case NT_PRIM_INT:
    return ir_unop_exec_n_int(ir, ir->pr->nodes[src].type, node_a_value);
  case NT_PRIM_FLT:
    return ir_unop_exec_n_flt(ir, ir->pr->nodes[src].type, node_a_value);
  case NT_PRIM_CMX:
    return ir_unop_exec_n_cmx(ir, ir->pr->nodes[src].type, node_a_value);
  default:
    return IR_ERR_NUM_ARG_EXPECTED;
  }
}

IR_ERR ir_biop_exec_cmp_n_int(Interpreter *ir, NodeType op, Primitive a,
                              Primitive b) {
  ir->nodes[0].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_BIOP_GRE, ir->nodes[0].as.pm.n_bol = a.n_int > b.n_int)
    EXEC_CASE(NT_BIOP_LES, ir->nodes[0].as.pm.n_bol = a.n_int < b.n_int)
    EXEC_CASE(NT_BIOP_GEQ, ir->nodes[0].as.pm.n_bol = a.n_int >= b.n_int)
    EXEC_CASE(NT_BIOP_LEQ, ir->nodes[0].as.pm.n_bol = a.n_int <= b.n_int)
    EXEC_CASE(NT_BIOP_EQU, ir->nodes[0].as.pm.n_bol = a.n_int == b.n_int)
    EXEC_CASE(NT_BIOP_NEQ, ir->nodes[0].as.pm.n_bol = a.n_int != b.n_int)
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_n_int(Interpreter *ir, NodeType op, Primitive a,
                          Primitive b) {
  ir->nodes[0].type = NT_PRIM_INT;

  switch (op) {
    EXEC_CASE(NT_BIOP_ADD, ir->nodes[0].as.pm.n_int = a.n_int + b.n_int)
    EXEC_CASE(NT_BIOP_SUB, ir->nodes[0].as.pm.n_int = a.n_int - b.n_int)
    EXEC_CASE(NT_BIOP_MUL, ir->nodes[0].as.pm.n_int = a.n_int * b.n_int)
    EXEC_CASE(NT_BIOP_MOD, ir->nodes[0].as.pm.n_int = a.n_int % b.n_int)
    EXEC_CASE(NT_BIOP_FAC, ir->nodes[0].as.pm.n_int = fac_int(a.n_int, b.n_int))
  case NT_BIOP_QUO:
    if (b.n_int == 0)
      return IR_ERR_DIV_BY_ZERO;

    if (a.n_int % b.n_int != 0) {
      ir->nodes[0].type = NT_PRIM_FLT;
      ir->nodes[0].as.pm.n_flt = (flt_t)a.n_int / b.n_int;
    } else
      ir->nodes[0].as.pm.n_int = a.n_int / b.n_int;

    break;
  case NT_BIOP_POW:
    if (b.n_int < 0) {
      ir->nodes[0].type = NT_PRIM_FLT;
      ir->nodes[0].as.pm.n_flt = pow((flt_t)a.n_int, (flt_t)b.n_int);
    } else
      ir->nodes[0].as.pm.n_int = pow_int(a.n_int, b.n_int);

    break;
  default:
    return ir_biop_exec_cmp_n_int(ir, op, a, b);
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_cmp_n_flt(Interpreter *ir, NodeType op, Primitive a,
                              Primitive b) {
  ir->nodes[0].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_BIOP_GRE, ir->nodes[0].as.pm.n_bol = a.n_flt > b.n_flt)
    EXEC_CASE(NT_BIOP_LES, ir->nodes[0].as.pm.n_bol = a.n_flt < b.n_flt)
    EXEC_CASE(NT_BIOP_GEQ,
              ir->nodes[0].as.pm.n_bol =
                  is_almost_equal_flt(a.n_flt, b.n_flt) || a.n_flt > b.n_flt)
    EXEC_CASE(NT_BIOP_LEQ,
              ir->nodes[0].as.pm.n_bol =
                  is_almost_equal_flt(a.n_flt, b.n_flt) || a.n_flt < b.n_flt)
    EXEC_CASE(NT_BIOP_EQU,
              ir->nodes[0].as.pm.n_bol = is_almost_equal_flt(a.n_flt, b.n_flt))
    EXEC_CASE(NT_BIOP_NEQ,
              ir->nodes[0].as.pm.n_bol = !is_almost_equal_flt(a.n_flt, b.n_flt))
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_n_flt(Interpreter *ir, NodeType op, Primitive a,
                          Primitive b) {
  ir->nodes[0].type = NT_PRIM_FLT;

  switch (op) {
    EXEC_CASE(NT_BIOP_ADD, ir->nodes[0].as.pm.n_flt = a.n_flt + b.n_flt)
    EXEC_CASE(NT_BIOP_SUB, ir->nodes[0].as.pm.n_flt = a.n_flt - b.n_flt)
    EXEC_CASE(NT_BIOP_MUL, ir->nodes[0].as.pm.n_flt = a.n_flt * b.n_flt)
    EXEC_CASE(NT_BIOP_MOD, ir->nodes[0].as.pm.n_flt = fmod(a.n_flt, b.n_flt))
    EXEC_CASE(NT_BIOP_POW, ir->nodes[0].as.pm.n_flt = pow(a.n_flt, b.n_flt))
    EXEC_CASE(NT_BIOP_FAC, ir->nodes[0].as.pm.n_flt = fac_flt(a.n_flt, b.n_flt))
  case NT_BIOP_QUO:
    if (b.n_flt == 0)
      return IR_ERR_DIV_BY_ZERO;

    ir->nodes[0].as.pm.n_flt = a.n_flt / b.n_flt;
    break;
  default:
    return ir_biop_exec_cmp_n_flt(ir, op, a, b);
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_cmp_n_cmx(Interpreter *ir, NodeType op, Primitive a,
                              Primitive b) {
  ir->nodes[0].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_BIOP_EQU,
              ir->nodes[0].as.pm.n_bol = is_almost_equal_cmx(a.n_cmx, b.n_cmx))
    EXEC_CASE(NT_BIOP_NEQ,
              ir->nodes[0].as.pm.n_bol = !is_almost_equal_cmx(a.n_cmx, b.n_cmx))
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_n_cmx(Interpreter *ir, NodeType op, Primitive a,
                          Primitive b) {
  ir->nodes[0].type = NT_PRIM_CMX;

  switch (op) {
    EXEC_CASE(NT_BIOP_ADD, ir->nodes[0].as.pm.n_cmx = a.n_cmx + b.n_cmx)
    EXEC_CASE(NT_BIOP_SUB, ir->nodes[0].as.pm.n_cmx = a.n_cmx - b.n_cmx)
    EXEC_CASE(NT_BIOP_MUL, ir->nodes[0].as.pm.n_cmx = a.n_cmx * b.n_cmx)
    EXEC_CASE(NT_BIOP_POW, ir->nodes[0].as.pm.n_cmx = pow(a.n_cmx, b.n_cmx))
  case NT_BIOP_MOD:
  case NT_BIOP_FAC:
    return IR_ERR_NOT_DEFINED_FOR_TYPE;
  case NT_BIOP_QUO:
    if (b.n_cmx == 0)
      return IR_ERR_DIV_BY_ZERO;

    ir->nodes[0].as.pm.n_cmx = a.n_cmx / b.n_cmx;
    break;
  default:
    return ir_biop_exec_cmp_n_cmx(ir, op, a, b);
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_biop_exec_n_bol(Interpreter *ir, NodeType op, Primitive a,
                          Primitive b) {
  ir->nodes[0].type = NT_PRIM_BOL;

  switch (op) {
    EXEC_CASE(NT_BIOP_ORR, ir->nodes[0].as.pm.n_bol = a.n_bol || b.n_bol)
    EXEC_CASE(NT_BIOP_AND, ir->nodes[0].as.pm.n_bol = a.n_bol && b.n_bol)
    EXEC_CASE(NT_BIOP_EQU, ir->nodes[0].as.pm.n_bol = a.n_bol == b.n_bol)
    EXEC_CASE(NT_BIOP_NEQ, ir->nodes[0].as.pm.n_bol = a.n_bol != b.n_bol)
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

#define IR_PM_CONVERT(name, from, to, to_type)                                 \
  {                                                                            \
    node_##name##_type = to_type;                                              \
    node_##name##_value.to = node_##name##_value.from;                         \
  }

IR_ERR ir_biop_exec(Interpreter *ir, NodeIndex src) {
  TRY(IR_ERR, ir_exec(ir, ir->pr->nodes[src].as.bp.l_arg));
  NodeType node_a_type = ir->nodes[0].type;
  Primitive node_a_value = ir->nodes[0].as.pm;

  ir_exec(ir, ir->pr->nodes[src].as.bp.r_arg);
  NodeType node_b_type = ir->nodes[0].type;
  Primitive node_b_value = ir->nodes[0].as.pm;

  if (node_a_type == NT_PRIM_FLT && node_b_type == NT_PRIM_INT)
    IR_PM_CONVERT(b, n_int, n_flt, NT_PRIM_FLT)
  else if (node_a_type == NT_PRIM_INT && node_b_type == NT_PRIM_FLT)
    IR_PM_CONVERT(a, n_int, n_flt, NT_PRIM_FLT)
  else if (node_a_type == NT_PRIM_CMX && node_b_type == NT_PRIM_INT)
    IR_PM_CONVERT(b, n_int, n_cmx, NT_PRIM_CMX)
  else if (node_a_type == NT_PRIM_INT && node_b_type == NT_PRIM_CMX)
    IR_PM_CONVERT(a, n_int, n_cmx, NT_PRIM_CMX)
  else if (node_a_type == NT_PRIM_CMX && node_b_type == NT_PRIM_FLT)
    IR_PM_CONVERT(b, n_flt, n_cmx, NT_PRIM_CMX)
  else if (node_a_type == NT_PRIM_FLT && node_b_type == NT_PRIM_CMX)
    IR_PM_CONVERT(a, n_flt, n_cmx, NT_PRIM_CMX)

  if (node_a_type == NT_PRIM_CMX)
    TRY(IR_ERR, ir_biop_exec_n_cmx(ir, ir->pr->nodes[src].type, node_a_value,
                                   node_b_value));

  if (node_a_type == NT_PRIM_FLT)
    TRY(IR_ERR, ir_biop_exec_n_flt(ir, ir->pr->nodes[src].type, node_a_value,
                                   node_b_value));

  if (node_a_type == NT_PRIM_INT)
    TRY(IR_ERR, ir_biop_exec_n_int(ir, ir->pr->nodes[src].type, node_a_value,
                                   node_b_value));

  if (node_a_type == NT_PRIM_BOL || node_b_type == NT_PRIM_BOL) {
    if (node_a_type != NT_PRIM_BOL || node_b_type != NT_PRIM_BOL)
      return IR_ERR_NOT_DEFINED_FOR_TYPE;

    TRY(IR_ERR, ir_biop_exec_n_bol(ir, ir->pr->nodes[src].type, node_a_value,
                                   node_b_value));
  }

  return IR_ERR_NOERROR;
}

IR_ERR ir_exec(Interpreter *ir, NodeIndex src) {
  switch (ir->pr->nodes[src].type) {
  case NT_PRIM_SYM:
  case NT_PRIM_INT:
  case NT_PRIM_FLT:
  case NT_PRIM_CMX:
  case NT_PRIM_BOL:
    ir->nodes[0] = ir->pr->nodes[src];
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
  case NT_BIOP_FAC:
    return ir_biop_exec(ir, src);
  case NT_FUNC:
  case NT_CALL:
  case NT_CALL_ANON:
    return IR_ERR_NOT_IMPLEMENTED;
  }

  return IR_ERR_ILL_NT;
}

//=:user
//     _   _ ___  ___ _ __
//    | | | / __|/ _ \ '__|
//    | |_| \__ \  __/ |
//     \__,_|___/\___|_|

_Noreturn void repl(Interpreter *ir) {
  NodeIndex source = 0;

#ifdef _READLINE_H_
  using_history();
#endif

  while (true) {
#ifdef _READLINE_H_
    if (ir->pr->lx.rd.page.data != NULL)
      free(ir->pr->lx.rd.page.data);
#endif

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

    IR_ERR ierr = ir_exec(ir, source);
    if (ierr != IR_ERR_NOERROR) {
      ERROR(CLR_INTERNAL "%s" CLR_RESET " (%d)\n", ir_err_stringify(ierr),
            ierr);
      continue;
    }

    printf(REPL_RESULT_PREFIX);
    nd_tree_print(ir->nodes, 0, RESULT_INDENTATION,
                  RESULT_INDENTATION + RESULT_MAX_DEPTH);

    printf(REPL_RESULT_SUFFIX);
  }
}

int main(int argc, char *argv[]) {
  Interpreter ir;
  ir.pr = malloc(sizeof(Parser) + NODE_BUF_SIZE * sizeof(Node));
  assert(ir.pr != NULL && "allocation failed");

  ir.pr->lx.rd.src = NULL;
  ir.pr->lx.rd.page.data = NULL;
  ir.pr->lx.rd.page.len = 0;
  ir.pr->lx.rd.page.cap = 0;
  ir.pr->p0c = 0;
  ir.pr->abs = false;
  ir.pr->len = 1;
  ir.pr->cap = NODE_BUF_SIZE;

  if (isatty(STDIN_FILENO) && argc == 1)
    repl(&ir);

  if (argc > 2)
    FATAL("too many arguments\n");

  if (argc == 2) {
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

  NodeIndex source = 0;

  PR_ERR perr = pr_next_node(ir.pr, &source);
  if (perr != PR_ERR_NOERROR)
    FATAL("%zu:%zu: %s (%d) [token: %s (%d)]\n", ir.pr->lx.rd.row,
          ir.pr->lx.rd.col, pr_err_stringify(perr), perr,
          tt_stringify(ir.pr->lx.tt), ir.pr->lx.tt);

#ifndef NDEBUG
  nd_tree_print(ir.pr->nodes, source, SOURCE_INDENTATION,
                SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

  IR_ERR ierr = ir_exec(&ir, source);
  if (ierr != IR_ERR_NOERROR)
    FATAL("%s (%d)\n", ir_err_stringify(ierr), ierr);

  printf(PIPE_RESULT_PREFIX);
  nd_tree_print(ir.nodes, 0, RESULT_INDENTATION,
                RESULT_INDENTATION + RESULT_MAX_DEPTH);

  printf(PIPE_RESULT_SUFFIX);

  if (ir.pr->lx.rd.src != NULL)
    free(ir.pr->lx.rd.page.data);
  free(ir.pr);

  return EXIT_SUCCESS;
}
