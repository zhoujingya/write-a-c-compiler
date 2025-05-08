add_library(tinyccCodeGen
  STATIC
  CodeGen.cpp
)

target_link_libraries(tinyccCodeGen
  PRIVATE
  tinyccAST
  tinyccSupport
  LLVMCore
  LLVMSupport
  LLVMAnalysis
)
