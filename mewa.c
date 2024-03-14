/******************************************************************************\
*                                                                              *
*   Copyright 2024 Mark Mandriota                                              *
*                                                                              *
*   Licensed under the Apache License, Version 2.0 (the "License");            *
*   you may not use this file except in compliance with the License.           *
*   You may obtain a copy of the License at                                    *
*                                                                              *
*       http://www.apache.org/licenses/LICENSE-2.0                             *
*                                                                              *
*   Unless required by applicable law or agreed to in writing, software        *
*   distributed under the License is distributed on an "AS IS" BASIS,          *
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
*   See the License for the specific language governing permissions and        *
*   limitations under the License.                                             *
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
#include "arena.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//=:util
//           _   _ _
//          | | (_) |
//     _   _| |_ _| |
//    | | | | __| | |
//    | |_| | |_| | |
//     \__,_|\__|_|_|

#define IS_WHITESPACE(c)                                                       \
  (c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n')

#define IS_LETTER(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_')

#define IS_DIGIT(c) (c >= '0' && c <= '9')

#define STRINGIFY(name) #name

#define STRINGIFY_CASE(name)                                                   \
  case name:                                                                   \
    return STRINGIFY(name);

#define TRY(prefix, expr)                                                      \
  {                                                                            \
    enum prefix err = expr;                                                    \
    if (err != prefix##_NOERROR)                                               \
      return err;                                                              \
  }

static struct Arena default_arena = {.head = NULL};

struct String {
  char *data;
  size_t len;
};

struct StringBuffer {
  struct String str;
  size_t cap;
};

void sb_push_char(struct StringBuffer *sb, char cc) {
  if (sb->str.len >= sb->cap)
    sb->str.data = arena_reacquire(&default_arena, sb->str.data,
                                   sb->cap * 2 + sb->cap == 0);

  sb->str.data[sb->str.len++] = cc;
}

//=:runtime
//                      _   _
//                     | | (_)
//     _ __ _   _ _ __ | |_ _ _ __ ___   ___
//    | '__| | | | '_ \| __| | '_ ` _ \ / _	\
//    | |  | |_| | | | | |_| | | | | | |  __/
//    |_|   \__,_|_| |_|\__|_|_| |_| |_|\___|

//=:runtime:types
typedef struct String str_t;

typedef long double flt_t;

typedef _BitInt(128) int_t;

typedef unsigned _BitInt(128) unt_t;

union Primitive {
  str_t str;
  flt_t n_flt;
  int_t n_int;
};

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
    expo >>= 1;
    base *= base;
  }
  return rt;
}

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
  size_t row;
  size_t col;
  size_t mrk;
  bool eof;
  bool eos;
};

void rd_prev(struct Reader *restrict rd) {
  if (rd->mrk != rd->ptr)
    --rd->ptr;
}

void rd_next_page(struct Reader *restrict rd) {
  rd->ptr = 0;
  rd->page.str.len = fread(rd->page.str.data, 1, rd->page.cap, rd->src);
  if (ferror(rd->src)) {
    perror("cannot read file");
    exit(2);
  }
  rd->eof = rd->page.str.len < rd->page.cap;
}

void rd_next_char(struct Reader *restrict rd) {
  ++rd->col;
  if (rd->page.str.data[rd->ptr] == '\n') {
    rd->col = 0;
    ++rd->row;
  }
  ++rd->ptr;

  if (rd->ptr >= rd->page.str.len) {
    if (rd->eof) {
      rd->eos = true;
      return;
    }
    rd_next_page(rd);
  }
}

void rd_skip_whitespaces(struct Reader *restrict rd) {
  while (rd->ptr < rd->page.str.len &&
         IS_WHITESPACE(rd->page.str.data[rd->ptr]))
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

  union {
    struct StringBuffer sb;
    union Primitive pv;
  } tk_opt;
};

int_t lx_read_integer(struct Lexer *lx, int_t *mnt, int_t *exp) {
  *mnt = 0;
  *exp = 0;

  int_t pow10 = 1;

  const int_t limit = (((unt_t)1 << (sizeof(pow10) * 8 - 1)) - 1) / 10;

  bool overflow = false;

  char cc;
  while (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
         IS_DIGIT((cc = lx->rd.page.str.data[lx->rd.ptr]))) {
    if (!overflow) {
      *mnt = *mnt * 10 + cc - '0';
      pow10 *= 10;
      overflow = pow10 > limit;
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

  lx->tk_opt.pv.n_flt = (flt_t)mnt * powl(10, exp);

  char cc;
  if (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
      (cc = lx->rd.page.str.data[lx->rd.ptr]) == '.') {

    rd_next_char(&lx->rd);
    int_t decimal_log10 = lx_read_integer(lx, &mnt, &exp);
    lx->tk_opt.pv.n_flt += (flt_t)mnt / decimal_log10;
  } else if (exp == 0) {
    lx->tt = TT_INT;
    lx->tk_opt.pv.n_int = mnt;
  }

  rd_prev(&lx->rd);
}

void lx_read_symbol(struct Lexer *lx) {
  lx->tt = TT_SYM;

  lx->tk_opt.sb.cap = 64;
  lx->tk_opt.sb.str.len = 0;
  lx->tk_opt.sb.str.data =
      (char *)arena_acquire(&default_arena, lx->tk_opt.sb.cap);
  if (lx->tk_opt.sb.str.data == NULL) {
    perror("cannot allocate memory");
    exit(3);
  }

  char cc;
  while (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
         IS_LETTER((cc = lx->rd.page.str.data[lx->rd.ptr]))) {
    sb_push_char(&lx->tk_opt.sb, cc);
    rd_next_char(&lx->rd);
  }

  rd_prev(&lx->rd);
}

void lx_next_token(struct Lexer *lx) {
  if (lx->rd.eos || (lx->rd.eof && lx->rd.page.str.len == 0)) {
    lx->tt = TT_EOS;
    return;
  }

  rd_next_char(&lx->rd);
  rd_skip_whitespaces(&lx->rd);

  char cc = lx->rd.page.str.data[lx->rd.ptr];
  switch (cc) {
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
    lx->tt = cc;
    return;
  }

  lx->rd.ptr = lx->rd.ptr;

  if (IS_DIGIT(cc))
    lx_next_token_number(lx);
  else if (IS_LETTER(cc))
    lx_read_symbol(lx);
  else
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
    STRINGIFY_CASE(NT_UNOP_NEG)
    STRINGIFY_CASE(NT_BIOP_POW)
  }

  return STRINGIFY(INVALID_NT);
}

struct Node;

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

void nd_debug_tree_print(struct Node *node, int depth, int depth_max) {
  if (node == NULL || depth >= depth_max)
    return;

  printf("\n%*s", depth * 2, ""); // indentation
  printf("type: %s (%d); ", nt_stringify(node->type), node->type);

  switch (node->type) {
  case NT_PRIM_SYM:
    printf("value: %*s (len: %zu);", (int)node->as.pm.str.len,
           node->as.pm.str.data, node->as.pm.str.len);
    return;
  case NT_PRIM_INT:
    printf("value: %lld;", (long long)node->as.pm.n_int);
    return;
  case NT_PRIM_FLT:
    printf("value: %Lf;", node->as.pm.n_flt);
    return;
  case NT_BIOP_LET:
  case NT_BIOP_ADD:
  case NT_BIOP_SUB:
  case NT_BIOP_MUL:
  case NT_BIOP_QUO:
  case NT_BIOP_MOD:
  case NT_BIOP_POW:
    nd_debug_tree_print(node->as.bp.a, depth + 1, depth_max);
    nd_debug_tree_print(node->as.bp.b, depth + 1, depth_max);
    return;
  case NT_UNOP_NEG:
    nd_debug_tree_print(node->as.bp.a, depth + 1, depth_max);
    return;
  }
}

//=:parser:parser
struct Parser {
  struct Lexer lx;

  int_t p0c;
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

enum PR_ERR pr_next_pow_node(struct Parser *pr, struct Node **node);

enum PR_ERR pr_next_mul_quo_mod_node(struct Parser *pr, struct Node **node);

enum PR_ERR pr_next_add_sub_node(struct Parser *pr, struct Node **node);

enum PR_ERR pr_next_neg_node(struct Parser *pr, struct Node **node);

enum PR_ERR pr_next_let_node(struct Parser *pr, struct Node **node);

enum PR_ERR pr_next_primitive_node(struct Parser *pr, struct Node **node) {
  switch (pr->lx.tt) {
  case TT_ILL:
    return PR_ERR_ARGUMENT_EXPECTED_ILLEGAL_TOKEN_UNEXPECTED;
  case TT_EOS:
    return PR_ERR_ARGUMENT_EXPECTED_END_OF_STREAM_UNEXPECTED;
  case TT_SYM:
    (*node)->type = NT_PRIM_SYM;
    (*node)->as.pm.str = pr->lx.tk_opt.pv.str;
    lx_next_token(&pr->lx);
    break;
  case TT_INT:
    (*node)->type = NT_PRIM_INT;
    (*node)->as.pm.n_int = pr->lx.tk_opt.pv.n_int;
    lx_next_token(&pr->lx);
    break;
  case TT_FLT:
    (*node)->type = NT_PRIM_FLT;
    (*node)->as.pm.n_flt = pr->lx.tk_opt.pv.n_flt;
    lx_next_token(&pr->lx);
    break;
  case TT_LP0:
    ++pr->p0c;
    lx_next_token(&pr->lx);
    return pr_next_let_node(pr, node);
  case TT_RP0:
    return PR_ERR_ARGUMENT_EXPECTED_RIGHT_PAREN_UNEXPECTED;
  case TT_EOX:
    return PR_ERR_ARGUMENT_EXPECTED_END_OF_EXPRESSION_UNEXPECTED;
  default:
    return PR_ERR_TOKEN_UNEXPECTED;
  }

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_next_pow_node(struct Parser *pr, struct Node **node) {
  (*node)->as.bp.a =
      (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));

  TRY(PR_ERR, pr_next_primitive_node(pr, &(*node)->as.bp.a));

  if (pr->lx.tt == TT_POW) {
    (*node)->as.bp.b =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    (*node)->type = (enum NodeType)pr->lx.tt;

    lx_next_token(&pr->lx);

    TRY(PR_ERR, pr_next_neg_node(pr, &(*node)->as.bp.b));
  } else
    **node = *(*node)->as.bp.a;

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_next_neg_node(struct Parser *pr, struct Node **node) {
  struct Node *node_tmp = *node;

  if (pr->lx.tt == TT_SUB) {
    node_tmp->type = NT_UNOP_NEG;
    node_tmp->as.up.a =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    node_tmp = node_tmp->as.up.a;
    lx_next_token(&pr->lx);
  }

  return pr_next_pow_node(pr, &node_tmp);
}

enum PR_ERR pr_next_mul_quo_mod_node(struct Parser *pr, struct Node **node) {
  (*node)->as.bp.a =
      (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));

  TRY(PR_ERR, pr_next_neg_node(pr, &(*node)->as.bp.a));

  struct Node *node_tmp = (*node)->as.bp.a;

  while (pr->lx.tt == TT_MUL || pr->lx.tt == TT_QUO || pr->lx.tt == TT_MOD) {
    (*node)->type = (enum NodeType)pr->lx.tt;
    (*node)->as.bp.b =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    lx_next_token(&pr->lx);
    TRY(PR_ERR, pr_next_neg_node(pr, &(*node)->as.bp.b));

    node_tmp = *node;
    *node = (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    (*node)->as.bp.a = node_tmp;
  }

  *node = node_tmp;

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_next_add_sub_node(struct Parser *pr, struct Node **node) {
  (*node)->as.bp.a =
      (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));

  TRY(PR_ERR, pr_next_mul_quo_mod_node(pr, &(*node)->as.bp.a));

  struct Node *node_tmp = (*node)->as.bp.a;

  while (pr->lx.tt == TT_ADD || pr->lx.tt == TT_SUB) {
    (*node)->type = (enum NodeType)pr->lx.tt;
    (*node)->as.bp.b =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    lx_next_token(&pr->lx);
    TRY(PR_ERR, pr_next_mul_quo_mod_node(pr, &(*node)->as.bp.b));

    node_tmp = *node;
    *node = (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    (*node)->as.bp.a = node_tmp;
  }

  *node = node_tmp;

  return PR_ERR_NOERROR;
}

enum PR_ERR pr_next_let_node(struct Parser *pr, struct Node **node) {
  (*node)->as.bp.a =
      (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));

  TRY(PR_ERR, pr_next_add_sub_node(pr, &(*node)->as.bp.a));

  if (pr->lx.tt == TT_LET) {
    (*node)->as.bp.b =
        (struct Node *)arena_acquire(&default_arena, sizeof(struct Node));
    (*node)->type = (enum NodeType)pr->lx.tt;

    lx_next_token(&pr->lx);

    TRY(PR_ERR, pr_next_let_node(pr, &(*node)->as.bp.b));
  } else
    **node = *(*node)->as.bp.a;

  if (pr->lx.tt == TT_RP0) {
    if (--pr->p0c >= 0)
      lx_next_token(&pr->lx);
    else
      return PR_ERR_PAREN_NOT_OPENED;
  }

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

struct Interpreter;

enum IR_ERR {
  IR_ERR_NOERROR,
  IR_ERR_ILL_NT,
  IR_ERR_INT_OR_FLT_ARG_EXPECTED,
  IR_ERR_DIV_BY_ZERO,
};

const char *ir_err_stringify(enum IR_ERR ir_err) {
  switch (ir_err) {
    STRINGIFY_CASE(IR_ERR_NOERROR)
    STRINGIFY_CASE(IR_ERR_ILL_NT)
    STRINGIFY_CASE(IR_ERR_INT_OR_FLT_ARG_EXPECTED)
    STRINGIFY_CASE(IR_ERR_DIV_BY_ZERO)
  }

  return STRINGIFY(INVALID_IR_ERR);
}

enum IR_ERR ir_exec(struct Node **dst, struct Node *src);

enum IR_ERR ir_unop_exec_int(struct Node *dst, enum NodeType op,
                             union Primitive a) {
  dst->type = NT_PRIM_INT;

  switch (op) {
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

int repl(void) { return 0; }

int main(void) {
  struct Parser pr = {
      .lx =
          {
              .rd =
                  {
                      .src = stdin,
                      .page =
                          {
                              .str =
                                  {
                                      .data = arena_acquire(&default_arena,
                                                            getpagesize()),
                                      .len = 0,
                                  },
                              .cap = getpagesize(),
                          },
                  },
          },
  };

  struct Node node;

  struct Node *node_p = &node;

  lx_next_token(&pr.lx);
  enum PR_ERR perr = pr_next_let_node(&pr, &node_p);
  if (perr != PR_ERR_NOERROR) {
    printf("%zu:%zu: %s (%d) [token: %s (%d)]\n", pr.lx.rd.row, pr.lx.rd.col,
           pr_err_stringify(perr), perr, tt_stringify(pr.lx.tt), pr.lx.tt);
    exit(1);
  }

  nd_debug_tree_print(node_p, 0, 100);

  puts("");

  struct Node *node_dst = arena_acquire(&default_arena, sizeof(struct Node));

  enum IR_ERR ierr = ir_exec(&node_dst, node_p);
  if (ierr != PR_ERR_NOERROR) {
    printf("\n%s (%d)\n", ir_err_stringify(ierr), ierr);
    exit(1);
  }

  printf("\n\nRESULT: ");
  nd_debug_tree_print(node_dst, 0, 100);

  fclose(pr.lx.rd.src);

  arena_dealloc(&default_arena);
  printf("\n\nmemory deallocated!\n");

  return 0;
}
