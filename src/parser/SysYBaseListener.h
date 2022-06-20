
// Generated from SysY.g4 by ANTLR 4.10.1

#pragma once


#include "antlr4-runtime.h"
#include "SysYListener.h"


/**
 * This class provides an empty implementation of SysYListener,
 * which can be extended to create a listener which only needs to handle a subset
 * of the available methods.
 */
class  SysYBaseListener : public SysYListener {
public:

  virtual void enterCompUnit(SysYParser::CompUnitContext * /*ctx*/) override { }
  virtual void exitCompUnit(SysYParser::CompUnitContext * /*ctx*/) override { }

  virtual void enterDecl(SysYParser::DeclContext * /*ctx*/) override { }
  virtual void exitDecl(SysYParser::DeclContext * /*ctx*/) override { }

  virtual void enterConstDecl(SysYParser::ConstDeclContext * /*ctx*/) override { }
  virtual void exitConstDecl(SysYParser::ConstDeclContext * /*ctx*/) override { }

  virtual void enterBType(SysYParser::BTypeContext * /*ctx*/) override { }
  virtual void exitBType(SysYParser::BTypeContext * /*ctx*/) override { }

  virtual void enterConstDef(SysYParser::ConstDefContext * /*ctx*/) override { }
  virtual void exitConstDef(SysYParser::ConstDefContext * /*ctx*/) override { }

  virtual void enterScalarConstInitVal(SysYParser::ScalarConstInitValContext * /*ctx*/) override { }
  virtual void exitScalarConstInitVal(SysYParser::ScalarConstInitValContext * /*ctx*/) override { }

  virtual void enterListConstInitVal(SysYParser::ListConstInitValContext * /*ctx*/) override { }
  virtual void exitListConstInitVal(SysYParser::ListConstInitValContext * /*ctx*/) override { }

  virtual void enterVarDecl(SysYParser::VarDeclContext * /*ctx*/) override { }
  virtual void exitVarDecl(SysYParser::VarDeclContext * /*ctx*/) override { }

  virtual void enterUninitVarDef(SysYParser::UninitVarDefContext * /*ctx*/) override { }
  virtual void exitUninitVarDef(SysYParser::UninitVarDefContext * /*ctx*/) override { }

  virtual void enterInitVarDef(SysYParser::InitVarDefContext * /*ctx*/) override { }
  virtual void exitInitVarDef(SysYParser::InitVarDefContext * /*ctx*/) override { }

  virtual void enterScalarInitVal(SysYParser::ScalarInitValContext * /*ctx*/) override { }
  virtual void exitScalarInitVal(SysYParser::ScalarInitValContext * /*ctx*/) override { }

  virtual void enterListInitval(SysYParser::ListInitvalContext * /*ctx*/) override { }
  virtual void exitListInitval(SysYParser::ListInitvalContext * /*ctx*/) override { }

  virtual void enterFuncDef(SysYParser::FuncDefContext * /*ctx*/) override { }
  virtual void exitFuncDef(SysYParser::FuncDefContext * /*ctx*/) override { }

  virtual void enterFuncType(SysYParser::FuncTypeContext * /*ctx*/) override { }
  virtual void exitFuncType(SysYParser::FuncTypeContext * /*ctx*/) override { }

  virtual void enterFuncFParams(SysYParser::FuncFParamsContext * /*ctx*/) override { }
  virtual void exitFuncFParams(SysYParser::FuncFParamsContext * /*ctx*/) override { }

  virtual void enterFuncFParam(SysYParser::FuncFParamContext * /*ctx*/) override { }
  virtual void exitFuncFParam(SysYParser::FuncFParamContext * /*ctx*/) override { }

  virtual void enterBlock(SysYParser::BlockContext * /*ctx*/) override { }
  virtual void exitBlock(SysYParser::BlockContext * /*ctx*/) override { }

  virtual void enterBlockItem(SysYParser::BlockItemContext * /*ctx*/) override { }
  virtual void exitBlockItem(SysYParser::BlockItemContext * /*ctx*/) override { }

  virtual void enterAssignment(SysYParser::AssignmentContext * /*ctx*/) override { }
  virtual void exitAssignment(SysYParser::AssignmentContext * /*ctx*/) override { }

  virtual void enterExpStmt(SysYParser::ExpStmtContext * /*ctx*/) override { }
  virtual void exitExpStmt(SysYParser::ExpStmtContext * /*ctx*/) override { }

  virtual void enterBlockStmt(SysYParser::BlockStmtContext * /*ctx*/) override { }
  virtual void exitBlockStmt(SysYParser::BlockStmtContext * /*ctx*/) override { }

  virtual void enterIfStmt1(SysYParser::IfStmt1Context * /*ctx*/) override { }
  virtual void exitIfStmt1(SysYParser::IfStmt1Context * /*ctx*/) override { }

  virtual void enterIfStmt2(SysYParser::IfStmt2Context * /*ctx*/) override { }
  virtual void exitIfStmt2(SysYParser::IfStmt2Context * /*ctx*/) override { }

  virtual void enterWhileStmt(SysYParser::WhileStmtContext * /*ctx*/) override { }
  virtual void exitWhileStmt(SysYParser::WhileStmtContext * /*ctx*/) override { }

  virtual void enterBreakStmt(SysYParser::BreakStmtContext * /*ctx*/) override { }
  virtual void exitBreakStmt(SysYParser::BreakStmtContext * /*ctx*/) override { }

