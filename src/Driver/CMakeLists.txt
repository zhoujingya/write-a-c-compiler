add_executable(tinycc
    main.cpp
)

target_link_libraries(tinycc
    PRIVATE tinyccLexer tinyccParser tinyccCodeGen LLVMSupport LLVMCore)
