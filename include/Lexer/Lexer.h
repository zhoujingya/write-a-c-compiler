#ifndef TINYLANG_LEXER_LEXER_H
#define TINYLANG_LEXER_LEXER_H

#include "Lexer/Token.h"
#include "Support/Diagnostic.h"
#include "Support/TokenKinds.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"

namespace tinycc {

class KeywordFilter {
  llvm::StringMap<tok::TokenKind> HashTable;

  void addKeyword(StringRef Keyword, tok::TokenKind TokenCode);

public:
  void addKeywords();

  tok::TokenKind getKeyword(StringRef Name,
                            tok::TokenKind DefaultTokenCode = tok::unknown) {
    auto Result = HashTable.find(Name);
    if (Result != HashTable.end())
      return Result->second;
    return DefaultTokenCode;
  }
};

class Lexer {
  SourceMgr &SrcMgr;
  DiagnosticsEngine &Diags;

  const char *CurPtr;
  StringRef CurBuf;

  /// CurBuffer - This is the current buffer index we're
  /// lexing from as managed by the SourceMgr object.
  unsigned CurBuffer = 0;

  KeywordFilter Keywords;

  // Lookahead token
  Token LookaheadToken;
  bool HasLookahead = false;

public:
  Lexer(SourceMgr &SrcMgr, DiagnosticsEngine &Diags)
      : SrcMgr(SrcMgr), Diags(Diags) {
    CurBuffer = SrcMgr.getMainFileID();
    CurBuf = SrcMgr.getMemoryBuffer(CurBuffer)->getBuffer();
    CurPtr = CurBuf.begin();
    Keywords.addKeywords();
  }

  // Constructor for testing - directly initialize from string
  Lexer(StringRef Input)
      : SrcMgr(*(new SourceMgr())), Diags(*(new DiagnosticsEngine(SrcMgr))) {
    CurBuf = Input;
    CurPtr = CurBuf.begin();
    Keywords.addKeywords();
  }

  DiagnosticsEngine &getDiagnostics() const { return Diags; }

  /// Returns the next token from the input.
  void next(Token &Result);

  /// Returns the next token from the input without consuming it.
  Token &lookAhead();

  /// Gets source code buffer.
  StringRef getBuffer() const { return CurBuf; }

  /// For testing - get all tokens from the input
  std::vector<Token> getAllTokens() {
    std::vector<Token> Tokens;
    Token Tok;
    do {
      next(Tok);
      Tokens.push_back(Tok);
    } while (!Tok.is(tok::eof));
    return Tokens;
  }

private:
  void identifier(Token &Result);
  // Form a number token from the current position.
  void number(Token &Result);
  // Form a string token from the current position.
  void string(Token &Result);

  void comment();

  // Get the location of the current position.
  SMLoc getLoc(const char *Ptr = nullptr) {
    if (Ptr == nullptr)
      Ptr = CurPtr;
    return SMLoc::getFromPointer(Ptr);
  }

  // Form a token from the current position to TokEnd.
  void formToken(Token &Result, const char *TokEnd, tok::TokenKind Kind);
};

class LexerDriver {
  Lexer Lexer;

public:
  LexerDriver(class Lexer &Lexer) : Lexer(Lexer) {}

  void run() {
    Token Tok;
    do {
      Lexer.next(Tok);
    } while (!Tok.is(tok::eof));
  }
};

} // namespace tinycc
#endif
