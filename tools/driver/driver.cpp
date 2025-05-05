#include "AST/AST.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Support/Diagnostic.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>

using namespace tinycc;
using namespace llvm;

static cl::opt<std::string> InputFile(cl::Positional,
                                     cl::desc("<input file>"),
                                     cl::init("-"));

int main(int argc, const char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "tinyc-compiler\n");

  // Create source manager and diagnostics
  SourceMgr SrcMgr;
  DiagnosticsEngine Diags(SrcMgr);

  // Read the input file
  ErrorOr<std::unique_ptr<MemoryBuffer>> FileOrErr =
      MemoryBuffer::getFileOrSTDIN(InputFile);
  if (std::error_code EC = FileOrErr.getError()) {
    errs() << "Error reading file: " << EC.message() << "\n";
    return 1;
  }

  // Add file to source manager
  unsigned BufferID = SrcMgr.AddNewSourceBuffer(std::move(FileOrErr.get()), SMLoc());

  // Create lexer
  Lexer Lex(SrcMgr, Diags);

  // Create parser
  Parser P(Lex, Diags);

  // Parse top-level declarations
  auto Decls = P.parse();

  // Check if there were errors
  if (Diags.numErrors() > 0) {
    errs() << "Parsing failed\n";
    return 1;
  }

  // Print success message
  outs() << "Successfully parsed " << Decls.size() << " declarations\n";

  return 0;
}
