/***************************************************
 Copyright 2024 Mark Mandriota. All right reserved.
***************************************************/

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*** UTILS ***/

#define IS_WHITESPACE(c)                                                       \
  (c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n')

#define IS_LETTER(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_')

#define IS_DIGIT(c) (c >= '0' && c <= '9')

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

/*************/

enum TokenType : int8_t {
  TT_ERR = -1,
  TT_EOS,

  TT_SYM,
  TT_INT,
  TT_FLT,

  TT_OP_LET,

  TT_OP_ADD,
  TT_OP_SUB,

  TT_OP_MUL,
  TT_OP_QUO,
  TT_OP_MOD,

  TT_OP_POW,

  // PARENTHESES
  TT_LP,
  TT_RP,

  // END OF EXPRESSION
  TT_EE,
};

const char *tt_stringify(enum TokenType tt) {
  switch (tt) {
  case TT_ERR:
    return "ERROR";
  case TT_SYM:
    return "SYMBOL";
  case TT_INT:
    return "INTEGER NUMBER";
  case TT_FLT:
    return "FLOATING-POINT NUMBER";
  case TT_OP_LET:
    return "LET OPERATOR";
  case TT_OP_ADD:
    return "ADD OPERATOR";
  case TT_OP_SUB:
    return "SUB OPERATOR";
  case TT_OP_MUL:
    return "MUL OPERATOR";
  case TT_OP_QUO:
    return "QUO OPERATOR";
  case TT_OP_MOD:
    return "MOD OPERATOR";
  case TT_OP_POW:
    return "POW OPERATOR";
  case TT_LP:
    return "LEFT PAREN";
  case TT_RP:
    return "RIGHT PAREN";
  case TT_EE:
    return "END OF EXPRESSION";
  default:
    return "INVALID TOKEN";
  }
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

  union {
    struct StringBuffer sb;
    long double n_flt;
    int64_t n_int;
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

  lx->rd.mrk = lx->rd.ptr;

  int64_t mnt, exp;
  lx_read_integer(lx, &mnt, &exp);

  lx->tk_opt.n_flt = (long double)mnt * powl(10, exp);

  char cc;
  if (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
      (cc = lx->rd.page.str.data[lx->rd.ptr]) == '.') {

    rd_next_char(&lx->rd);
    int64_t decimal_log10 = lx_read_integer(lx, &mnt, &exp);
    lx->tk_opt.n_flt += (long double)mnt / decimal_log10;
  } else if (exp == 0) {
    tt = TT_INT;
    lx->tk_opt.n_int = mnt;
  }

  rd_prev(&lx->rd);

  return tt;
}

void lx_read_symbol(struct Lexer *lx) {
  lx->rd.mrk = lx->rd.ptr;

  lx->tk_opt.sb.cap = 64;
  lx->tk_opt.sb.str.len = 0;
  lx->tk_opt.sb.str.data = malloc(lx->tk_opt.sb.cap);

  char cc;
  while (!(lx->rd.eos || lx->rd.page.str.len == 0) &&
         IS_LETTER((cc = lx->rd.page.str.data[lx->rd.ptr]))) {
    sb_push_char(&lx->tk_opt.sb, cc);
    rd_next_char(&lx->rd);
  }

  sb_push_char(&lx->tk_opt.sb, '\0'); // for debug printf
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
    return TT_OP_LET;
  case '+':
    return TT_OP_ADD;
  case '-':
    return TT_OP_SUB;
  case '*':
    return TT_OP_MUL;
  case '/':
    return TT_OP_QUO;
  case '%':
    return TT_OP_MOD;
  case '^':
    return TT_OP_POW;
  case '(':
    return TT_LP;
  case ')':
    return TT_RP;
  case ';':
    return TT_EE;
  default:
    if (IS_DIGIT(cc)) {
      return lx_next_token_number(lx);
    } else if (IS_LETTER(cc)) {
      lx_read_symbol(lx);
      return TT_SYM;
    }
    break;
  }

  return TT_ERR;
};

enum NodeType : int8_t {
  NT_LET,

  NT_ADD,
  NT_SUB,

  NT_MUL,
  NT_QUO,
  NT_MOD,

  NT_POW,
};

struct UnOp {
  struct String a;
  enum TokenType tk_a;
};

struct BiOp {
  struct String a, b;
  enum TokenType tk_a, tk_b;
};

struct NodeI {
  enum NodeType nodeType;

  struct NodeI *childs;
  size_t childs_len;
};

struct Parser {
  struct Lexer lx;
};

struct Interpreter {};

int main() {
#define FILE_NAME "test.meva"
  FILE *fs = fopen(FILE_NAME, "w");

  fprintf(fs, "something=e^(pi*i)/12346789987654321123456789");

  printf("1000000000000.1234567 = %0.15Lf\n",
         (long double)1000000000000.1234567);

  fclose(fs);

  struct Lexer lx = {
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
  };

  enum TokenType tk;
  while (true) {
    tk = lx_next_token(&lx);
    if (tk == TT_EOS)
      break;

    printf("token: '%d' (%s)\n", tk, tt_stringify(tk));

    if (tk == TT_SYM) {
      printf("optional: %s\n", lx.tk_opt.sb.str.data);
    } else if (tk == TT_INT) {
      printf("optional: %lld\n", lx.tk_opt.n_int);
    } else if (tk == TT_FLT) {
      printf("optional: %Lf\n", lx.tk_opt.n_flt);
    }
  }

  puts(""); // print new line, for debugging.

  fclose(lx.rd.src);
  return 0;
}
