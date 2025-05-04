#ifndef TINYCC_BASIC_TOKENKINDS_H
#define TINYCC_BASIC_TOKENKINDS_H

#include "llvm/Support/Compiler.h"

namespace tinycc {

namespace tok {
enum TokenKind : unsigned short {
#define TOK(ID) ID,
#include "TokenKinds.def"
  NUM_TOKENS
};

const char *getTokenName(TokenKind Kind) LLVM_READNONE;

const char *getPunctuatorSpelling(TokenKind Kind) LLVM_READNONE;

const char *getKeywordSpelling(TokenKind Kind) LLVM_READNONE;
} // namespace tok
} // namespace tinycc

#endif
