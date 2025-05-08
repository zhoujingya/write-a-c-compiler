#ifndef TINYCC_PARSER_PARSER_H
#define TINYCC_PARSER_PARSER_H

#include "AST/AST.h"
#include "Lexer/Lexer.h"
#include "Support/Diagnostic.h"
#include <llvm/ADT/StringRef.h>
#include <memory>

namespace tinycc {

class Parser {
private:
  Lexer &Lex;
  DiagnosticsEngine &Diags;
  Token CurTok;

  // Utility methods for token handling
  void advance() { Lex.next(CurTok); }
  bool consume(tok::TokenKind Kind);
  bool expect(tok::TokenKind Kind);

  // Parsing methods for declarations
  std::unique_ptr<FunctionDecl> parseFunctionDecl();
  std::unique_ptr<VarDecl> parseVarDecl();
  std::unique_ptr<ParamDecl> parseParamDecl();
  ParamList parseParamList();

  // Parsing methods for statements
  std::unique_ptr<Stmt> parseStmt();
  std::unique_ptr<CompoundStmt> parseCompoundStmt();
  std::unique_ptr<ReturnStmt> parseReturnStmt();
  std::unique_ptr<IfStmt> parseIfStmt();
  std::unique_ptr<ExprStmt> parseExprStmt();

  // Parsing methods for expressions
  std::unique_ptr<Expr> parseExpr();
  std::unique_ptr<Expr> parseAssignExpr();
  std::unique_ptr<Expr> parseEqualityExpr();
  std::unique_ptr<Expr> parseRelationalExpr();
  std::unique_ptr<Expr> parseAdditiveExpr();
  std::unique_ptr<Expr> parseMultiplicativeExpr();
  std::unique_ptr<Expr> parseUnaryExpr();
  std::unique_ptr<Expr> parsePrimaryExpr();
  std::unique_ptr<Expr> parseIntegerLiteral();
  std::unique_ptr<Expr> parseFloatLiteral();
  std::unique_ptr<CallExpr> parseCallExpr(StringRef FuncName, SMLoc Loc);

  // Helper for detecting keyword case errors
  bool checkKeywordCaseError();

public:
  Parser(Lexer &Lex, DiagnosticsEngine &Diags) : Lex(Lex), Diags(Diags) {
    advance(); // Prime the first token
  }

  // Main parsing entry points
  std::vector<std::unique_ptr<Decl>> parse();
  std::unique_ptr<Decl> parseTopLevelDecl();
};

class ParserDriver {
  Parser Parser;
  Lexer Lex;
  DiagnosticsEngine Diags;

public:
  ParserDriver(Lexer &Lex, DiagnosticsEngine &Diags)
      : Parser(Lex, Diags), Lex(Lex), Diags(Diags) {}
  std::vector<std::unique_ptr<Decl>> parse() {
    return Parser.parse();
  }
};

} // namespace tinycc

#endif // TINYCC_PARSER_PARSER_H