  virtual void enterContinueStmt(SysYParser::ContinueStmtContext * /*ctx*/) override { }
  virtual void exitContinueStmt(SysYParser::ContinueStmtContext * /*ctx*/) override { }

  virtual void enterReturnStmt(SysYParser::ReturnStmtContext * /*ctx*/) override { }
  virtual void exitReturnStmt(SysYParser::ReturnStmtContext * /*ctx*/) override { }

  virtual void enterExp(SysYParser::ExpContext * /*ctx*/) override { }
  virtual void exitExp(SysYParser::ExpContext * /*ctx*/) override { }

  virtual void enterCond(SysYParser::CondContext * /*ctx*/) override { }
  virtual void exitCond(SysYParser::CondContext * /*ctx*/) override { }

  virtual void enterLVal(SysYParser::LValContext * /*ctx*/) override { }
  virtual void exitLVal(SysYParser::LValContext * /*ctx*/) override { }

  virtual void enterPrimaryExp1(SysYParser::PrimaryExp1Context * /*ctx*/) override { }
  virtual void exitPrimaryExp1(SysYParser::PrimaryExp1Context * /*ctx*/) override { }

  virtual void enterPrimaryExp2(SysYParser::PrimaryExp2Context * /*ctx*/) override { }
  virtual void exitPrimaryExp2(SysYParser::PrimaryExp2Context * /*ctx*/) override { }

  virtual void enterPrimaryExp3(SysYParser::PrimaryExp3Context * /*ctx*/) override { }
  virtual void exitPrimaryExp3(SysYParser::PrimaryExp3Context * /*ctx*/) override { }

  virtual void enterNumber(SysYParser::NumberContext * /*ctx*/) override { }
  virtual void exitNumber(SysYParser::NumberContext * /*ctx*/) override { }

  virtual void enterUnary1(SysYParser::Unary1Context * /*ctx*/) override { }
  virtual void exitUnary1(SysYParser::Unary1Context * /*ctx*/) override { }

  virtual void enterUnary2(SysYParser::Unary2Context * /*ctx*/) override { }
  virtual void exitUnary2(SysYParser::Unary2Context * /*ctx*/) override { }

  virtual void enterUnary3(SysYParser::Unary3Context * /*ctx*/) override { }
  virtual void exitUnary3(SysYParser::Unary3Context * /*ctx*/) override { }

  virtual void enterUnaryOp(SysYParser::UnaryOpContext * /*ctx*/) override { }
  virtual void exitUnaryOp(SysYParser::UnaryOpContext * /*ctx*/) override { }

  virtual void enterFuncRParams(SysYParser::FuncRParamsContext * /*ctx*/) override { }
  virtual void exitFuncRParams(SysYParser::FuncRParamsContext * /*ctx*/) override { }

  virtual void enterExpAsRParam(SysYParser::ExpAsRParamContext * /*ctx*/) override { }
  virtual void exitExpAsRParam(SysYParser::ExpAsRParamContext * /*ctx*/) override { }

  virtual void enterStringAsRParam(SysYParser::StringAsRParamContext * /*ctx*/) override { }
  virtual void exitStringAsRParam(SysYParser::StringAsRParamContext * /*ctx*/) override { }

  virtual void enterMul2(SysYParser::Mul2Context * /*ctx*/) override { }
  virtual void exitMul2(SysYParser::Mul2Context * /*ctx*/) override { }

  virtual void enterMul1(SysYParser::Mul1Context * /*ctx*/) override { }
  virtual void exitMul1(SysYParser::Mul1Context * /*ctx*/) override { }

  virtual void enterAdd2(SysYParser::Add2Context * /*ctx*/) override { }
  virtual void exitAdd2(SysYParser::Add2Context * /*ctx*/) override { }

  virtual void enterAdd1(SysYParser::Add1Context * /*ctx*/) override { }
  virtual void exitAdd1(SysYParser::Add1Context * /*ctx*/) override { }

  virtual void enterRel2(SysYParser::Rel2Context * /*ctx*/) override { }
  virtual void exitRel2(SysYParser::Rel2Context * /*ctx*/) override { }

  virtual void enterRel1(SysYParser::Rel1Context * /*ctx*/) override { }
  virtual void exitRel1(SysYParser::Rel1Context * /*ctx*/) override { }

  virtual void enterEq1(SysYParser::Eq1Context * /*ctx*/) override { }
  virtual void exitEq1(SysYParser::Eq1Context * /*ctx*/) override { }

  virtual void enterEq2(SysYParser::Eq2Context * /*ctx*/) override { }
  virtual void exitEq2(SysYParser::Eq2Context * /*ctx*/) override { }

  virtual void enterLAnd2(SysYParser::LAnd2Context * /*ctx*/) override { }
  virtual void exitLAnd2(SysYParser::LAnd2Context * /*ctx*/) override { }

  virtual void enterLAnd1(SysYParser::LAnd1Context * /*ctx*/) override { }
  virtual void exitLAnd1(SysYParser::LAnd1Context * /*ctx*/) override { }

  virtual void enterLOr1(SysYParser::LOr1Context * /*ctx*/) override { }
  virtual void exitLOr1(SysYParser::LOr1Context * /*ctx*/) override { }

  virtual void enterLOr2(SysYParser::LOr2Context * /*ctx*/) override { }
  virtual void exitLOr2(SysYParser::LOr2Context * /*ctx*/) override { }

  virtual void enterConstExp(SysYParser::ConstExpContext * /*ctx*/) override { }
  virtual void exitConstExp(SysYParser::ConstExpContext * /*ctx*/) override { }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

};

