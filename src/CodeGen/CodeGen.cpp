#include "CodeGen/CodeGen.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace tinycc;

CodeGenerator::CodeGenerator(DiagnosticsEngine &Diags, StringRef ModuleName)
    : Diags(Diags), CurFunction(nullptr) {
  Context = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>(ModuleName.str(), *Context);
  Builder = std::make_unique<llvm::IRBuilder<>>(*Context);
}

bool CodeGenerator::generateCode(
    const std::vector<std::unique_ptr<Decl>> &Decls) {
  bool Success = true;

  // Generate code for all top-level declarations
  for (const auto &D : Decls) {
    if (auto *FD = llvm::dyn_cast<FunctionDecl>(D.get())) {
      if (!generateFunctionDecl(FD))
        Success = false;
    } else if (auto *VD = llvm::dyn_cast<VarDecl>(D.get())) {
      if (!generateVarDecl(VD))
        Success = false;
    }
  }

  return Success;
}

void CodeGenerator::print(llvm::raw_ostream &OS) {
  TheModule->print(OS, nullptr);
}

llvm::Type *CodeGenerator::getLLVMType(StringRef TypeName) {
  if (TypeName == "int") {
    return llvm::Type::getInt32Ty(*Context);
  } else if (TypeName == "float") {
    return llvm::Type::getFloatTy(*Context);
  } else if (TypeName == "void") {
    return llvm::Type::getVoidTy(*Context);
  }

  // Report unknown type and default to int
  Diags.report(SMLoc(), diag::unknown_type, TypeName);
  return llvm::Type::getInt32Ty(*Context);
}

llvm::FunctionType *CodeGenerator::getFunctionType(FunctionDecl *FD) {
  llvm::Type *RetType = getLLVMType(FD->getReturnType());

  std::vector<llvm::Type *> ParamTypes;
  for (const auto *Param : FD->getParams()) {
    ParamTypes.push_back(getLLVMType(Param->getType()));
  }

  return llvm::FunctionType::get(RetType, ParamTypes, false);
}

llvm::Function *CodeGenerator::generateFunctionDecl(FunctionDecl *FD) {
  llvm::FunctionType *FT = getFunctionType(FD);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, FD->getName(), TheModule.get());

  // Set parameter names
  unsigned Idx = 0;
  for (auto &Arg : F->args()) {
    Arg.setName(FD->getParams()[Idx++]->getName());
  }

  // If this is just a declaration without a body, return the function
  if (FD->getBody().empty())
    return F;

  // Create a basic block for the function body
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*Context, "entry", F);
  Builder->SetInsertPoint(BB);

  // Save the current function
  llvm::Function *OldCurFunction = CurFunction;
  CurFunction = F;

  // Clear the variable symbol table
  NamedValues.clear();

  // Add parameters to the symbol table
  for (auto &Arg : F->args()) {
    NamedValues[Arg.getName().str()] = &Arg;
  }

  // Generate code for the function body
  for (Stmt *S : FD->getBody()) {
    generateStmt(S);
  }

  // Add a return statement if the function doesn't have one
  if (!Builder->GetInsertBlock()->getTerminator()) {
    if (F->getReturnType()->isVoidTy()) {
      Builder->CreateRetVoid();
    } else {
      // For non-void functions, return a default value (0 for int)
      Builder->CreateRet(llvm::Constant::getNullValue(F->getReturnType()));
    }
  }

  // Verify the function
  if (llvm::verifyFunction(*F, &llvm::errs())) {
    Diags.report(FD->getLocation(), diag::invalid_function, FD->getName());
    F->eraseFromParent();
    return nullptr;
  }

  // Restore the old current function
  CurFunction = OldCurFunction;

  return F;
}

