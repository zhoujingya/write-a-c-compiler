#include "Lexer/Lexer.h"
#include <string>
#include <vector>

using namespace tinycc;

void KeywordFilter::addKeyword(StringRef Keyword, tok::TokenKind TokenCode) {
  HashTable.insert(std::make_pair(Keyword, TokenCode));
}

void KeywordFilter::addKeywords() {
#define KEYWORD(NAME, FLAGS) addKeyword(StringRef(#NAME), tok::kw_##NAME);
#include "Support/TokenKinds.def"
}

namespace charinfo {
LLVM_READNONE inline bool isASCII(char Ch) {
  return static_cast<unsigned char>(Ch) <= 127;
}

LLVM_READNONE inline bool isVerticalWhitespace(char Ch) {
  return isASCII(Ch) && (Ch == '\r' || Ch == '\n');
}

LLVM_READNONE inline bool isHorizontalWhitespace(char Ch) {
  return isASCII(Ch) && (Ch == ' ' || Ch == '\t' || Ch == '\f' || Ch == '\v');
}

LLVM_READNONE inline bool isWhitespace(char Ch) {
  return isHorizontalWhitespace(Ch) || isVerticalWhitespace(Ch);
}

LLVM_READNONE inline bool isDigit(char Ch) {
  return isASCII(Ch) && Ch >= '0' && Ch <= '9';
}

LLVM_READNONE inline bool isHexDigit(char Ch) {
  return isASCII(Ch) && (isDigit(Ch) || (Ch >= 'A' && Ch <= 'F'));
}

LLVM_READNONE inline bool isIdentifierHead(char Ch) {
  return isASCII(Ch) &&
         (Ch == '_' || (Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z'));
}

LLVM_READNONE inline bool isIdentifierBody(char Ch) {
  return isIdentifierHead(Ch) || isDigit(Ch);
}
} // namespace charinfo

void Lexer::next(Token &Result) {
  if (HasLookahead) {
    Result = LookaheadToken;
    HasLookahead = false;
    return;
  }

  while (*CurPtr && charinfo::isWhitespace(*CurPtr)) {
    ++CurPtr;
  }
  if (!*CurPtr) {
    Result.setKind(tok::eof);
    return;
  }
  if (charinfo::isIdentifierHead(*CurPtr)) {
    identifier(Result);
    return;
  } else if (charinfo::isDigit(*CurPtr)) {
    number(Result);
    return;
  }
  // Handle punctuators and operators
  else {
    switch (*CurPtr) {
#define CASE(ch, tok)                                                          \
  case ch:                                                                     \
    formToken(Result, CurPtr + 1, tok);                                        \
    break
      CASE('{', tok::open_brace);
      CASE('}', tok::close_brace);
      CASE(')', tok::close_paren);
      CASE('(', tok::open_paren);
      CASE(';', tok::semi);
#undef CASE
    case '/':
      if (*(CurPtr + 1) == '/' || *(CurPtr + 1) == '*') {
        comment();
      }
      break;
    default:
      Result.setKind(tok::unknown);
      Diags.report(getLoc(), diag::unknown_identifier, *CurPtr);
    }
    return;
  }
}

void Lexer::identifier(Token &Result) {
  const char *Start = CurPtr;
  const char *End = CurPtr + 1;
  while (charinfo::isIdentifierBody(*End))
    ++End;
  StringRef Name(Start, End - Start);
  formToken(Result, End, Keywords.getKeyword(Name, tok::identifier));
}

// TODO: not correct, should handle the case where the number is too large
void Lexer::number(Token &Result) {
  const char *End = CurPtr + 1;
  tok::TokenKind Kind = tok::unknown;
  bool HasInvalidSuffix = false;
  const char *InvalidSuffix = nullptr;
  while (*End && !charinfo::isWhitespace(*End) && *End != ';') {
    if (!charinfo::isDigit(*End)) {
      HasInvalidSuffix = true;
      InvalidSuffix = End;
      break;
    }
    ++End;
  }
  if (HasInvalidSuffix)
    Diags.report(getLoc(InvalidSuffix), diag::invalid_suffix_in_constant,
                 *InvalidSuffix);
  formToken(Result, End, Kind);
}

void Lexer::string(Token &Result) {
  const char *Start = CurPtr;
  const char *End = CurPtr + 1;
  while (*End && *End != *Start && !charinfo::isVerticalWhitespace(*End))
    ++End;
  if (charinfo::isVerticalWhitespace(*End)) {
    //   Diags.report(getLoc(), diag::err_unterminated_char_or_string);
    exit(1);
  }
  formToken(Result, End + 1, tok::identifier);
}

// Generate token from tokend and curptr
void Lexer::formToken(Token &Result, const char *TokEnd, tok::TokenKind Kind) {
  size_t TokLen = TokEnd - CurPtr;
  Result.Ptr = CurPtr;
  Result.Length = TokLen;
  Result.Kind = Kind;
  CurPtr = TokEnd;
}

Token &Lexer::lookAhead() {
  if (!HasLookahead) {
    next(LookaheadToken);
    HasLookahead = true;
  }
  return LookaheadToken;
}

// TODO: this is a very very simple comment lexer, it only supports // and /*
// */ and it does not handle the case where the comment is not terminated
void Lexer::comment() {
  while (*CurPtr && *CurPtr != '\n')
    ++CurPtr;
}
