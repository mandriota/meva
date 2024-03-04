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
\******************************************************************************/

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/************************************ UTIL ************************************/

#define IS_WHITESPACE(c)                                                       \
  (c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n')

#define IS_LETTER(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_')

#define IS_DIGIT(c) (c >= '0' && c <= '9')

#define STRINGIFY(name) #name

#define STRINGIFY_CASE(name)                                                   \
  case name:                                                                   \
    return STRINGIFY(name);

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
    sb->str.data = realloc(sb->str.data, sb->cap * 2 + sb->cap == 0);

  sb->str.data[sb->str.len++] = cc;
}

/************************************ RUNT ************************************/

union Primitive {
  struct String str;
  long double n_flt;
  int64_t n_int;
};

struct IdentNode {

  u_int64_t v;
};

/************************************ LEXR ************************************/

enum TokenType {
  TT_ERR = -1,
  TT_EOS,

  TT_SYM,
  TT_INT,
  TT_FLT,

  TT_LET,

  TT_ADD,
  TT_SUB,

  TT_MUL,
  TT_QUO,
  TT_MOD,

  TT_POW,

  TT_LP,
  TT_RP,

  TT_EOX,
};

const char *tt_stringify(enum TokenType tt) {
  switch (tt) {
    STRINGIFY_CASE(TT_ERR)
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
    STRINGIFY_CASE(TT_LP)
    STRINGIFY_CASE(TT_RP)
    STRINGIFY_CASE(TT_EOX)
  }

  return STRINGIFY(INVALID_TT);
};

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

struct Lexer {
  struct Reader rd;

  enum TokenType tt;

  union {
    struct StringBuffer sb;
    union Primitive pv;
  } tk_opt;
};

