#include "Support/TokenKinds.h"
#include <cassert>

using namespace tinycc;

static const char * const TokNames[] = {
#define TOK(Id) #Id,
#define KEYWORD(Id, Flag) #Id,
#define PUNCTUATOR(Id, Spelling) #Id,
#include "Support/TokenKinds.def"
  nullptr
};

const char *tok::getTokenName(TokenKind Kind) {
  if (Kind < tok::NUM_TOKENS)
    return TokNames[Kind];
  return nullptr;
}

static const char * const PunctuatorNames[] = {
#define PUNCTUATOR(Id, Spelling) Spelling,
#define TOK(Id)
#define KEYWORD(Id, Flag)
#include "Support/TokenKinds.def"
  nullptr
};

const char *tok::getPunctuatorSpelling(TokenKind Kind) {
  if (Kind < tok::NUM_TOKENS)
    return PunctuatorNames[Kind - tok::open_paren];
  return nullptr;
}

static const char * const KeywordNames[] = {
#define KEYWORD(Id, Flag) #Id,
#define TOK(Id)
#define PUNCTUATOR(Id, Spelling)
#include "Support/TokenKinds.def"
  nullptr
};

const char *tok::getKeywordSpelling(TokenKind Kind) {
  if (Kind > tok::kw_start && Kind < tok::kw_end)
    return KeywordNames[Kind - tok::kw_start];
  return nullptr;
}