llvm::Value *CodeGenerator::generateVarDecl(VarDecl *VD) {
  llvm::Type *VarType = getLLVMType(VD->getType());

  // Global variable
  if (!CurFunction) {
    llvm::GlobalVariable *GV = new llvm::GlobalVariable(
        *TheModule, VarType, false, llvm::GlobalValue::ExternalLinkage,
        llvm::Constant::getNullValue(VarType), VD->getName());

    // Initialize if there's an initializer
    if (VD->getInit()) {
      if (auto *FL = llvm::dyn_cast<FloatLiteral>(VD->getInit())) {
        GV->setInitializer(
            llvm::ConstantFP::get(VarType, FL->getValue().convertToFloat()));
      } else if (auto *IL = llvm::dyn_cast<IntegerLiteral>(VD->getInit())) {
        GV->setInitializer(
            llvm::ConstantInt::get(VarType, IL->getValue().getZExtValue()));
      }
    }

    return GV;
  }

  // Local variable
  llvm::AllocaInst *Alloca =
      Builder->CreateAlloca(VarType, nullptr, VD->getName());
  NamedValues[VD->getName().str()] = Alloca;

  // Initialize if there's an initializer
  if (VD->getInit()) {
    llvm::Value *InitVal = generateExpr(VD->getInit());
    if (!InitVal)
      return nullptr;
    Builder->CreateStore(InitVal, Alloca);
  }

  return Alloca;
}

void CodeGenerator::generateStmt(Stmt *S) {
  switch (S->getKind()) {
  case Stmt::SK_Return:
    generateReturnStmt(llvm::cast<ReturnStmt>(S));
    break;
  case Stmt::SK_If:
    generateIfStmt(llvm::cast<IfStmt>(S));
    break;
  case Stmt::SK_Compound:
    generateCompoundStmt(llvm::cast<CompoundStmt>(S));
    break;
  case Stmt::SK_Expr:
    generateExprStmt(llvm::cast<ExprStmt>(S));
    break;
  }
}

void CodeGenerator::generateReturnStmt(ReturnStmt *RS) {
  if (!RS->getRetVal()) {
    Builder->CreateRetVoid();
    return;
  }

  llvm::Value *RetVal = generateExpr(RS->getRetVal());
  if (!RetVal)
    return;

  Builder->CreateRet(RetVal);
}

void CodeGenerator::generateIfStmt(IfStmt *IS) {
  llvm::Value *CondV = generateExpr(IS->getCond());
  if (!CondV)
    return;

  // Convert condition to boolean (i1)
  CondV = Builder->CreateICmpNE(
      CondV, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0),
      "ifcond");

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  // Create blocks for the then and else cases
  llvm::BasicBlock *ThenBB =
      llvm::BasicBlock::Create(*Context, "then", TheFunction);
  llvm::BasicBlock *ElseBB =
      IS->getElse() ? llvm::BasicBlock::Create(*Context, "else") : nullptr;
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*Context, "ifcont");

  // Create conditional branch
  Builder->CreateCondBr(CondV, ThenBB, ElseBB ? ElseBB : MergeBB);

  // Emit then block
  Builder->SetInsertPoint(ThenBB);
  generateStmt(IS->getThen());

  // Check if the 'then' block already has a terminator (like a return)
  bool ThenHasTerminator =
      Builder->GetInsertBlock()->getTerminator() != nullptr;
  if (!ThenHasTerminator)
    Builder->CreateBr(MergeBB);

  // Emit else block if it exists
  bool ElseHasTerminator = false;
  if (ElseBB) {
    // Add the else block to the function
    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);
    generateStmt(IS->getElse());

    // Check if the 'else' block already has a terminator
    ElseHasTerminator = Builder->GetInsertBlock()->getTerminator() != nullptr;
    if (!ElseHasTerminator)
      Builder->CreateBr(MergeBB);
  }

  // Only add the merge block if it's reachable
  if (!ThenHasTerminator || (ElseBB && !ElseHasTerminator)) {
    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);
  } else {
    // If both branches have terminators, the merge block is unreachable
    // We can delete it
    delete MergeBB;
  }
}

void CodeGenerator::generateCompoundStmt(CompoundStmt *CS) {
  for (Stmt *S : CS->getBody()) {
    generateStmt(S);
  }
}

