add_library(tinyccLexer
    SHARED
    Lexer.cpp
)

target_link_libraries(tinyccLexer
    PRIVATE tinyccSupport)

