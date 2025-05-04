#include "AST.h"
#include "Diagnostic.h"
#include "Lexer.h"
#include "Parser.h"
#include "Sema.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace tinycc;
namespace cl = llvm::cl;

static cl::opt<std::string> InputFile(cl::Positional, cl::desc("<input file>"),
                                      cl::Required);

static cl::opt<bool> DumpTokens("dump-tokens",
                                cl::desc("Dump tokens from lexer"));

static cl::opt<bool> Debug("debug",
                           cl::desc("Enable debug output during parsing"));


int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "A simple C compiler driver\n");

  // Set up source manager and diagnostics
  SourceMgr SrcMgr;
  DiagnosticsEngine Diags(SrcMgr);

  // Load input file
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileOrErr =
      llvm::MemoryBuffer::getFile(InputFile);
  if (std::error_code EC = FileOrErr.getError()) {
    llvm::errs() << "Error opening file: " << EC.message() << '\n';
    return 1;
  }

  // Load the file into the source manager
  unsigned FileID =
      SrcMgr.AddNewSourceBuffer(std::move(FileOrErr.get()), llvm::SMLoc());

  // Initialize lexer
  Lexer Lex(SrcMgr, Diags);

  // Initialize semantic analyzer
  Sema Actions(Diags);

  // Initialize parser with timeout
  Parser Parser(Lex, Actions);

  // Parse the input file
  ModuleDeclaration *AST = Parser.parse();

  if (!AST) {
    llvm::errs() << "Parsing failed!\n";
    return 1;
  }

  llvm::outs() << "Parsing completed successfully.\n";

  // Print the AST (or you could continue with code generation)
  // This is just a placeholder - you might want to implement a proper AST
  // printer
  llvm::outs() << "Parsed file: " << InputFile << "\n";

  return 0;
}