void CodeGenerator::generateExprStmt(ExprStmt *ES) {
  // Check if this is a variable reference that might be from a variable
  // declaration
  if (auto *VR = llvm::dyn_cast<VarRefExpr>(ES->getExpr())) {
    // If the variable is not in the symbol table, it might be a new declaration
    if (!NamedValues.count(VR->getName().str())) {
      // Create a new local variable
      llvm::AllocaInst *Alloca = Builder->CreateAlloca(
          llvm::Type::getInt32Ty(*Context), nullptr, VR->getName());

      // Add to symbol table
      NamedValues[VR->getName().str()] = Alloca;

      // No need to generate any other code for the declaration
      return;
    }
  }

  // For regular expressions, generate the code
  generateExpr(ES->getExpr());
}

llvm::Value *CodeGenerator::generateExpr(Expr *E) {
  switch (E->getKind()) {
  case Expr::EK_IntegerLiteral:
    return generateIntegerLiteral(llvm::cast<IntegerLiteral>(E));
  case Expr::EK_FloatLiteral:
    return generateFloatLiteral(llvm::cast<FloatLiteral>(E));
  case Expr::EK_VarRef:
    return generateVarRefExpr(llvm::cast<VarRefExpr>(E));
  case Expr::EK_Binary:
    return generateBinaryExpr(llvm::cast<BinaryExpr>(E));
  case Expr::EK_Unary:
    return generateUnaryExpr(llvm::cast<UnaryExpr>(E));
  case Expr::EK_Call:
    return generateCallExpr(llvm::cast<CallExpr>(E));
  }

  return nullptr;
}

llvm::Value *CodeGenerator::generateIntegerLiteral(IntegerLiteral *IL) {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),
                                IL->getValue().getZExtValue());
}

llvm::Value *CodeGenerator::generateFloatLiteral(FloatLiteral *FL) {
  return llvm::ConstantFP::get(llvm::Type::getFloatTy(*Context),
                               FL->getValue().convertToFloat());
}

llvm::Value *CodeGenerator::generateVarRefExpr(VarRefExpr *VR) {
  llvm::Value *V = NamedValues[VR->getName().str()];
  if (!V) {
    Diags.report(VR->getLocation(), diag::unknown_identifier, VR->getName());
    return nullptr;
  }

  // If it's an alloca (local variable), load its value
  if (llvm::AllocaInst *AI = llvm::dyn_cast<llvm::AllocaInst>(V)) {
    return Builder->CreateLoad(AI->getAllocatedType(), AI, VR->getName());
  }

  return V;
}

