
// Generated from SysY.g4 by ANTLR 4.10.1

#pragma once


#include "antlr4-runtime.h"
#include "SysYParser.h"


/**
 * This interface defines an abstract listener for a parse tree produced by SysYParser.
 */
class  SysYListener : public antlr4::tree::ParseTreeListener {
public:

  virtual void enterCompUnit(SysYParser::CompUnitContext *ctx) = 0;
  virtual void exitCompUnit(SysYParser::CompUnitContext *ctx) = 0;

  virtual void enterDecl(SysYParser::DeclContext *ctx) = 0;
  virtual void exitDecl(SysYParser::DeclContext *ctx) = 0;

  virtual void enterConstDecl(SysYParser::ConstDeclContext *ctx) = 0;
  virtual void exitConstDecl(SysYParser::ConstDeclContext *ctx) = 0;

  virtual void enterBType(SysYParser::BTypeContext *ctx) = 0;
  virtual void exitBType(SysYParser::BTypeContext *ctx) = 0;

  virtual void enterConstDef(SysYParser::ConstDefContext *ctx) = 0;
  virtual void exitConstDef(SysYParser::ConstDefContext *ctx) = 0;

  virtual void enterScalarConstInitVal(SysYParser::ScalarConstInitValContext *ctx) = 0;
  virtual void exitScalarConstInitVal(SysYParser::ScalarConstInitValContext *ctx) = 0;

  virtual void enterListConstInitVal(SysYParser::ListConstInitValContext *ctx) = 0;
  virtual void exitListConstInitVal(SysYParser::ListConstInitValContext *ctx) = 0;

  virtual void enterVarDecl(SysYParser::VarDeclContext *ctx) = 0;
  virtual void exitVarDecl(SysYParser::VarDeclContext *ctx) = 0;

  virtual void enterUninitVarDef(SysYParser::UninitVarDefContext *ctx) = 0;
  virtual void exitUninitVarDef(SysYParser::UninitVarDefContext *ctx) = 0;

  virtual void enterInitVarDef(SysYParser::InitVarDefContext *ctx) = 0;
  virtual void exitInitVarDef(SysYParser::InitVarDefContext *ctx) = 0;

  virtual void enterScalarInitVal(SysYParser::ScalarInitValContext *ctx) = 0;
  virtual void exitScalarInitVal(SysYParser::ScalarInitValContext *ctx) = 0;

  virtual void enterListInitval(SysYParser::ListInitvalContext *ctx) = 0;
  virtual void exitListInitval(SysYParser::ListInitvalContext *ctx) = 0;

  virtual void enterFuncDef(SysYParser::FuncDefContext *ctx) = 0;
  virtual void exitFuncDef(SysYParser::FuncDefContext *ctx) = 0;

  virtual void enterFuncType(SysYParser::FuncTypeContext *ctx) = 0;
  virtual void exitFuncType(SysYParser::FuncTypeContext *ctx) = 0;

  virtual void enterFuncFParams(SysYParser::FuncFParamsContext *ctx) = 0;
  virtual void exitFuncFParams(SysYParser::FuncFParamsContext *ctx) = 0;

  virtual void enterFuncFParam(SysYParser::FuncFParamContext *ctx) = 0;
  virtual void exitFuncFParam(SysYParser::FuncFParamContext *ctx) = 0;

  virtual void enterBlock(SysYParser::BlockContext *ctx) = 0;
  virtual void exitBlock(SysYParser::BlockContext *ctx) = 0;

  virtual void enterBlockItem(SysYParser::BlockItemContext *ctx) = 0;
  virtual void exitBlockItem(SysYParser::BlockItemContext *ctx) = 0;

  virtual void enterAssignment(SysYParser::AssignmentContext *ctx) = 0;
  virtual void exitAssignment(SysYParser::AssignmentContext *ctx) = 0;

  virtual void enterExpStmt(SysYParser::ExpStmtContext *ctx) = 0;
  virtual void exitExpStmt(SysYParser::ExpStmtContext *ctx) = 0;

  virtual void enterBlockStmt(SysYParser::BlockStmtContext *ctx) = 0;
  virtual void exitBlockStmt(SysYParser::BlockStmtContext *ctx) = 0;

  virtual void enterIfStmt1(SysYParser::IfStmt1Context *ctx) = 0;
  virtual void exitIfStmt1(SysYParser::IfStmt1Context *ctx) = 0;

  virtual void enterIfStmt2(SysYParser::IfStmt2Context *ctx) = 0;
  virtual void exitIfStmt2(SysYParser::IfStmt2Context *ctx) = 0;

  virtual void enterWhileStmt(SysYParser::WhileStmtContext *ctx) = 0;
  virtual void exitWhileStmt(SysYParser::WhileStmtContext *ctx) = 0;

  virtual void enterBreakStmt(SysYParser::BreakStmtContext *ctx) = 0;
  virtual void exitBreakStmt(SysYParser::BreakStmtContext *ctx) = 0;

  virtual void enterContinueStmt(SysYParser::ContinueStmtContext *ctx) = 0;
  virtual void exitContinueStmt(SysYParser::ContinueStmtContext *ctx) = 0;

