#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "AST/AST.h"
#include "CodeGen/CodeGen.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace tinycc;
using namespace llvm;

static cl::opt<bool> enableLexer("lex", cl::desc("Enable lexer"),
                                 cl::init(false),
                                 cl::value_desc("enable or not"));

static cl::opt<bool> enableParser("parse", cl::desc("Enable parser"),
                                 cl::init(false),
                                 cl::value_desc("enable or not"));

static cl::opt<bool> enableCodeGen("codegen", cl::desc("Enable code generation"),
                                 cl::init(false),
                                 cl::value_desc("enable or not"));

static cl::opt<std::string> outputFile("o", cl::desc("Output file"),
                                      cl::init("output.ll"),
                                      cl::value_desc("Output file path"));

static cl::opt<std::string> inputFile(cl::Positional, cl::desc("<Input file>"),
                                      cl::init("-"),
                                      cl::value_desc("Input file path"));

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "tinycc driver\n");

  // Set up source manager and diagnostics
  SourceMgr SrcMgr;
  DiagnosticsEngine Diags(SrcMgr);

  // Read input file
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileOrErr =
      llvm::MemoryBuffer::getFile(inputFile);

  if (!FileOrErr) {
    errs() << "Error opening file '" << inputFile << "': "
           << FileOrErr.getError().message() << "\n";
    return 1;
  }

  SrcMgr.AddNewSourceBuffer(std::move(*FileOrErr), llvm::SMLoc());
  Lexer lexer(SrcMgr, Diags);

  // Run lexer if enabled
  if (enableLexer) {
    LexerDriver driver(lexer);
    driver.run();
    return 0;
  }

  // Run parser and optionally code generation
  if (enableParser || enableCodeGen) {
    ParserDriver parser(lexer, Diags);
    auto decls = parser.parse();

    // Check for parsing errors
    if (Diags.numErrors() > 0) {
      errs() << "Parsing failed with " << Diags.numErrors() << " errors.\n";
      return 1;
    }

    outs() << "Successfully parsed " << decls.size() << " declarations.\n";

    // Run code generation if enabled
    if (enableCodeGen) {
      // Generate LLVM IR
      CodeGenerator CodeGen(Diags);
      if (!CodeGen.generateCode(decls)) {
        errs() << "Code generation failed.\n";
        return 1;
      }

      // Write LLVM IR to output file
      std::error_code EC;
      raw_fd_ostream OS(outputFile, EC, sys::fs::OF_None);
      if (EC) {
        errs() << "Could not open output file: " << EC.message() << "\n";
        return 1;
      }

      CodeGen.print(OS);
      outs() << "Generated LLVM IR written to " << outputFile << "\n";
    }

    return 0;
  }

  // No action specified, show help
  errs() << "No action specified. Use --lex, --parse, or --codegen.\n";
  cl::PrintHelpMessage();
  return 1;
}
