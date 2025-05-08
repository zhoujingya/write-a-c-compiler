#ifndef TINYCC_CODEGEN_CODEGEN_H
#define TINYCC_CODEGEN_CODEGEN_H

#include "AST/AST.h"
#include "Support/Diagnostic.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>

namespace tinycc {

class CodeGenerator {
private:
  DiagnosticsEngine &Diags;
  std::unique_ptr<llvm::LLVMContext> Context;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::IRBuilder<>> Builder;

  // Symbol table for variables
  std::map<std::string, llvm::Value *> NamedValues;

  // Current function being generated
  llvm::Function *CurFunction;

  // Helper methods for code generation
  llvm::Value *generateExpr(Expr *E);
  llvm::Value *generateIntegerLiteral(IntegerLiteral *IL);
  llvm::Value *generateFloatLiteral(FloatLiteral *FL);
  llvm::Value *generateVarRefExpr(VarRefExpr *VR);
  llvm::Value *generateBinaryExpr(BinaryExpr *BE);
  llvm::Value *generateUnaryExpr(UnaryExpr *UE);
  llvm::Value *generateCallExpr(CallExpr *CE);

  void generateStmt(Stmt *S);
  void generateReturnStmt(ReturnStmt *RS);
  void generateIfStmt(IfStmt *IS);
  void generateCompoundStmt(CompoundStmt *CS);
  void generateExprStmt(ExprStmt *ES);

  llvm::Function *generateFunctionDecl(FunctionDecl *FD);
  llvm::Value *generateVarDecl(VarDecl *VD);

  // Type conversion helpers
  llvm::Type *getLLVMType(StringRef TypeName);
  llvm::FunctionType *getFunctionType(FunctionDecl *FD);

public:
  CodeGenerator(DiagnosticsEngine &Diags, StringRef ModuleName = "tinycc_module");

  // Main entry point for code generation
  bool generateCode(const std::vector<std::unique_ptr<Decl>> &Decls);

  // Get the generated LLVM module
  llvm::Module *getModule() const { return TheModule.get(); }

  // Print the generated LLVM IR to the given output stream
  void print(llvm::raw_ostream &OS);
};

} // namespace tinycc

#endif // TINYCC_CODEGEN_CODEGEN_H
