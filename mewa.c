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
//    | | | | | (__| | |_| | (_| |  __/\__ -
//    |_|_| |_|\___|_|\__,_|\__,_|\___||___/

#include "config.h"

#include "arena.h"

#include "util.h"

#include <math.h>
#include <stdbool.h> // IWYU pragma: keep
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#if INTERNAL_READING_BUF_SIZE < 1
#error INTERNAL_READING_BUF_SIZE must be at least 1
#endif

//=:globals
static struct Arena default_arena = {.head = NULL};

//=:reader
//                        _
//                       | |
//     _ __ ___  __ _  __| | ___ _ __
//    | '__/ _ \/ _` |/ _` |/ _ \ '__|
//    | | |  __/ (_| | (_| |  __/ |
//    |_|  \___|\__,_|\__,_|\___|_|

struct Reader {
  struct StringBuffer page;

  FILE *src;

  size_t ptr;
  ssize_t row;
  ssize_t col;
  ssize_t mrk;

  char cc;

  bool prv;

  bool eof;
  bool eos;
  bool eoi;
};

void rd_reset_counters(struct Reader *rd) {
  rd->ptr = rd->mrk = 0;
  rd->row = rd->col = 0;
  rd->eof = rd->eos = false;
  rd->eoi = false;
}

void rd_prev(struct Reader *rd) {
  rd->prv = rd->mrk == -1 || (size_t)rd->mrk != rd->ptr;
}

