## Basic introduction

### Q1 - why writing in c++

* Answer: I an working in a GPGPU company's compiler team, I am just familiar with cpp but not an expert

### Q2 - why choose llvm

* Answer: LLVM provides many useful libraries, I think we'd better use tools provided by great teams

### Q3 - where is the compiler entry point

* Answer: [driver](src/Driver/main.cpp) is here

## How to build

`cmake -B build -GNinja`

* set `ENABLE_TESTING` equals `ON` to enable unit test, default `OFF`.

If above cmake command reports error, you just need to fix, mostly it may be related to llvm(installation path etc.)

## How to test

Just follow [this repository](https://github.com/nlsandler/writing-a-c-compiler-tests/blob/main/README.md) and its PDF


## Chapter1 - lexer/parser/codegen

```
./test_compiler [this repo path]/build/bin/tinycc --chapter 1 --stage lex/parse/codegen`

```




