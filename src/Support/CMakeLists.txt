add_library(tinyccSupport
    SHARED
    TokenKinds.cpp
    Diagnostic.cpp
)

target_link_libraries(tinyccSupport
    PRIVATE LLVMCore LLVMSupport)
