#ifndef MEWA_TYPES_H
#define MEWA_TYPES_H

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

#endif