void rd_next_page(struct Reader *rd) {
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

void rd_next_char(struct Reader *rd) {
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

void rd_skip_whitespaces(struct Reader *rd) {
  while (IS_WHITESPACE(rd->cc))
    rd_next_char(rd);
}

void rd_skip_line(struct Reader *rd) {
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
enum TokenType {
  TT_ILL = -1,
  TT_EOS = '\0',

  TT_SYM,
  TT_INT,
  TT_FLT,

  TT_LET = '=',

  TT_ADD = '+',
  TT_SUB = '-',

  TT_MUL = '*',
  TT_QUO = '/',
  TT_MOD = '%',

  TT_POW = '^',

  TT_FAC = '!',

  TT_LP0 = '(',
  TT_RP0 = ')',

  TT_EOX = ';',
};

const char *tt_stringify(enum TokenType tt) {
  switch (tt) {
    STRINGIFY_CASE(TT_ILL)
    STRINGIFY_CASE(TT_EOS)
    STRINGIFY_CASE(TT_SYM)
    STRINGIFY_CASE(TT_INT)
    STRINGIFY_CASE(TT_FLT)
    STRINGIFY_CASE(TT_LET)
    STRINGIFY_CASE(TT_ADD)
    STRINGIFY_CASE(TT_SUB)
    STRINGIFY_CASE(TT_MUL)
    STRINGIFY_CASE(TT_QUO)
    STRINGIFY_CASE(TT_MOD)
    STRINGIFY_CASE(TT_POW)
    STRINGIFY_CASE(TT_FAC)
    STRINGIFY_CASE(TT_LP0)
    STRINGIFY_CASE(TT_RP0)
    STRINGIFY_CASE(TT_EOX)
  }

  return STRINGIFY(INVALID_TT);
}

//=:lexer:lexer
struct Lexer {
  struct Reader rd;

  enum TokenType tt;
  union Primitive pm;
};

int_t lx_read_integer(struct Lexer *lx, int_t *mnt, int_t *exp) {
  *mnt = 0;
  *exp = 0;

  int_t pow10 = 1;

  bool overflow = false;

  while (IS_DIGIT(lx->rd.cc)) {
    if (!overflow) {
      *mnt = *mnt * 10 + lx->rd.cc - '0';
      pow10 *= 10;
      overflow = pow10 > INT_T_MAX / 10;
    } else
      ++*exp; // TODO: add warning that right part of integer were ignored

    rd_next_char(&lx->rd);
  }

  return pow10;
}

void lx_next_token_number(struct Lexer *lx) {
  lx->tt = TT_FLT;

  int_t mnt, exp;
  lx_read_integer(lx, &mnt, &exp);

  lx->pm.n_flt = (flt_t)mnt * powl(10, exp);

  if (lx->rd.cc == '.') {
    rd_next_char(&lx->rd);
    int_t decimal_log10 = lx_read_integer(lx, &mnt, &exp);
    lx->pm.n_flt += (flt_t)mnt / decimal_log10;
  } else if (exp == 0) {
    lx->tt = TT_INT;
    lx->pm.n_int = mnt;
  }

  rd_prev(&lx->rd);
}

void lx_next_token_symbol(struct Lexer *lx) {
  lx->tt = TT_SYM;
  lx->pm.n_unt = 0;

  int bit_off = 0;

  while (IS_LETTER(lx->rd.cc) || IS_DIGIT(lx->rd.cc)) {
    lx->pm.n_unt |= encode_symbol_c(lx->rd.cc) << bit_off;
    bit_off += 6;
    rd_next_char(&lx->rd);
  }

  rd_prev(&lx->rd);
}

void lx_next_token_factorial(struct Lexer *lx) {
  lx->tt = TT_FAC;

  for (lx->pm.n_unt = 0; lx->rd.cc == TT_FAC; ++lx->pm.n_unt)
    rd_next_char(&lx->rd);

  rd_prev(&lx->rd);
}

void lx_next_token(struct Lexer *lx) {
  rd_next_char(&lx->rd);
  rd_skip_whitespaces(&lx->rd);

  switch (lx->rd.cc) {
  case TT_LET:
  case TT_ADD:
  case TT_SUB:
  case TT_MUL:
  case TT_QUO:
  case TT_MOD:
  case TT_POW:
  case TT_LP0:
  case TT_RP0:
  case TT_EOX:
  case TT_EOS:
    lx->tt = lx->rd.cc;
    return;
  }

  lx->rd.mrk = lx->rd.ptr;

  if (IS_DIGIT(lx->rd.cc) || lx->rd.cc == '.') {
    lx_next_token_number(lx);
  } else if (IS_LETTER(lx->rd.cc)) {
    lx_next_token_symbol(lx);
  } else if (lx->rd.cc == TT_FAC) {
    lx_next_token_factorial(lx);
  } else
    lx->tt = TT_ILL;
}

//=:parser
//     _ __   __ _ _ __ ___  ___ _ __
//    | '_ \ / _` | '__/ __|/ _ \ '__|
//    | |_) | (_| | |  \__ \  __/ |
//    | .__/ \__,_|_|  |___/\___|_|
//    | |
//    |_|

//=:parser:nodes
enum NodeType {
  NT_PRIM_SYM = TT_SYM,
  NT_PRIM_INT = TT_INT,
  NT_PRIM_FLT = TT_FLT,

  NT_BIOP_LET = TT_LET,

  NT_BIOP_ADD = TT_ADD,
  NT_BIOP_SUB = TT_SUB,

  NT_BIOP_MUL = TT_MUL,
  NT_BIOP_QUO = TT_QUO,
  NT_BIOP_MOD = TT_MOD,

  NT_BIOP_FAC = TT_FAC,

  NT_UNOP_NOP,
  NT_UNOP_NEG,

  NT_BIOP_POW = TT_POW,
};

const char *nt_stringify(enum NodeType nt) {
  switch (nt) {
    STRINGIFY_CASE(NT_PRIM_SYM)
    STRINGIFY_CASE(NT_PRIM_INT)
    STRINGIFY_CASE(NT_PRIM_FLT)
    STRINGIFY_CASE(NT_BIOP_LET)
    STRINGIFY_CASE(NT_BIOP_ADD)
    STRINGIFY_CASE(NT_BIOP_SUB)
    STRINGIFY_CASE(NT_BIOP_MUL)
    STRINGIFY_CASE(NT_BIOP_QUO)
    STRINGIFY_CASE(NT_BIOP_MOD)
    STRINGIFY_CASE(NT_BIOP_FAC)
    STRINGIFY_CASE(NT_UNOP_NOP)
    STRINGIFY_CASE(NT_UNOP_NEG)
    STRINGIFY_CASE(NT_BIOP_POW)
  }

  return STRINGIFY(INVALID_NT);
}

struct Node; // IWYU pragma: keep

struct UnOp {
  struct Node *a;
};

struct BiOp {
  struct Node *a, *b;
};

struct Node {
  enum NodeType type;

  union {
    union Primitive pm;
    struct UnOp up;
    struct BiOp bp;
  } as;
};

void nd_tree_print(struct Node *node, int depth, int depth_max) {
  if (node == NULL || depth >= depth_max)
    return;

  static char dst[48];
  char *str;

  printf("%*s", depth * 2, ""); // indentation
#ifndef NDEBUG
  printf(CLR_INTERNAL "%s" CLR_RESET " (%d) ", nt_stringify(node->type),
         node->type);
#endif

  switch (node->type) {
  case NT_PRIM_SYM:
    str = decode_symbol(dst, &dst[sizeof dst - 1], node->as.pm.n_unt);
    *str = 0;
    printf(CLR_PRIM "%s" CLR_RESET "\n", dst);
    return;
  case NT_PRIM_INT:
    dst[sizeof dst - 1] = 0;

    str = int_stringify(dst, &dst[sizeof dst - 2], node->as.pm.n_int);
    printf(CLR_PRIM "%s" CLR_RESET "\n", str);
    return;
  case NT_PRIM_FLT:
    printf(CLR_PRIM "%Lf\n" CLR_RESET, node->as.pm.n_flt);
    return;
  case NT_BIOP_LET:
  case NT_BIOP_ADD:
  case NT_BIOP_SUB:
  case NT_BIOP_MUL:
  case NT_BIOP_QUO:
  case NT_BIOP_MOD:
  case NT_BIOP_POW:
  case NT_BIOP_FAC:
    printf("\n");
    nd_tree_print(node->as.bp.a, depth + 1, depth_max);
    nd_tree_print(node->as.bp.b, depth + 1, depth_max);
    return;
  case NT_UNOP_NEG:
  case NT_UNOP_NOP:
    printf("\n");
    nd_tree_print(node->as.up.a, depth + 1, depth_max);
    return;
  }
}

//=:parser:parser
struct Parser {
  struct Lexer lx;

  ssize_t p0c;
};

enum PR_ERR {
  PR_ERR_NOERROR,
  PR_ERR_GENERAL,
  PR_ERR_PAREN_NOT_OPENED,
  PR_ERR_PAREN_NOT_CLOSED,
  PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_END_OF_EXPRESSION_UNEXPECTED,
  PR_ERR_ARGUMENT_EXPECTED_RIGHT_PAREN_UNEXPECTED,
  PR_ERR_TOKEN_UNEXPECTED,
};

const char *pr_err_stringify(enum PR_ERR pr_err) {
  switch (pr_err) {
    STRINGIFY_CASE(PR_ERR_NOERROR)
    STRINGIFY_CASE(PR_ERR_GENERAL)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_OPENED)
    STRINGIFY_CASE(PR_ERR_PAREN_NOT_CLOSED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_END_OF_EXPRESSION_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_ARGUMENT_EXPECTED_RIGHT_PAREN_UNEXPECTED)
    STRINGIFY_CASE(PR_ERR_TOKEN_UNEXPECTED)
  }

  return STRINGIFY(INVALID_PR_ERR);
}

enum Priority {
  PT_SKIP_RP0,
  PT_LET0,
  PT_LET1,
  PT_ADD_SUB,
  PT_MUL_QUO_MOD,
  PT_NOP_NEG,
  PT_POW0,
  PT_POW1,
  PT_FAC,
  PT_PRIM,
};

enum PR_ERR pr_rl_unop_next_node(struct Parser *pr, struct Node **node,
                                 enum Priority pt);

enum PR_ERR pr_lr_biop_next_node(struct Parser *pr, struct Node **node,
                                 enum Priority pt);

enum PR_ERR pr_rl_biop_next_node(struct Parser *pr, struct Node **node,
                                 enum Priority pt);

enum PR_ERR pr_skip_rp0(struct Parser *pr, struct Node **node,
                        enum Priority pt);

enum PR_ERR pr_next_primitive_node(struct Parser *pr, struct Node **node,
                                   enum Priority pt);

bool pr_includes_tt(enum TokenType tt, enum Priority pt) {
  switch (pt) {
  case PT_LET0:
  case PT_LET1:
    return tt == TT_LET;
  case PT_ADD_SUB:
    return tt == TT_ADD || tt == TT_SUB;
  case PT_MUL_QUO_MOD:
    return tt == TT_MUL || tt == TT_QUO || tt == TT_MOD;
  case PT_NOP_NEG:
    return tt == TT_SUB || tt == TT_ADD;
  case PT_POW0:
  case PT_POW1:
    return tt == TT_POW;
  case PT_FAC:
    return tt == TT_FAC;
  default:
    DBG_FATAL("%s: unknown priority: %d\n", __func__, pt);
    return false;
  }
}

enum PR_ERR pr_call(struct Parser *pr, struct Node **node, enum Priority pt) {
  switch (pt) {
  case PT_SKIP_RP0:
    return pr_rl_biop_next_node(pr, node, PT_LET0);
  case PT_LET0:
    return pr_lr_biop_next_node(pr, node, PT_ADD_SUB);
  case PT_LET1:
    return pr_rl_biop_next_node(pr, node, PT_LET0);
  case PT_ADD_SUB:
    return pr_lr_biop_next_node(pr, node, PT_MUL_QUO_MOD);
  case PT_MUL_QUO_MOD:
    return pr_rl_unop_next_node(pr, node, PT_NOP_NEG);
  case PT_NOP_NEG:
    return pr_rl_biop_next_node(pr, node, PT_POW0);
  case PT_POW0:
    return pr_lr_biop_next_node(pr, node, PT_FAC);
  case PT_POW1:
    return pr_rl_unop_next_node(pr, node, PT_NOP_NEG);
  case PT_FAC:
    return pr_next_primitive_node(pr, node, PT_PRIM);
  case PT_PRIM:
    return pr_skip_rp0(pr, node, PT_SKIP_RP0);
  }

  DBG_FATAL("%s: unknown priority: %d\n", __func__, pt);
  return false;
}

enum PR_ERR pr_next_primitive_node(struct Parser *pr, struct Node **node,
                                   enum Priority pt) {
  switch (pr->lx.tt) {
  case TT_ILL:
    return PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED;
  case TT_EOS:
    return PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED;
  case TT_SYM:
    (*node)->type = NT_PRIM_SYM;
    (*node)->as.pm.n_unt = pr->lx.pm.n_unt;
    lx_next_token(&pr->lx);
    break;
  case TT_INT:
    DBG_PRINT("setting int node\n");
    (*node)->type = NT_PRIM_INT;
    (*node)->as.pm.n_int = pr->lx.pm.n_int;
    lx_next_token(&pr->lx);

    DBG_PRINT("int node set\n");
    break;
  case TT_FLT:
    (*node)->type = NT_PRIM_FLT;
    (*node)->as.pm.n_flt = pr->lx.pm.n_flt;
    lx_next_token(&pr->lx);
    break;
  case TT_FAC:
    (*node)->type = NT_PRIM_INT;
    (*node)->as.pm.n_int = pr->lx.pm.n_int;
    lx_next_token(&pr->lx);
    break;
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

enum PR_ERR pr_rl_unop_next_node(struct Parser *pr, struct Node **node,
                                 enum Priority pt) {
  struct Node *node_tmp = *node;

  if (pr_includes_tt(pr->lx.tt, pt)) {
    node_tmp->type = NT_UNOP_NEG * (pr->lx.tt == TT_SUB) +
                     NT_UNOP_NOP * (pr->lx.tt == TT_ADD);
    node_tmp->as.up.a =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    node_tmp = node_tmp->as.up.a;
    lx_next_token(&pr->lx);
  }

  return pr_call(pr, &node_tmp, pt);
}

enum PR_ERR pr_lr_biop_next_node(struct Parser *pr, struct Node **node,
                                 enum Priority pt) {
  (*node)->as.bp.a =
      (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));

  DBG_PRINT("%s: pending primitive 1\n", __func__);

  TRY(PR_ERR, pr_call(pr, &(*node)->as.bp.a, pt));

  DBG_PRINT("%s: primitive 1 received\n", __func__);

  struct Node *node_tmp = (*node)->as.bp.a;

  DBG_PRINT("%s: entering loop\n", __func__);

  DBG_PRINT("%s: p0c: %zx\n", __func__, pr->p0c);
  DBG_PRINT("%s: !!!!\n", __func__);
  DBG_PRINT("%s: lx.tt: %s\n", __func__, tt_stringify(pr->lx.tt));
  DBG_PRINT("%s: pr_includes_tt(pr->lx.tt, pt): %d\n", __func__,
            pr_includes_tt(pr->lx.tt, pt));

  while (pr_includes_tt(pr->lx.tt, pt)) { // segfault!
    DBG_PRINT("%s: loop entered\n", __func__);
    (*node)->type = (enum NodeType)pr->lx.tt;
    (*node)->as.bp.b =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    DBG_PRINT("%s: b allocated", __func__);
    if (pr->lx.tt != TT_FAC)
      lx_next_token(&pr->lx);

    DBG_PRINT("%s: pending primitive 2\n", __func__);
    TRY(PR_ERR, pr_call(pr, &(*node)->as.bp.b, pt));
    DBG_PRINT("%s: primitive 2 received\n", __func__);

    node_tmp = *node;
    *node = (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    (*node)->as.bp.a = node_tmp;
  }

  DBG_PRINT("%s: setting node to node_tmp\n", __func__);

  *node = node_tmp;

  DBG_PRINT("%s: node is set to node_tmp\n", __func__);

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_rl_biop_next_node(struct Parser *pr, struct Node **node,
                                 enum Priority pt) {
  (*node)->as.bp.a =
      (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));

  DBG_PRINT("%s: pending primitive 1\n", __func__);

  TRY(PR_ERR, pr_call(pr, &(*node)->as.bp.a, pt));

  DBG_PRINT("%s: primitive 1 received\n", __func__);

  if (pr_includes_tt(pr->lx.tt, pt)) {
    (*node)->as.bp.b =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    (*node)->type = (enum NodeType)pr->lx.tt;

    lx_next_token(&pr->lx);

    TRY(PR_ERR, pr_call(pr, &(*node)->as.bp.b, pt + 1));
  } else {
    DBG_PRINT("%s: setting node\n", __func__);
    **node = *(*node)->as.bp.a; // segfault!
    DBG_PRINT("%s: node set\n", __func__);
  }

  DBG_PRINT("%s: returning\n", __func__);

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_skip_rp0(struct Parser *pr, struct Node **node,
                        enum Priority pt) {
  DBG_PRINT("%s: pending primitive 1\n", __func__);

  TRY(PR_ERR, pr_call(pr, node, pt));

  DBG_PRINT("%s: primitive 1 received\n", __func__);

  if (pr->lx.tt == TT_RP0) {
    if (--pr->p0c < 0)
      return PR_ERR_PAREN_NOT_OPENED;

    lx_next_token(&pr->lx);
  }

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_next_node(struct Parser *pr, struct Node **node) {
  lx_next_token(&pr->lx);
  TRY(PR_ERR, pr_call(pr, node, 00));

  return PR_ERR_NOERROR;
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

struct Interpreter; // IWYU pragma: keep

enum IR_ERR {
  IR_ERR_NOERROR,
  IR_ERR_ILL_NT,
  IR_ERR_INT_OR_FLT_ARG_EXPECTED,
  IR_ERR_DIV_BY_ZERO,
  IR_ERR_OP_NOT_DEFINED_FOR_TYPE,
};

const char *ir_err_stringify(enum IR_ERR ir_err) {
  switch (ir_err) {
    STRINGIFY_CASE(IR_ERR_NOERROR)
    STRINGIFY_CASE(IR_ERR_ILL_NT)
    STRINGIFY_CASE(IR_ERR_INT_OR_FLT_ARG_EXPECTED)
    STRINGIFY_CASE(IR_ERR_DIV_BY_ZERO)
    STRINGIFY_CASE(IR_ERR_OP_NOT_DEFINED_FOR_TYPE)
  }

  return STRINGIFY(INVALID_IR_ERR);
}

enum IR_ERR ir_exec(struct Node **dst, struct Node *src);

enum IR_ERR ir_unop_exec_int(struct Node *dst, enum NodeType op,
                             union Primitive a) {
  dst->type = NT_PRIM_INT;

  switch (op) {
  case NT_UNOP_NOP:
    break;
  case NT_UNOP_NEG:
    dst->as.pm.n_int = -a.n_int;
    break;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

enum IR_ERR ir_unop_exec_flt(struct Node *dst, enum NodeType op,
                             union Primitive a) {
  dst->type = NT_PRIM_FLT;

  switch (op) {
  case NT_UNOP_NOP:
    break;
  case NT_UNOP_NEG:
    dst->as.pm.n_flt = -a.n_flt;
    break;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

enum IR_ERR ir_unop_exec(struct Node **dst, struct Node *src) {
  TRY(IR_ERR, ir_exec(dst, src->as.up.a));
  enum NodeType node_a_type = (*dst)->type;
  union Primitive node_a_value = (*dst)->as.pm;

  if (node_a_type == NT_PRIM_INT)
    return ir_unop_exec_int(*dst, src->type, node_a_value);

  if (node_a_type == NT_PRIM_FLT)
    return ir_unop_exec_flt(*dst, src->type, node_a_value);

  return IR_ERR_INT_OR_FLT_ARG_EXPECTED;
}

enum IR_ERR ir_biop_exec_int(struct Node *dst, enum NodeType op,
                             union Primitive a, union Primitive b) {
  dst->type = NT_PRIM_INT;

  switch (op) {
  case NT_BIOP_ADD:
    dst->as.pm.n_int = a.n_int + b.n_int;
    break;
  case NT_BIOP_SUB:
    dst->as.pm.n_int = a.n_int - b.n_int;
    break;
  case NT_BIOP_MUL:
    dst->as.pm.n_int = a.n_int * b.n_int;
    break;
  case NT_BIOP_QUO:
    if (b.n_int == 0)
      return IR_ERR_DIV_BY_ZERO;

    if (a.n_int % b.n_int != 0) {
      dst->type = NT_PRIM_FLT;
      dst->as.pm.n_flt = (flt_t)a.n_int / b.n_int;
    } else
      dst->as.pm.n_int = a.n_int / b.n_int;

    break;
  case NT_BIOP_MOD:
    dst->as.pm.n_int = a.n_int % b.n_int;
    break;
  case NT_BIOP_POW:
    if (b.n_int < 0) {
      dst->type = NT_PRIM_FLT;
      dst->as.pm.n_flt = powl((flt_t)a.n_int, (flt_t)b.n_int);
    } else
      dst->as.pm.n_int = pow_int(a.n_int, b.n_int);

    break;
  case NT_BIOP_FAC:
    dst->as.pm.n_int = fac_int(a.n_int, b.n_int);
    break;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

enum IR_ERR ir_biop_exec_flt(struct Node *dst, enum NodeType op,
                             union Primitive a, union Primitive b) {
  dst->type = NT_PRIM_FLT;

  switch (op) {
  case NT_BIOP_ADD:
    dst->as.pm.n_flt = a.n_flt + b.n_flt;
    break;
  case NT_BIOP_SUB:
    dst->as.pm.n_flt = a.n_flt - b.n_flt;
    break;
  case NT_BIOP_MUL:
    dst->as.pm.n_flt = a.n_flt * b.n_flt;
    break;
  case NT_BIOP_QUO:
    if (b.n_flt == 0)
      return IR_ERR_DIV_BY_ZERO;

    dst->as.pm.n_flt = a.n_flt / b.n_flt;
    break;
  case NT_BIOP_MOD:
    dst->as.pm.n_flt = fmodl(a.n_flt, b.n_flt);
    break;
  case NT_BIOP_POW:
    dst->as.pm.n_flt = powl(a.n_flt, b.n_flt);
    break;
  case NT_BIOP_FAC:
    dst->as.pm.n_flt = fac_flt(a.n_flt, b.n_flt);
    break;
  default:
    return IR_ERR_ILL_NT;
  }

  return IR_ERR_NOERROR;
}

enum IR_ERR ir_biop_exec(struct Node **dst, struct Node *src) {
  TRY(IR_ERR, ir_exec(dst, src->as.bp.a));
  enum NodeType node_a_type = (*dst)->type;
  union Primitive node_a_value = (*dst)->as.pm;

  ir_exec(dst, src->as.bp.b);
  enum NodeType node_b_type = (*dst)->type;
  union Primitive node_b_value = (*dst)->as.pm;

  if (node_a_type == NT_PRIM_FLT && node_b_type == NT_PRIM_INT) {
    node_b_type = NT_PRIM_FLT;
    node_b_value.n_flt = node_b_value.n_int;
  }

  if (node_a_type == NT_PRIM_INT && node_b_type == NT_PRIM_FLT) {
    node_a_type = NT_PRIM_FLT;
    node_a_value.n_flt = node_a_value.n_int;
  }

  if (node_a_type == NT_PRIM_FLT)
    TRY(IR_ERR, ir_biop_exec_flt(*dst, src->type, node_a_value, node_b_value));

  if (node_a_type == NT_PRIM_INT)
    TRY(IR_ERR, ir_biop_exec_int(*dst, src->type, node_a_value, node_b_value));

  return IR_ERR_NOERROR;
}

enum IR_ERR ir_exec(struct Node **dst, struct Node *src) {
  switch (src->type) {
  case NT_PRIM_SYM:
  case NT_PRIM_INT:
  case NT_PRIM_FLT:
    *dst = src;
    break;
  case NT_UNOP_NOP:
  case NT_UNOP_NEG:
    return ir_unop_exec(dst, src);
  default:
    return ir_biop_exec(dst, src);
  }

  return IR_ERR_NOERROR;
}

//=:user
//     _   _ ___  ___ _ __
//    | | | / __|/ _ \ '__|
//    | |_| \__ \  __/ |
//     \__,_|___/\___|_|

static struct Node ast_source;
static struct Node ast_result;

_Noreturn void repl(struct Parser *pr) {
  struct Node *src, *dst;

#ifdef _READLINE_H_
  using_history();
#endif

  while (true) {
    arena_reset(&default_arena);
#ifdef _READLINE_H_
    if (pr->lx.rd.page.data != NULL)
      free(pr->lx.rd.page.data);
#endif

    rd_reset_counters(&pr->lx.rd);

#ifdef _READLINE_H_
    if ((pr->lx.rd.page.data = readline(REPL_PROMPT)) == NULL)
      PFATAL("cannot read line\n");

    pr->lx.rd.page.len = SIZE_MAX;
    if (pr->lx.rd.page.data[0] == '\0')
      continue;

    add_history(pr->lx.rd.page.data);
#else
    printf(REPL_PROMPT);

    ssize_t line_len =
        getline(&pr->lx.rd.page.data, &pr->lx.rd.page.cap, stdin);
    if (line_len == -1)
      FATAL("cannot read line\n");

    pr->lx.rd.page.len = (size_t)line_len;
#endif

    src = &ast_source;
    dst = &ast_result;

    enum PR_ERR perr = pr_next_node(pr, &src);
    if (perr != PR_ERR_NOERROR) {
      ERROR("%zu:%zu: " CLR_INTERNAL "%s" CLR_RESET
            " (%d) [token: " CLR_INTERNAL "%s" CLR_RESET " (%d)]\n",
            pr->lx.rd.row, pr->lx.rd.col, pr_err_stringify(perr), perr,
            tt_stringify(pr->lx.tt), pr->lx.tt);
      rd_skip_line(&pr->lx.rd);
      continue;
    }

#ifndef NDEBUG
    nd_tree_print(src, SOURCE_INDENTATION,
                  SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

    enum IR_ERR ierr = ir_exec(&dst, src);
    if (ierr != IR_ERR_NOERROR) {
      ERROR(CLR_INTERNAL "%s" CLR_RESET " (%d)\n", ir_err_stringify(ierr),
            ierr);
      continue;
    }

    printf(REPL_RESULT_PREFIX);
    nd_tree_print(dst, RESULT_INDENTATION,
                  RESULT_INDENTATION + RESULT_MAX_DEPTH);

    printf(REPL_RESULT_SUFFIX);
  }
}

int main(int argc, char *argv[]) {
  struct Parser pr = {
      .lx =
          {
              .rd =
                  {
                      .src = NULL,
                      .page =
                          {
                              .data = NULL,
                              .len = 0,
                              .cap = 0,
                          },
                  },
              .tt = 0,
          },
      .p0c = 0,
  };

  struct Node *src = &ast_source;
  struct Node *dst = &ast_result;

  static struct Arena principal_arena = {.head = NULL};

  if (isatty(STDIN_FILENO) && argc == 1)
    repl(&pr);

  if (argc > 2)
    FATAL("too many arguments\n");

  if (argc == 2) {
    pr.lx.rd.page.len = pr.lx.rd.page.cap = strlen(argv[1]);
    pr.lx.rd.page.data = argv[1];
  } else {
    pr.lx.rd.src = stdin;

    pr.lx.rd.page.cap = INTERNAL_READING_BUF_SIZE;
    pr.lx.rd.page.data = arena_acquire(&principal_arena, pr.lx.rd.page.cap);
  }

  rd_reset_counters(&pr.lx.rd);

  enum PR_ERR perr = pr_next_node(&pr, &src);
  if (perr != PR_ERR_NOERROR)
    FATAL("%zu:%zu: %s (%d) [token: %s (%d)]\n", pr.lx.rd.row, pr.lx.rd.col,
          pr_err_stringify(perr), perr, tt_stringify(pr.lx.tt), pr.lx.tt);

#ifndef NDEBUG
  nd_tree_print(src, SOURCE_INDENTATION, SOURCE_INDENTATION + SOURCE_MAX_DEPTH);
#endif

  enum IR_ERR ierr = ir_exec(&dst, src);
  if (ierr != IR_ERR_NOERROR)
    FATAL("%s (%d)\n", ir_err_stringify(ierr), ierr);

  printf(PIPE_RESULT_PREFIX);
  nd_tree_print(dst, RESULT_INDENTATION, RESULT_INDENTATION + RESULT_MAX_DEPTH);

  printf(PIPE_RESULT_SUFFIX);
  fclose(pr.lx.rd.src);

  arena_dealloc(&default_arena);
  arena_dealloc(&principal_arena);

  return EXIT_SUCCESS;
}
