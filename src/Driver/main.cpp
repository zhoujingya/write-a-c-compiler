#include "Lexer/Lexer.h"
#include <llvm/Support/CommandLine.h>
using namespace tinycc;
using namespace llvm;

static cl::opt<bool> enableLexer("lex", cl::desc("Enable lexer"),
                                 cl::init(false),
                                 cl::value_desc("enable or not"));

static cl::opt<std::string> inputFile(cl::Positional, cl::desc("<Input file>"),
                                      cl::init("-"),
                                      cl::value_desc("Input file path"));

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "tinycc driver\n");
  SourceMgr SrcMgr;
  DiagnosticsEngine Diags(SrcMgr);
  // Tell SrcMgr about this buffer, which is what the
  // parser will pick up.
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileOrErr =
      llvm::MemoryBuffer::getFile(inputFile);
  SrcMgr.AddNewSourceBuffer(std::move(*FileOrErr), llvm::SMLoc());
  Lexer lexer(SrcMgr, Diags);
  // print file content
  if (enableLexer) {
    LexerDriver driver(lexer);
    driver.run();
  }
  return 0;
}
