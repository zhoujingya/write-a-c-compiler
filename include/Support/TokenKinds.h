#ifndef TINYCC_SUPPORT_TOKENKINDS_H
#define TINYCC_SUPPORT_TOKENKINDS_H

#include <llvm/ADT/StringRef.h>

namespace tinycc {

namespace tok {
enum TokenKind {
#define TOK(ID) ID,
#include "Support/TokenKinds.def"
  NUM_TOKENS
};

/// Return the name of a token kind, like "identifier".
const char *getTokenName(TokenKind Kind);

/// Return the punctuator name like "(".
const char *getPunctuatorSpelling(TokenKind Kind);

/// Return the keyword spelling like "int".
const char *getKeywordSpelling(TokenKind Kind);
} // namespace tok

} // namespace tinycc

#endif // TINYCC_SUPPORT_TOKENKINDS_H
