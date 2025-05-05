#ifndef TINYCC_AST_AST_H
#define TINYCC_AST_AST_H

#include "Support/TokenKinds.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SMLoc.h"
#include <string>
#include <vector>

using namespace llvm;
namespace tinycc {

class Decl;
class Expr;
class Stmt;

using DeclList = std::vector<Decl *>;
using ExprList = std::vector<Expr *>;
using StmtList = std::vector<Stmt *>;

// Base class for all declarations
class Decl {
public:
  enum DeclKind { DK_Function, DK_Var };

private:
  const DeclKind Kind;

protected:
  SMLoc Loc;
  StringRef Name;

public:
  Decl(DeclKind Kind, SMLoc Loc, StringRef Name)
      : Kind(Kind), Loc(Loc), Name(Name) {}
  virtual ~Decl() = default;

  DeclKind getKind() const { return Kind; }
  SMLoc getLocation() const { return Loc; }
  StringRef getName() const { return Name; }
};

// Function parameter declaration
class ParamDecl : public Decl {
  // Type represented as a string for now, can be enhanced later
  StringRef Type;

public:
  ParamDecl(SMLoc Loc, StringRef Name, StringRef Type)
      : Decl(DK_Var, Loc, Name), Type(Type) {}

  StringRef getType() const { return Type; }
};

using ParamList = std::vector<ParamDecl *>;

// Function declaration
class FunctionDecl : public Decl {
  ParamList Params;
  StringRef ReturnType;
  StmtList Body;

public:
  FunctionDecl(SMLoc Loc, StringRef Name, StringRef ReturnType,
               ParamList Params)
      : Decl(DK_Function, Loc, Name), Params(Params), ReturnType(ReturnType) {}

  const ParamList &getParams() const { return Params; }
  StringRef getReturnType() const { return ReturnType; }

  void setBody(StmtList Body) { this->Body = Body; }
  const StmtList &getBody() const { return Body; }

  static bool classof(const Decl *D) { return D->getKind() == DK_Function; }
};

// Variable declaration
class VarDecl : public Decl {
  StringRef Type;
  Expr *Init; // Optional initializer

public:
  VarDecl(SMLoc Loc, StringRef Name, StringRef Type, Expr *Init = nullptr)
      : Decl(DK_Var, Loc, Name), Type(Type), Init(Init) {}

  StringRef getType() const { return Type; }
  Expr *getInit() const { return Init; }
  void setInit(Expr *E) { Init = E; }

  static bool classof(const Decl *D) { return D->getKind() == DK_Var; }
};

// Base class for all expressions
class Expr {
public:
  enum ExprKind { EK_Binary, EK_Unary, EK_IntegerLiteral, EK_VarRef, EK_Call };

private:
  const ExprKind Kind;
  SMLoc Loc;

protected:
  Expr(ExprKind Kind, SMLoc Loc) : Kind(Kind), Loc(Loc) {}

public:
  virtual ~Expr() = default;

  ExprKind getKind() const { return Kind; }
  SMLoc getLocation() const { return Loc; }
};

// Integer literal expression
class IntegerLiteral : public Expr {
  llvm::APSInt Value;

public:
  IntegerLiteral(SMLoc Loc, const llvm::APSInt &Value)
      : Expr(EK_IntegerLiteral, Loc), Value(Value) {}

  const llvm::APSInt &getValue() const { return Value; }

  static bool classof(const Expr *E) {
    return E->getKind() == EK_IntegerLiteral;
  }
};

// Variable reference expression
class VarRefExpr : public Expr {
  StringRef Name;

public:
  VarRefExpr(SMLoc Loc, StringRef Name) : Expr(EK_VarRef, Loc), Name(Name) {}

  StringRef getName() const { return Name; }

  static bool classof(const Expr *E) { return E->getKind() == EK_VarRef; }
};

// Binary operation expression
class BinaryExpr : public Expr {
public:
  enum BinaryOpKind { BO_Add, BO_Sub, BO_Mul, BO_Div, BO_Lt, BO_Gt, BO_Eq };

private:
  BinaryOpKind Op;
  Expr *Left;
  Expr *Right;

public:
  BinaryExpr(SMLoc Loc, BinaryOpKind Op, Expr *Left, Expr *Right)
      : Expr(EK_Binary, Loc), Op(Op), Left(Left), Right(Right) {}

  BinaryOpKind getOpcode() const { return Op; }
  Expr *getLeft() const { return Left; }
  Expr *getRight() const { return Right; }

  static bool classof(const Expr *E) { return E->getKind() == EK_Binary; }
};

// Unary operation expression
class UnaryExpr : public Expr {
public:
  enum UnaryOpKind { UO_Minus, UO_Not };

private:
  UnaryOpKind Op;
  Expr *SubExpr;

public:
  UnaryExpr(SMLoc Loc, UnaryOpKind Op, Expr *SubExpr)
      : Expr(EK_Unary, Loc), Op(Op), SubExpr(SubExpr) {}

  UnaryOpKind getOpcode() const { return Op; }
  Expr *getSubExpr() const { return SubExpr; }

  static bool classof(const Expr *E) { return E->getKind() == EK_Unary; }
};

// Function call expression
class CallExpr : public Expr {
  StringRef Callee;
  ExprList Args;

public:
  CallExpr(SMLoc Loc, StringRef Callee, ExprList Args)
      : Expr(EK_Call, Loc), Callee(Callee), Args(Args) {}

  StringRef getCallee() const { return Callee; }
  const ExprList &getArgs() const { return Args; }

  static bool classof(const Expr *E) { return E->getKind() == EK_Call; }
};

// Base class for all statements
class Stmt {
public:
  enum StmtKind { SK_Expr, SK_Return, SK_If, SK_Compound };

private:
  const StmtKind Kind;

protected:
  Stmt(StmtKind Kind) : Kind(Kind) {}

public:
  virtual ~Stmt() = default;

  StmtKind getKind() const { return Kind; }
};

// Expression statement
class ExprStmt : public Stmt {
  Expr *E;

public:
  ExprStmt(Expr *E) : Stmt(SK_Expr), E(E) {}

  Expr *getExpr() const { return E; }

  static bool classof(const Stmt *S) { return S->getKind() == SK_Expr; }
};

// Return statement
class ReturnStmt : public Stmt {
  Expr *RetVal; // Optional return value

public:
  ReturnStmt(Expr *RetVal = nullptr) : Stmt(SK_Return), RetVal(RetVal) {}

  Expr *getRetVal() const { return RetVal; }

  static bool classof(const Stmt *S) { return S->getKind() == SK_Return; }
};

// If statement
class IfStmt : public Stmt {
  Expr *Cond;
  Stmt *Then;
  Stmt *Else; // Optional else block

public:
  IfStmt(Expr *Cond, Stmt *Then, Stmt *Else = nullptr)
      : Stmt(SK_If), Cond(Cond), Then(Then), Else(Else) {}

  Expr *getCond() const { return Cond; }
  Stmt *getThen() const { return Then; }
  Stmt *getElse() const { return Else; }

  static bool classof(const Stmt *S) { return S->getKind() == SK_If; }
};

// Compound statement (block)
class CompoundStmt : public Stmt {
  StmtList Body;

public:
  CompoundStmt(StmtList Body) : Stmt(SK_Compound), Body(Body) {}

  const StmtList &getBody() const { return Body; }

  static bool classof(const Stmt *S) { return S->getKind() == SK_Compound; }
};

} // namespace tinycc
#endif
