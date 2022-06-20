
// Generated from SysY.g4 by ANTLR 4.10.1

#pragma once


#include "antlr4-runtime.h"




class  SysYLexer : public antlr4::Lexer {
public:
  enum {
    T__0 = 1, Int = 2, Float = 3, Void = 4, Const = 5, Return = 6, If = 7, 
    Else = 8, For = 9, While = 10, Do = 11, Break = 12, Continue = 13, Lparen = 14, 
    Rparen = 15, Lbrkt = 16, Rbrkt = 17, Lbrace = 18, Rbrace = 19, Comma = 20, 
    Semicolon = 21, Question = 22, Colon = 23, Minus = 24, Exclamation = 25, 
    Tilde = 26, Addition = 27, Multiplication = 28, Division = 29, Modulo = 30, 
    LAND = 31, LOR = 32, EQ = 33, NEQ = 34, LT = 35, LE = 36, GT = 37, GE = 38, 
    IntLiteral = 39, HexadecimalDigits = 40, ExponentPart = 41, FractionPart = 42, 
    FloatLiteral = 43, Identifier = 44, STRING = 45, WS = 46, LINE_COMMENT = 47, 
    COMMENT = 48
  };

  explicit SysYLexer(antlr4::CharStream *input);

  ~SysYLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

