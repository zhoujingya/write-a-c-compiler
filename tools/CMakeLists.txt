set(LLVM_LINK_COMPONENTS
  Support
)

add_executable(LexerTest
  LexerTest.cpp
)

target_link_libraries(LexerTest
  tinyccLexer
  tinyccSupport
)

install(TARGETS LexerTest
  RUNTIME DESTINATION bin
)
