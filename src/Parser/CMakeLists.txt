add_library(tinyccParser
    SHARED
    Parser.cpp
)

target_link_libraries(tinyccParser
    PRIVATE tinyccSupport
    PRIVATE tinyccLexer)

