#include "Lexer/Lexer.h"
#include <llvm/ADT/StringExtras.h>
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
      CASE(',', tok::comma);
      CASE('+', tok::plus);
      CASE('-', tok::minus);
      CASE('*', tok::star);
      CASE('=', tok::equal);
      CASE('<', tok::less);
      CASE('>', tok::greater);
#undef CASE
    case '/':
      if (*(CurPtr + 1) == '/' || *(CurPtr + 1) == '*') {
        comment();
        // After skipping a comment, restart the lexing process
        return next(Result);
      } else {
        formToken(Result, CurPtr + 1, tok::slash);
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

  // Check for keywords
  tok::TokenKind Kind = Keywords.getKeyword(Name, tok::identifier);
  formToken(Result, End, Kind);
}

// Improved number tokenization function to handle decimal, hexadecimal, and
// octal numbers
void Lexer::number(Token &Result) {
  const char *Start = CurPtr;
  const char *End = CurPtr;
  bool IsHex = false;
  bool IsOctal = false;
  bool IsFloat = false;

  // Check for hex (0x) or octal (0) prefix
  if (*End == '0') {
    ++End;
    if (*End == 'x' || *End == 'X') {
      IsHex = true;
      ++End;
      // At least one hex digit required after 0x
      if (!charinfo::isHexDigit(*End)) {
        Diags.report(getLoc(End), diag::invalid_suffix_in_constant, *End);
        formToken(Result, End, tok::integer_cons);
        return;
      }
    } else if (charinfo::isDigit(*End)) {
      IsOctal = true;
    } else if (*End == '.') {
      // This is a float like "0.123"
      IsFloat = true;
      ++End;
    }
  } else if (*End == '.') {
    // This is a float like ".123"
    IsFloat = true;
    ++End;
    // Must have at least one digit after the decimal point
    if (!charinfo::isDigit(*End)) {
      Diags.report(getLoc(End), diag::invalid_suffix_in_constant, *End);
      formToken(Result, End, tok::unknown);
      return;
    }
  }

  // Consume all valid digits based on the number type
  while (true) {
    if (IsHex && charinfo::isHexDigit(*End)) {
      ++End;
    } else if ((IsOctal && *End >= '0' && *End <= '7') ||
               (!IsHex && !IsOctal && !IsFloat && charinfo::isDigit(*End))) {
      ++End;
    } else if (IsFloat && charinfo::isDigit(*End)) {
      ++End;
    } else {
      break;
    }
  }

  // Check for decimal point in non-float numbers
  if (!IsFloat && !IsHex && !IsOctal && *End == '.') {
    IsFloat = true;
    ++End;
    // Consume digits after decimal point
    while (charinfo::isDigit(*End)) {
      ++End;
    }
  }

  // Check for exponent in float numbers (e.g., 1.23e+45)
  if (IsFloat && (*End == 'e' || *End == 'E')) {
    const char *ExpStart = End;
    ++End;

    // Optional sign
    if (*End == '+' || *End == '-') {
      ++End;
    }

    // Must have at least one digit in exponent
    if (!charinfo::isDigit(*End)) {
      Diags.report(getLoc(ExpStart), diag::invalid_suffix_in_constant, *ExpStart);
      formToken(Result, ExpStart, tok::float_cons);
      return;
    }

    // Consume exponent digits
    while (charinfo::isDigit(*End)) {
      ++End;
    }
  }

  // Check for invalid suffixes or characters
  if (*End && !charinfo::isWhitespace(*End) && *End != ';' && *End != ',' &&
      *End != ')' && *End != ']' && *End != '}') {
    const char *InvalidSuffix = End;
    Diags.report(getLoc(InvalidSuffix), diag::invalid_suffix_in_constant,
                 *InvalidSuffix);

    // Skip the invalid suffix
    while (*End && !charinfo::isWhitespace(*End) && *End != ';' &&
           *End != ',' && *End != ')' && *End != ']' && *End != '}') {
      ++End;
    }
  }

  // Form the token
  llvm::outs() << "IsFloat: " << IsFloat << "\n";
  formToken(Result, End, IsFloat ? tok::float_cons : tok::integer_cons);
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

  // For debugging purposes, you can uncomment this to print token information
  // if (Kind == tok::constant) {
  //   llvm::StringRef TokenStr(CurPtr, TokLen);
  //   llvm::errs() << "Formed constant token: '" << TokenStr << "'\n";
  // }

  CurPtr = TokEnd;
}

Token &Lexer::lookAhead() {
  if (!HasLookahead) {
    next(LookaheadToken);
    HasLookahead = true;
  }
  return LookaheadToken;
}

// 修改comment函数以支持单行和多行注释
void Lexer::comment() {
  assert(*CurPtr == '/' && "Expected comment start with /");

  if (*(CurPtr + 1) == '/') {
    // Single line comment, skip to end of line
    CurPtr += 2; // Skip the '//'
    while (*CurPtr && *CurPtr != '\n')
      ++CurPtr;
    if (*CurPtr == '\n')
      ++CurPtr; // Consume the newline
  } else if (*(CurPtr + 1) == '*') {
    // Multi-line comment, skip until */
    CurPtr += 2; // Skip the '/*'
    while (*CurPtr && !(*CurPtr == '*' && *(CurPtr + 1) == '/')) {
      ++CurPtr;
      if (!*CurPtr) {
        Diags.report(getLoc(), diag::err_unterminated_block_comment);
        return;
      }
    }
    if (*CurPtr)
      CurPtr += 2; // Skip the '*/'
  }
}
