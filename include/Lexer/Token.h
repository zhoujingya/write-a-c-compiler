#ifndef TINYCC_LEXER_TOKEN_H
#define TINYCC_LEXER_TOKEN_H

#include "Support/TokenKinds.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SMLoc.h>

using namespace llvm;

namespace tinycc {

class Lexer;

class Token {
  friend class Lexer;

  /// The location of the token.
  const char *Ptr;

  /// The length of the token.
  size_t Length;

  /// Kind - The actual flavor of token this is.
  tok::TokenKind Kind;

public:
  tok::TokenKind getKind() const { return Kind; }
  void setKind(tok::TokenKind K) { Kind = K; }

  /// is/isNot - Predicates to check if this token is a
  /// specific kind, as in "if (Tok.is(tok::l_brace))
  /// {...}".
  bool is(tok::TokenKind K) const { return Kind == K; }
  bool isNot(tok::TokenKind K) const { return Kind != K; }
  template <typename... Tokens> bool isOneOf(Tokens &&...Toks) const {
    return (... || is(Toks));
  }

  const char *getName() const { return tok::getTokenName(Kind); }

  SMLoc getLocation() const { return SMLoc::getFromPointer(Ptr); }
  size_t getLength() const { return Length; }

  StringRef getIdentifier() const {
    // assert(is(tok::identifier) && "Cannot get identfier of non-identifier");
    return StringRef(Ptr, Length);
  }

  StringRef getLiteralData() const {
    assert(isOneOf(tok::identifier) &&
           "Cannot get literal data of non-literal");
    return StringRef(Ptr, Length);
  }

  StringRef getConstantValue() const {
    assert(isOneOf(tok::integer_cons, tok::float_cons) &&
           "Cannot get value of non-constant token");
    return StringRef(Ptr, Length);
  }
};

} // namespace tinycc
#endif
