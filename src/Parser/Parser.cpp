#include "Parser/Parser.h"
#include "AST/AST.h"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringExtras.h>
#include <memory>

using namespace tinycc;

// Utility methods
bool Parser::consume(tok::TokenKind Kind) {
  if (CurTok.is(Kind)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::expect(tok::TokenKind Kind) {
  if (CurTok.is(Kind)) {
    return true;
  }
  Diags.report(CurTok.getLocation(), diag::err_expected,
               tok::getTokenName(Kind), StringRef(CurTok.getName()));
  return false;
}

// Main entry point for parsing
std::vector<std::unique_ptr<Decl>> Parser::parse() {
  std::vector<std::unique_ptr<Decl>> Decls;

  // Keep parsing until we hit EOF
  while (!CurTok.is(tok::eof)) {
    // Skip any stray closing braces - they're likely from a previous error
    if (CurTok.is(tok::close_brace)) {
      // Just consume them silently and move on
      advance();
      continue;
    }

    // Try to parse a top-level declaration
    if (auto D = parseTopLevelDecl()) {
      Decls.push_back(std::move(D));
    } else {
      // If we're looking at a '(' after a failed declaration, it's likely part
      // of an invalid function
      if (CurTok.is(tok::open_paren)) {
        // Skip the entire parameter list
        advance(); // consume '('
        int ParenLevel = 1;

        while (ParenLevel > 0 && !CurTok.is(tok::eof)) {
          if (CurTok.is(tok::open_paren))
            ParenLevel++;
          else if (CurTok.is(tok::close_paren))
            ParenLevel--;

          advance();

          if (ParenLevel == 0)
            break;
        }

        // Now look for a function body if present
        if (CurTok.is(tok::open_brace)) {
          int BraceLevel = 1;
          advance(); // consume '{'

          while (BraceLevel > 0 && !CurTok.is(tok::eof)) {
            if (CurTok.is(tok::open_brace))
              BraceLevel++;
            else if (CurTok.is(tok::close_brace))
              BraceLevel--;

            advance();

            if (BraceLevel == 0)
              break;
          }
        }
        // If there's a semicolon, consume it too
        else if (CurTok.is(tok::semi)) {
          advance();
        }

        continue;
      }

      // Error recovery: skip to the next semicolon or opening brace
      while (!CurTok.is(tok::semi) && !CurTok.is(tok::open_brace) &&
             !CurTok.is(tok::eof) && !CurTok.is(tok::close_brace)) {
        advance();
      }

      // If we found a semicolon, consume it
      if (CurTok.is(tok::semi)) {
        advance();
      }

      // If we found an opening brace, try to consume until the matching closing
      // brace
      if (CurTok.is(tok::open_brace)) {
        int BraceLevel = 1;
        advance(); // consume the opening brace

        while (BraceLevel > 0 && !CurTok.is(tok::eof)) {
          if (CurTok.is(tok::open_brace))
            BraceLevel++;
          else if (CurTok.is(tok::close_brace))
            BraceLevel--;

          advance();

          // If we've balanced out, we can stop
          if (BraceLevel == 0)
            break;
        }
      }
    }
  }

  return Decls;
}

// Parse top-level declarations
std::unique_ptr<Decl> Parser::parseTopLevelDecl() {
  // Check for type keywords with wrong case
  if (checkKeywordCaseError()) {
    return parseTopLevelDecl();
  }

  // For now, we only handle function declarations and global variables
  if (CurTok.is(tok::kw_int) || CurTok.is(tok::kw_void) || CurTok.is(tok::kw_float)) {
    StringRef Type = CurTok.getIdentifier();
    SMLoc TypeLoc = CurTok.getLocation();
    advance();

    // After a type specifier, we expect an identifier (variable or function name)
    if (!CurTok.is(tok::identifier)) {
      // If we see a constant, it's likely a malformed function declaration with a numeric name
      if (CurTok.is(tok::integer_cons)) {
        StringRef ConstValue = CurTok.getIdentifier();
        Diags.report(CurTok.getLocation(), diag::err_invalid_function_name, ConstValue);

        // Skip the constant token
        advance();

        // Look for an open parenthesis, which suggests it's attempting to be a function
        if (CurTok.is(tok::open_paren)) {
          // Skip the entire parameter list
          advance(); // consume '('
          int ParenLevel = 1;

          while (ParenLevel > 0 && !CurTok.is(tok::eof)) {
            if (CurTok.is(tok::open_paren))
              ParenLevel++;
            else if (CurTok.is(tok::close_paren))
              ParenLevel--;

            advance();

            if (ParenLevel == 0)
              break;
          }

          // If there's a function body, skip it too
          if (CurTok.is(tok::open_brace)) {
            int BraceLevel = 1;
            advance(); // consume '{'

            while (BraceLevel > 0 && !CurTok.is(tok::eof)) {
              if (CurTok.is(tok::open_brace))
                BraceLevel++;
              else if (CurTok.is(tok::close_brace))
                BraceLevel--;

              advance();

              if (BraceLevel == 0)
                break;
            }
          }
        }
      } else {
        Diags.report(CurTok.getLocation(), diag::err_expected, "identifier",
                     StringRef(CurTok.getName()));
      }

      // Skip until we find something we can use to recover
      while (!CurTok.is(tok::semi) && !CurTok.is(tok::open_brace) &&
             !CurTok.is(tok::open_paren) && !CurTok.is(tok::eof)) {
        advance();
      }
      return nullptr;
    }

    StringRef Name = CurTok.getIdentifier();
    SMLoc NameLoc = CurTok.getLocation();
    advance();

    // Check if it's a function declaration
    if (CurTok.is(tok::open_paren)) {
      // Function declaration
      advance(); // consume '('

      // Parse parameter list
      ParamList Params;
      if (!CurTok.is(tok::close_paren)) {
        Params = parseParamList();
      }

      if (!expect(tok::close_paren)) {
        return nullptr;
      }
      advance(); // consume ')'

      auto Func = std::make_unique<FunctionDecl>(NameLoc, Name, Type, Params);

      // Check for function body
      if (CurTok.is(tok::open_brace)) {
        auto Body = parseCompoundStmt();
        if (Body) {
          Func->setBody(Body->getBody());
        }
      } else if (CurTok.is(tok::semi)) {
        advance(); // consume ';'
      } else {
        Diags.report(CurTok.getLocation(), diag::err_expected, "'{' or ';'",
                     StringRef(CurTok.getName()));
        // Try to recover by skipping to next declaration
        while (!CurTok.is(tok::semi) && !CurTok.is(tok::open_brace) &&
               !CurTok.is(tok::eof)) {
          advance();
        }

        // If we found a brace, try to parse function body
        if (CurTok.is(tok::open_brace)) {
          auto Body = parseCompoundStmt();
          if (Body) {
            Func->setBody(Body->getBody());
          }
        } else if (CurTok.is(tok::semi)) {
          advance(); // consume ';'
        }
      }

      return Func;
    } else {
      // Variable declaration
      auto Var = std::make_unique<VarDecl>(NameLoc, Name, Type);

      // Check for initialization
      if (CurTok.is(tok::equal)) {
        advance(); // consume '='
        auto Init = parseExpr();
        if (Init)
          Var->setInit(Init.release());
      }

      if (!expect(tok::semi)) {
        return nullptr;
      }
      advance(); // consume ';'

      return Var;
    }
  }

  Diags.report(CurTok.getLocation(), diag::err_expected, "type specifier",
               StringRef(CurTok.getName()));
  return nullptr;
}

// Parse parameter list for function declarations
ParamList Parser::parseParamList() {
  ParamList Params;

  do {
    auto Param = parseParamDecl();
    if (Param) {
      Params.push_back(Param.release());
    } else if (CurTok.is(tok::close_paren)) {
      break; // void parameter or error
    } else {
      return Params; // Error recovery
    }
  } while (consume(tok::comma));

  return Params;
}

// Parse a single parameter declaration
std::unique_ptr<ParamDecl> Parser::parseParamDecl() {
  if (!CurTok.is(tok::kw_int) && !CurTok.is(tok::kw_void)) {
    Diags.report(CurTok.getLocation(), diag::err_expected, "type specifier",
                 StringRef(CurTok.getName()));
    return nullptr;
  }

  StringRef Type = CurTok.getIdentifier();
  advance();

  // Special case for 'void' parameter (i.e., no parameters)
  if (Type == "void" && !CurTok.is(tok::identifier)) {
    return nullptr; // Return null to indicate void parameter
  }

  if (!CurTok.is(tok::identifier)) {
    Diags.report(CurTok.getLocation(), diag::err_expected, "identifier",
                 StringRef(CurTok.getName()));
    return nullptr;
  }

  StringRef Name = CurTok.getIdentifier();
  SMLoc Loc = CurTok.getLocation();
  advance();

  return std::make_unique<ParamDecl>(Loc, Name, Type);
}

// Parse compound statement (block)
std::unique_ptr<CompoundStmt> Parser::parseCompoundStmt() {
  if (!expect(tok::open_brace)) {
    return nullptr;
  }
  advance(); // consume '{'

  StmtList Body;
  while (!CurTok.is(tok::close_brace) && !CurTok.is(tok::eof)) {
    if (auto S = parseStmt()) {
      Body.push_back(S.release());
    } else {
      // Skip to next statement for error recovery
      while (!CurTok.is(tok::semi) && !CurTok.is(tok::close_brace) &&
             !CurTok.is(tok::eof)) {
        advance();
      }
      if (CurTok.is(tok::semi))
        advance();
    }
  }

  if (CurTok.is(tok::close_brace)) {
    advance(); // consume '}'
  } else {
    Diags.report(CurTok.getLocation(), diag::err_expected, "}",
                 StringRef(CurTok.getName()));
    return nullptr;
  }

  return std::make_unique<CompoundStmt>(std::move(Body));
}

// Parse any statement
std::unique_ptr<Stmt> Parser::parseStmt() {
  // Check for keywords with wrong case
  if (checkKeywordCaseError()) {
    return parseStmt();
  }

  // Check for variable declarations
  if (CurTok.is(tok::kw_int) || CurTok.is(tok::kw_void)) {
    StringRef Type = CurTok.getIdentifier();
    SMLoc TypeLoc = CurTok.getLocation();
    advance();

    if (CurTok.is(tok::identifier)) {
      StringRef Name = CurTok.getIdentifier();
      SMLoc NameLoc = CurTok.getLocation();
      advance();

      auto VD = std::make_unique<VarDecl>(NameLoc, Name, Type);

      // Check for initialization
      if (CurTok.is(tok::equal)) {
        advance(); // consume '='
        auto Init = parseExpr();
        if (Init)
          VD->setInit(Init.release());
      }

      if (!expect(tok::semi)) {
        return nullptr;
      }
      advance(); // consume ';'

      // Create a reference to the variable for the expression statement
      auto VarRef = std::make_unique<VarRefExpr>(NameLoc, Name);

      // Return an expression statement with the variable reference
      return std::make_unique<ExprStmt>(VarRef.release());
    }
  }

  if (CurTok.is(tok::kw_return)) {
    return parseReturnStmt();
  } else if (CurTok.is(tok::kw_if)) {
    return parseIfStmt();
  } else if (CurTok.is(tok::open_brace)) {
    return parseCompoundStmt();
  } else {
    return parseExprStmt();
  }
}

// Parse return statement
std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
  SMLoc Loc = CurTok.getLocation();
  advance(); // consume 'return'

  std::unique_ptr<Expr> RetVal = nullptr;
  if (!CurTok.is(tok::semi)) {
    RetVal = parseExpr();
  }

  if (!expect(tok::semi)) {
    return nullptr;
  }
  advance(); // consume ';'

  return std::make_unique<ReturnStmt>(RetVal.release());
}

// Parse if statement
std::unique_ptr<IfStmt> Parser::parseIfStmt() {
  advance(); // consume 'if'

  if (!expect(tok::open_paren)) {
    return nullptr;
  }
  advance(); // consume '('

  auto Cond = parseExpr();
  if (!Cond) {
    return nullptr;
  }

  if (!expect(tok::close_paren)) {
    return nullptr;
  }
  advance(); // consume ')'

  auto Then = parseStmt();
  if (!Then) {
    return nullptr;
  }

  std::unique_ptr<Stmt> Else = nullptr;
  if (CurTok.is(tok::kw_else)) {
    advance(); // consume 'else'
    Else = parseStmt();
    if (!Else) {
      return nullptr;
    }
  }

  return std::make_unique<IfStmt>(Cond.release(), Then.release(),
                                  Else.release());
}

// Parse expression statement (including assignments)
std::unique_ptr<ExprStmt> Parser::parseExprStmt() {
  auto E = parseExpr();
  if (!E) {
    return nullptr;
  }

  if (!expect(tok::semi)) {
    return nullptr;
  }
  advance(); // consume ';'

  return std::make_unique<ExprStmt>(E.release());
}

// Parse expressions
std::unique_ptr<Expr> Parser::parseExpr() {
  return parseAssignExpr();
}

// Parse assignment expressions
std::unique_ptr<Expr> Parser::parseAssignExpr() {
  auto LHS = parseEqualityExpr();
  if (!LHS)
    return nullptr;

  if (CurTok.is(tok::equal)) {
    SMLoc OpLoc = CurTok.getLocation();
    advance(); // consume '='

    auto RHS = parseAssignExpr();
    if (!RHS)
      return nullptr;

    // Check if LHS is a variable reference
    if (auto *VR = llvm::dyn_cast<VarRefExpr>(LHS.get())) {
      // Create an assignment expression (using binary expression with BO_Eq)
      return std::make_unique<BinaryExpr>(OpLoc, BinaryExpr::BO_Eq, LHS.release(), RHS.release());
    }

    Diags.report(OpLoc, diag::err_expected, "lvalue", "expression");
    return nullptr;
  }

  return LHS;
}

// Parse equality expressions (==, !=)
std::unique_ptr<Expr> Parser::parseEqualityExpr() {
  auto E = parseRelationalExpr();
  if (!E)
    return nullptr;

  // For now, we don't support equality operators yet
  return E;
}

// Parse relational expressions (<, >, <=, >=)
std::unique_ptr<Expr> Parser::parseRelationalExpr() {
  auto E = parseAdditiveExpr();
  if (!E)
    return nullptr;

  while (CurTok.is(tok::less) || CurTok.is(tok::greater) || CurTok.is(tok::equal)) {
    BinaryExpr::BinaryOpKind Op;
    if (CurTok.is(tok::less))
      Op = BinaryExpr::BO_Lt;
    else if (CurTok.is(tok::greater))
      Op = BinaryExpr::BO_Gt;
    else
      Op = BinaryExpr::BO_Eq;

    SMLoc OpLoc = CurTok.getLocation();
    advance(); // consume the operator

    auto RHS = parseAdditiveExpr();
    if (!RHS)
      return nullptr;

    E = std::make_unique<BinaryExpr>(OpLoc, Op, E.release(), RHS.release());
  }

  return E;
}

// Parse additive expressions (+, -)
std::unique_ptr<Expr> Parser::parseAdditiveExpr() {
  auto E = parseMultiplicativeExpr();
  if (!E)
    return nullptr;

  while (CurTok.is(tok::plus) || CurTok.is(tok::minus)) {
    BinaryExpr::BinaryOpKind Op =
        CurTok.is(tok::plus) ? BinaryExpr::BO_Add : BinaryExpr::BO_Sub;
    SMLoc OpLoc = CurTok.getLocation();
    advance(); // consume '+' or '-'

    auto RHS = parseMultiplicativeExpr();
    if (!RHS)
      return nullptr;

    E = std::make_unique<BinaryExpr>(OpLoc, Op, E.release(), RHS.release());
  }

  return E;
}

// Parse multiplicative expressions (*, /)
std::unique_ptr<Expr> Parser::parseMultiplicativeExpr() {
  auto E = parseUnaryExpr();
  if (!E)
    return nullptr;

  while (CurTok.is(tok::star) || CurTok.is(tok::slash)) {
    BinaryExpr::BinaryOpKind Op =
        CurTok.is(tok::star) ? BinaryExpr::BO_Mul : BinaryExpr::BO_Div;
    SMLoc OpLoc = CurTok.getLocation();
    advance(); // consume '*' or '/'

    auto RHS = parseUnaryExpr();
    if (!RHS)
      return nullptr;

    E = std::make_unique<BinaryExpr>(OpLoc, Op, E.release(), RHS.release());
  }

  return E;
}

// Parse unary expressions (-, !)
std::unique_ptr<Expr> Parser::parseUnaryExpr() {
  if (CurTok.is(tok::minus)) {
    SMLoc OpLoc = CurTok.getLocation();
    advance(); // consume '-'

    auto SubExpr = parseUnaryExpr();
    if (!SubExpr)
      return nullptr;

    return std::make_unique<UnaryExpr>(OpLoc, UnaryExpr::UO_Minus,
                                       SubExpr.release());
  }

  return parsePrimaryExpr();
}

// Parse primary expressions (identifiers, literals, parenthesized expressions)
std::unique_ptr<Expr> Parser::parsePrimaryExpr() {
  if (CurTok.is(tok::identifier)) {
    StringRef Name = CurTok.getIdentifier();
    SMLoc Loc = CurTok.getLocation();
    advance();

    // Check if it's a function call
    if (CurTok.is(tok::open_paren)) {
      return parseCallExpr(Name, Loc);
    }

    // Otherwise, it's a variable reference
    return std::make_unique<VarRefExpr>(Loc, Name);
  } else if (CurTok.is(tok::integer_cons)) {
    return parseIntegerLiteral();
  } else if (CurTok.is(tok::float_cons)) {
    return parseFloatLiteral();
  } else if (CurTok.is(tok::open_paren)) {
    advance(); // consume '('
    auto E = parseExpr();
    if (!E) {
      return nullptr;
    }

    if (!expect(tok::close_paren)) {
      return nullptr;
    }
    advance(); // consume ')'

    return E;
  }

  Diags.report(CurTok.getLocation(), diag::err_expected, "expression",
               StringRef(CurTok.getName()));
  return nullptr;
}

// Parse integer literal
std::unique_ptr<Expr> Parser::parseIntegerLiteral() {
  SMLoc Loc = CurTok.getLocation();
  StringRef ValueStr = CurTok.getIdentifier();

  // Parse the integer value
  llvm::APInt Value(32, ValueStr.str(), 10);
  advance();

  return std::make_unique<IntegerLiteral>(Loc, llvm::APSInt(Value));
}

// Parse float literal
std::unique_ptr<Expr> Parser::parseFloatLiteral() {
  SMLoc Loc = CurTok.getLocation();
  StringRef ValueStr = CurTok.getIdentifier();

  // Parse the float value - use standard C++ conversion to avoid LLVM API version issues
  float FloatValue = std::stof(ValueStr.str());
  llvm::outs() << "Float value: " << FloatValue << "\n";
  llvm::APFloat Value(FloatValue);

  advance();

  return std::make_unique<FloatLiteral>(Loc, Value);
}

// Parse function call
std::unique_ptr<CallExpr> Parser::parseCallExpr(StringRef FuncName, SMLoc Loc) {
  advance(); // consume '('

  ExprList Args;
  if (!CurTok.is(tok::close_paren)) {
    // Parse argument list
    do {
      auto Arg = parseExpr();
      if (Arg) {
        Args.push_back(Arg.release());
      } else {
        return nullptr;
      }
    } while (consume(tok::comma));
  }

  if (!expect(tok::close_paren)) {
    return nullptr;
  }
  advance(); // consume ')'

  return std::make_unique<CallExpr>(Loc, FuncName, std::move(Args));
}

// Helper method to check for keyword case errors (e.g., "RETURN" instead of
// "return")
bool Parser::checkKeywordCaseError() {
  if (!CurTok.is(tok::identifier))
    return false;

  StringRef Id = CurTok.getIdentifier();

  // Check all keywords by iterating through them
  for (tok::TokenKind K = tok::kw_start; K <= tok::kw_end;
       K = static_cast<tok::TokenKind>(K + 1)) {
    const char *KwSpelling = tok::getKeywordSpelling(K);
    if (KwSpelling && Id.equals_insensitive(KwSpelling) &&
        Id.str() != KwSpelling) {
      Diags.report(CurTok.getLocation(), diag::err_wrong_keyword_case, Id,
                   KwSpelling);
      advance(); // Skip this token
      return true;
    }
  }

  return false;
}
