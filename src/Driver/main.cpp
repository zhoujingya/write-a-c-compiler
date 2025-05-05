#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "AST/AST.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

using namespace tinycc;
using namespace llvm;

static cl::opt<bool> enableLexer("lex", cl::desc("Enable lexer"),
                                 cl::init(false),
                                 cl::value_desc("enable or not"));

static cl::opt<bool> enableParser("parse", cl::desc("Enable parser"),
                                 cl::init(false),
                                 cl::value_desc("enable or not"));

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

  // Run parser if enabled
  if (enableParser) {
    ParserDriver parser(lexer, Diags);
    auto decls = parser.parse();

    // Check for parsing errors
    if (Diags.numErrors() > 0) {
      errs() << "Parsing failed with " << Diags.numErrors() << " errors.\n";
      return 1;
    }

    // Success - print information about parsed declarations
    outs() << "Successfully parsed " << decls.size() << " declarations.\n";
    return 0;
  }

  // No action specified, show help
  errs() << "No action specified. Use --lex or --parse.\n";
  cl::PrintHelpMessage();
  return 1;
}