int64_t lx_read_integer(struct Lexer *lx, int64_t *mnt, int64_t *exp) {
  *mnt = 0;
  *exp = 0;

  int64_t pow10 = 1;

  const int64_t limit = (((uint64_t)1 << (sizeof(pow10) * 8 - 1)) - 1) / 10;

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

enum TokenType lx_next_token_number(struct Lexer *lx) {
  enum TokenType tt = TT_FLT;

  int64_t mnt, exp;
  lx_read_integer(lx, &mnt, &exp);

  lx->tk_opt.pv.n_flt = (long double)mnt * powl(10, exp);

  char cc;
  if (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
      (cc = lx->rd.page.str.data[lx->rd.ptr]) == '.') {

    rd_next_char(&lx->rd);
    int64_t decimal_log10 = lx_read_integer(lx, &mnt, &exp);
    lx->tk_opt.pv.n_flt += (long double)mnt / decimal_log10;
  } else if (exp == 0) {
    tt = TT_INT;
    lx->tk_opt.pv.n_int = mnt;
  }

  rd_prev(&lx->rd);

  return tt;
}

void lx_read_symbol(struct Lexer *lx) {
  lx->tk_opt.sb.cap = 64;
  lx->tk_opt.sb.str.len = 0;
  lx->tk_opt.sb.str.data = malloc(lx->tk_opt.sb.cap);

  char cc;
  while (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
         IS_LETTER((cc = lx->rd.page.str.data[lx->rd.ptr]))) {
    sb_push_char(&lx->tk_opt.sb, cc);
    rd_next_char(&lx->rd);
  }

  rd_prev(&lx->rd);
}

enum TokenType lx_next_token(struct Lexer *lx) {
  if (lx->rd.eos || lx->rd.eof && lx->rd.page.str.len == 0)
    return TT_EOS;

  rd_next_char(&lx->rd);
  rd_skip_whitespaces(&lx->rd);

  char cc = lx->rd.page.str.data[lx->rd.ptr];
  switch (cc) {
  case '=':
    return lx->tt = TT_LET;
  case '+':
    return lx->tt = TT_ADD;
  case '-':
    return lx->tt = TT_SUB;
  case '*':
    return lx->tt = TT_MUL;
  case '/':
    return lx->tt = TT_QUO;
  case '%':
    return lx->tt = TT_MOD;
  case '^':
    return lx->tt = TT_POW;
  case '(':
    return lx->tt = TT_LP;
  case ')':
    return lx->tt = TT_RP;
  case ';':
    return lx->tt = TT_EOX;
  }

  lx->rd.ptr = lx->rd.ptr;

  if (IS_DIGIT(cc)) {
    return lx->tt = lx_next_token_number(lx);
  } else if (IS_LETTER(cc)) {
    lx_read_symbol(lx);
    return lx->tt = TT_SYM;
  }

  return lx->tt = TT_ERR;
}

/************************************ PRSR ************************************/

enum NodeType {
  NT_PRIM_SYM,
  NT_PRIM_INT,
  NT_PRIM_FLT,

  NT_BIOP_LET,

  NT_BIOP_ADD,
  NT_BIOP_SUB,

  NT_BIOP_MUL,
  NT_BIOP_QUO,
  NT_BIOP_MOD,

  NT_UNOP_NEG,

  NT_BIOP_POW,
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
};

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
    printf("value: %lld;", node->as.pm.n_int);
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

struct Parser {
  struct Lexer lx;
};

void pr_next_pow_node(struct Parser *pr, struct Node *node);

void pr_next_mul_quo_mod_node(struct Parser *pr, struct Node *node);

void pr_next_primitive_node(struct Parser *pr, struct Node *node) {
  switch (pr->lx.tt) {
  case TT_ERR:
    break;
  case TT_EOS:
    break;
  case TT_SYM:
    node->type = NT_PRIM_SYM;
    node->as.pm.str = pr->lx.tk_opt.pv.str;
    lx_next_token(&pr->lx);
    break;
  case TT_INT:
    node->type = NT_PRIM_INT;
    node->as.pm.n_int = pr->lx.tk_opt.pv.n_int;
    lx_next_token(&pr->lx);
    break;
  case TT_FLT:
    node->type = NT_PRIM_FLT;
    node->as.pm.n_flt = pr->lx.tk_opt.pv.n_flt;
    lx_next_token(&pr->lx);
    break;
  case TT_LP:
    lx_next_token(&pr->lx);
	pr_next_mul_quo_mod_node(pr, node);
    break;
  case TT_RP:
	lx_next_token(&pr->lx);
    break;
  case TT_EOX:
    break;
  default:
	break;
  }
}

void pr_next_pow_node(struct Parser *pr, struct Node *node) {
  struct Node *node_a = (struct Node *)malloc(sizeof(struct Node)), *node_b;

  pr_next_primitive_node(pr, node_a);

  printf("%s\n", tt_stringify(pr->lx.tt));

  if (pr->lx.tt == TT_POW) {
    node_b = (struct Node *)malloc(sizeof(struct Node));
    node->type = NT_BIOP_POW;
    node->as.bp.a = node_a;
    node->as.bp.b = node_b;

    lx_next_token(&pr->lx);

    pr_next_pow_node(pr, node_b); // least priority node
  } else
    *node = *node_a;
};

void pr_next_mul_quo_mod_node(struct Parser *pr, struct Node *node) {
  struct Node *node_a = (struct Node *)malloc(sizeof(struct Node)), *node_b,
              *node_p = node;
  pr_next_pow_node(pr, node_a);

  printf("%s\n", tt_stringify(pr->lx.tt));

  if (pr->lx.tt == TT_MUL || pr->lx.tt == TT_QUO ||
      pr->lx.tt == TT_MOD) {
    node_b = (struct Node *)malloc(sizeof(struct Node));
    node_p->type = NT_BIOP_MUL * (pr->lx.tt == TT_MUL) +
                   NT_BIOP_QUO * (pr->lx.tt == TT_QUO) +
                   NT_BIOP_MOD * (pr->lx.tt == TT_MOD);
    node_p->as.bp.a = node_a;
    node_p->as.bp.b = node_b;

    lx_next_token(&pr->lx);
    pr_next_mul_quo_mod_node(pr, node_b); // least priority node
  } else
    *node = *node_a;

  if (pr->lx.tt == TT_RP) lx_next_token(&pr->lx);
};

/************************************ IRPR ************************************/

struct Interpreter {};

/************************************ USER ************************************/

int main() {
#define FILE_NAME "test.meva"
  FILE *fs = fopen(FILE_NAME, "w");

  fprintf(fs, "(23.33^(3.5*3)*4)*3");

  fclose(fs);

  struct Parser pr = {
      .lx =
          {
              .rd =
                  {
                      .src = fopen(FILE_NAME, "r"),
                      .page =
                          {
                              .str =
                                  {
                                      .data = malloc(getpagesize()),
                                      .len = 0,
                                  },
                              .cap = getpagesize(),
                          },
                  },
          },
  };

  struct Node node;

  lx_next_token(&pr.lx);
  pr_next_mul_quo_mod_node(&pr, &node);
  /* node.type = NT_MUL; */
  /* node.as.bp.a = malloc(sizeof(struct Node)); */
  /* node.as.bp.a->type = NT_PRIM_INT; */
  /* node.as.bp.a->as.pm.n_int = 2; */
  /* node.as.bp.b = malloc(sizeof(struct Node)); */
  /* node.as.bp.b->type = NT_MUL; */
  /* node.as.bp.b->as.bp.a = malloc(sizeof(struct Node)); */
  /* node.as.bp.b->as.bp.a->type = NT_PRIM_INT; */
  /* node.as.bp.b->as.bp.a->as.pm.n_int = 3; */
  /* node.as.bp.b->as.bp.b = malloc(sizeof(struct Node)); */
  /* node.as.bp.b->as.bp.b->type = NT_PRIM_INT; */
  /* node.as.bp.b->as.bp.b->as.pm.n_int = 5; */

  /* printf("%d\n", node.type); */
  /* printf("%d\n", node.as.bp.a->type); */
  /* printf("%d\n", node.as.bp.b->type); */
  /* printf("%d\n", node.as.bp.a->as.bp.a->type); */
  /* printf("%d\n", node.as.bp.a->as.bp.b->type); */

  nd_debug_tree_print(&node, 0, 10);

  fclose(pr.lx.rd.src);
  return 0;
}

/* int main() { */
/* #define FILE_NAME "test.meva" */
/*   FILE *fs = fopen(FILE_NAME, "w"); */

/*   fprintf(fs, "something=e^(pi*i)/12346789987654321123456789"); */

/*   fclose(fs); */

/*   struct Lexer lx = { */
/*       .rd = */
/*           { */
/*               .src = fopen(FILE_NAME, "r"), */
/*               .page = */
/*                   { */
/*                       .str = */
/*                           { */
/*                               .data = malloc(getpagesize()), */
/*                               .len = 0, */
/*                           }, */
/*                       .cap = getpagesize(), */
/*                   }, */
/*           }, */
/*   }; */

/*   enum TokenType tk; */
/*   while (true) { */
/*     tk = lx_next_token(&lx); */
/*     if (tk == TT_EOS) */
/*       return; */

/*     printf("%zu:%zu: token: '%d' (%s)\n", lx.rd.row, lx.rd.col - 1, tk, */
/*            tt_stringify(tk)); */

/*     if (tk == TT_SYM) { */
/*       printf("optional: %*s (len: %zu)\n", (int)lx.tk_opt.sb.str.len, */
/*              lx.tk_opt.sb.str.data, lx.tk_opt.sb.str.len); */
/*     } else if (tk == TT_INT) { */
/*       printf("optional: %lld\n", lx.tk_opt.pv.n_int); */
/*     } else if (tk == TT_FLT) { */
/*       printf("optional: %Lf\n", lx.tk_opt.pv.n_flt); */
/*     } */
/*   } */

/*   puts(""); // print new line, for debugging. */

/*   fclose(lx.rd.src); */
/*   return 0; */
/* } */
