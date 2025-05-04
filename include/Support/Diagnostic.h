#ifndef TINYCC_BASIC_DIAGNOSTIC_H
#define TINYCC_BASIC_DIAGNOSTIC_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <utility>

using namespace llvm;

namespace tinycc {

namespace diag {
enum {
#define DIAG(ID, Level, Msg) ID,
#include "Support/Diagnostic.def"
};
} // namespace diag

class DiagnosticsEngine {

  SourceMgr &SrcMgr;
  unsigned NumErrors;

public:
  DiagnosticsEngine(llvm::SourceMgr &SrcMgr) : SrcMgr(SrcMgr), NumErrors(0) {}

  unsigned numErrors() { return NumErrors; }
  static const char *getDiagnosticText(unsigned DiagID);
  static SourceMgr::DiagKind getDiagnosticKind(unsigned DiagID);
  template <typename... Args>
  void report(SMLoc Loc, unsigned DiagID, Args &&...Arguments) {
    std::string Msg = llvm::formatv(getDiagnosticText(DiagID),
                                    std::forward<Args>(Arguments)...)
                          .str();
    SourceMgr::DiagKind Kind = getDiagnosticKind(DiagID);
    SrcMgr.PrintMessage(Loc, Kind, Msg);
    NumErrors += (Kind == SourceMgr::DK_Error);
    exit(1);
  }
};

} // namespace tinycc

#endif
