#include "Lexer.h"
#include <iostream>
#include <iomanip>
#include <string>

using namespace tinycc;

void testLexer(const std::string& input, const std::string& description) {
  std::cout << "=== Test: " << description << " ===" << std::endl;
  std::cout << "Input: \"" << input << "\"" << std::endl;

  Lexer lexer(input);
  std::vector<Token> tokens = lexer.getAllTokens();

  std::cout << "Tokens:" << std::endl;
  for (const auto& token : tokens) {
    std::cout << "  " << std::left << std::setw(20) << Lexer::getTokenString(token) << std::endl;
  }
  std::cout << std::endl;
}

int main() {
  // 测试基本标记
  testLexer("a b c", "Basic identifiers");
  testLexer("123 456 789", "Basic numbers");
  testLexer("+ - * / = # , . ; : () < > <= >=", "Basic operators");

  // 测试字符串
  testLexer("\"hello world\" 'c'", "Strings");

  // 测试注释
  testLexer("(* This is a comment *) code", "Oberon-style comments");
  testLexer("// This is a line comment\ncode", "C-style line comments");
  testLexer("/* This is a block comment */code", "C-style block comments");
  testLexer("/* This comment /* has nested */ parts */code", "Nested comments");

  // 测试混合代码
  testLexer("int main() {\n  printf(\"Hello, world!\");\n  return 0;\n", "C code");

  return 0;
}