  virtual void enterReturnStmt(SysYParser::ReturnStmtContext *ctx) = 0;
  virtual void exitReturnStmt(SysYParser::ReturnStmtContext *ctx) = 0;

  virtual void enterExp(SysYParser::ExpContext *ctx) = 0;
  virtual void exitExp(SysYParser::ExpContext *ctx) = 0;

  virtual void enterCond(SysYParser::CondContext *ctx) = 0;
  virtual void exitCond(SysYParser::CondContext *ctx) = 0;

  virtual void enterLVal(SysYParser::LValContext *ctx) = 0;
  virtual void exitLVal(SysYParser::LValContext *ctx) = 0;

  virtual void enterPrimaryExp1(SysYParser::PrimaryExp1Context *ctx) = 0;
  virtual void exitPrimaryExp1(SysYParser::PrimaryExp1Context *ctx) = 0;

  virtual void enterPrimaryExp2(SysYParser::PrimaryExp2Context *ctx) = 0;
  virtual void exitPrimaryExp2(SysYParser::PrimaryExp2Context *ctx) = 0;

  virtual void enterPrimaryExp3(SysYParser::PrimaryExp3Context *ctx) = 0;
  virtual void exitPrimaryExp3(SysYParser::PrimaryExp3Context *ctx) = 0;

  virtual void enterNumber(SysYParser::NumberContext *ctx) = 0;
  virtual void exitNumber(SysYParser::NumberContext *ctx) = 0;

  virtual void enterUnary1(SysYParser::Unary1Context *ctx) = 0;
  virtual void exitUnary1(SysYParser::Unary1Context *ctx) = 0;

  virtual void enterUnary2(SysYParser::Unary2Context *ctx) = 0;
  virtual void exitUnary2(SysYParser::Unary2Context *ctx) = 0;

  virtual void enterUnary3(SysYParser::Unary3Context *ctx) = 0;
  virtual void exitUnary3(SysYParser::Unary3Context *ctx) = 0;

  virtual void enterUnaryOp(SysYParser::UnaryOpContext *ctx) = 0;
  virtual void exitUnaryOp(SysYParser::UnaryOpContext *ctx) = 0;

  virtual void enterFuncRParams(SysYParser::FuncRParamsContext *ctx) = 0;
  virtual void exitFuncRParams(SysYParser::FuncRParamsContext *ctx) = 0;

  virtual void enterExpAsRParam(SysYParser::ExpAsRParamContext *ctx) = 0;
  virtual void exitExpAsRParam(SysYParser::ExpAsRParamContext *ctx) = 0;

  virtual void enterStringAsRParam(SysYParser::StringAsRParamContext *ctx) = 0;
  virtual void exitStringAsRParam(SysYParser::StringAsRParamContext *ctx) = 0;

  virtual void enterMul2(SysYParser::Mul2Context *ctx) = 0;
  virtual void exitMul2(SysYParser::Mul2Context *ctx) = 0;

  virtual void enterMul1(SysYParser::Mul1Context *ctx) = 0;
  virtual void exitMul1(SysYParser::Mul1Context *ctx) = 0;

  virtual void enterAdd2(SysYParser::Add2Context *ctx) = 0;
  virtual void exitAdd2(SysYParser::Add2Context *ctx) = 0;

  virtual void enterAdd1(SysYParser::Add1Context *ctx) = 0;
  virtual void exitAdd1(SysYParser::Add1Context *ctx) = 0;

  virtual void enterRel2(SysYParser::Rel2Context *ctx) = 0;
  virtual void exitRel2(SysYParser::Rel2Context *ctx) = 0;

  virtual void enterRel1(SysYParser::Rel1Context *ctx) = 0;
  virtual void exitRel1(SysYParser::Rel1Context *ctx) = 0;

  virtual void enterEq1(SysYParser::Eq1Context *ctx) = 0;
  virtual void exitEq1(SysYParser::Eq1Context *ctx) = 0;

  virtual void enterEq2(SysYParser::Eq2Context *ctx) = 0;
  virtual void exitEq2(SysYParser::Eq2Context *ctx) = 0;

  virtual void enterLAnd2(SysYParser::LAnd2Context *ctx) = 0;
  virtual void exitLAnd2(SysYParser::LAnd2Context *ctx) = 0;

  virtual void enterLAnd1(SysYParser::LAnd1Context *ctx) = 0;
  virtual void exitLAnd1(SysYParser::LAnd1Context *ctx) = 0;

  virtual void enterLOr1(SysYParser::LOr1Context *ctx) = 0;
  virtual void exitLOr1(SysYParser::LOr1Context *ctx) = 0;

  virtual void enterLOr2(SysYParser::LOr2Context *ctx) = 0;
  virtual void exitLOr2(SysYParser::LOr2Context *ctx) = 0;

  virtual void enterConstExp(SysYParser::ConstExpContext *ctx) = 0;
  virtual void exitConstExp(SysYParser::ConstExpContext *ctx) = 0;


};