llvm::Value *CodeGenerator::generateBinaryExpr(BinaryExpr *BE) {
  // Special case for assignment
  if (BE->getOpcode() == BinaryExpr::BO_Eq) {
    // The left side must be a variable reference
    auto *LHS = llvm::dyn_cast<VarRefExpr>(BE->getLeft());
    if (!LHS) {
      Diags.report(BE->getLocation(), diag::err_expected, "lvalue",
                   "expression");
      return nullptr;
    }

    // Generate code for the right hand side
    llvm::Value *RHS = generateExpr(BE->getRight());
    if (!RHS)
      return nullptr;

    // Look up the variable
    llvm::Value *Variable = NamedValues[LHS->getName().str()];
    if (!Variable) {
      Diags.report(LHS->getLocation(), diag::unknown_identifier,
                   LHS->getName());
      return nullptr;
    }

    // Store the value
    Builder->CreateStore(RHS, Variable);

    // Return the value we just stored
    return RHS;
  }

  // Normal binary expression
  llvm::Value *L = generateExpr(BE->getLeft());
  llvm::Value *R = generateExpr(BE->getRight());

  if (!L || !R)
    return nullptr;

  // Check if we're dealing with float operands
  bool IsFloat = L->getType()->isFloatTy() || R->getType()->isFloatTy();

  // If mixed types (int and float), convert int to float
  if (IsFloat) {
    if (L->getType()->isIntegerTy()) {
      L = Builder->CreateSIToFP(L, llvm::Type::getFloatTy(*Context));
    }
    if (R->getType()->isIntegerTy()) {
      R = Builder->CreateSIToFP(R, llvm::Type::getFloatTy(*Context));
    }
  }

  switch (BE->getOpcode()) {
  case BinaryExpr::BO_Add:
    return IsFloat ? Builder->CreateFAdd(L, R) : Builder->CreateAdd(L, R);
  case BinaryExpr::BO_Sub:
    return IsFloat ? Builder->CreateFSub(L, R) : Builder->CreateSub(L, R);
  case BinaryExpr::BO_Mul:
    return IsFloat ? Builder->CreateFMul(L, R) : Builder->CreateMul(L, R);
  case BinaryExpr::BO_Div:
    return IsFloat ? Builder->CreateFDiv(L, R) : Builder->CreateSDiv(L, R);
  case BinaryExpr::BO_Lt:
    if (IsFloat) {
      L = Builder->CreateFCmpOLT(L, R);
    } else {
      L = Builder->CreateICmpSLT(L, R);
    }
    // Convert i1 to i32
    return Builder->CreateZExt(L, llvm::Type::getInt32Ty(*Context));
  case BinaryExpr::BO_Gt:
    if (IsFloat) {
      L = Builder->CreateFCmpOGT(L, R);
    } else {
      L = Builder->CreateICmpSGT(L, R);
    }
    // Convert i1 to i32
    return Builder->CreateZExt(L, llvm::Type::getInt32Ty(*Context));
  case BinaryExpr::BO_Eq:
    // This should never happen, as we handle assignments above
    if (IsFloat) {
      L = Builder->CreateFCmpOEQ(L, R);
    } else {
      L = Builder->CreateICmpEQ(L, R);
    }
    // Convert i1 to i32
    return Builder->CreateZExt(L, llvm::Type::getInt32Ty(*Context));
  }

  return nullptr;
}

llvm::Value *CodeGenerator::generateUnaryExpr(UnaryExpr *UE) {
  llvm::Value *SubV = generateExpr(UE->getSubExpr());
  if (!SubV)
    return nullptr;

  // Check if we're dealing with a float operand
  bool IsFloat = SubV->getType()->isFloatTy();

  switch (UE->getOpcode()) {
  case UnaryExpr::UO_Minus:
    return IsFloat ? Builder->CreateFNeg(SubV) : Builder->CreateNeg(SubV);
  case UnaryExpr::UO_Not:
    // For float operands, compare with 0.0
    if (IsFloat) {
      // First compare with 0.0 to get i1
      SubV = Builder->CreateFCmpOEQ(
          SubV, llvm::ConstantFP::get(llvm::Type::getFloatTy(*Context), 0.0),
          "");
    } else {
      // First compare with 0 to get i1
      SubV = Builder->CreateICmpEQ(
          SubV, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0),
          "");
    }
    // Convert i1 to i32
    return Builder->CreateZExt(SubV, llvm::Type::getInt32Ty(*Context));
  }

  return nullptr;
}

llvm::Value *CodeGenerator::generateCallExpr(CallExpr *CE) {
  // Look up the function in the module
  llvm::Function *CalleeF = TheModule->getFunction(CE->getCallee());
  if (!CalleeF) {
    Diags.report(CE->getLocation(), diag::unknown_identifier, CE->getCallee());
    return nullptr;
  }

  // Check argument count
  if (CalleeF->arg_size() != CE->getArgs().size()) {
    Diags.report(CE->getLocation(), diag::err_argument_count_mismatch,
                 CE->getCallee(), CalleeF->arg_size(), CE->getArgs().size());
    return nullptr;
  }

  // Generate code for arguments
  std::vector<llvm::Value *> ArgsV;
  for (auto *Arg : CE->getArgs()) {
    llvm::Value *ArgV = generateExpr(Arg);
    if (!ArgV)
      return nullptr;
    ArgsV.push_back(ArgV);
  }

  return Builder->CreateCall(CalleeF, ArgsV);
}
