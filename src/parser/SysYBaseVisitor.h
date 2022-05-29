
// Generated from SysY.g4 by ANTLR 4.10.1

#pragma once


#include "antlr4-runtime.h"
#include "SysYVisitor.h"


/**
 * This class provides an empty implementation of SysYVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  SysYBaseVisitor : public SysYVisitor {
public:

  virtual std::any visitCompUnit(SysYParser::CompUnitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDecl(SysYParser::DeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConstDecl(SysYParser::ConstDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBType(SysYParser::BTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConstDef(SysYParser::ConstDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScalarConstInitVal(SysYParser::ScalarConstInitValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitListConstInitVal(SysYParser::ListConstInitValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarDecl(SysYParser::VarDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUninitVarDef(SysYParser::UninitVarDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitVarDef(SysYParser::InitVarDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScalarInitVal(SysYParser::ScalarInitValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitListInitval(SysYParser::ListInitvalContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncDef(SysYParser::FuncDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncType(SysYParser::FuncTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncFParams(SysYParser::FuncFParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncFParam(SysYParser::FuncFParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(SysYParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockItem(SysYParser::BlockItemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignment(SysYParser::AssignmentContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpStmt(SysYParser::ExpStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockStmt(SysYParser::BlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt1(SysYParser::IfStmt1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt2(SysYParser::IfStmt2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStmt(SysYParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStmt(SysYParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStmt(SysYParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(SysYParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExp(SysYParser::ExpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCond(SysYParser::CondContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLVal(SysYParser::LValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrimaryExp1(SysYParser::PrimaryExp1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrimaryExp2(SysYParser::PrimaryExp2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrimaryExp3(SysYParser::PrimaryExp3Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNumber(SysYParser::NumberContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnary1(SysYParser::Unary1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnary2(SysYParser::Unary2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnary3(SysYParser::Unary3Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryOp(SysYParser::UnaryOpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncRParams(SysYParser::FuncRParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpAsRParam(SysYParser::ExpAsRParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStringAsRParam(SysYParser::StringAsRParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMul2(SysYParser::Mul2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMul1(SysYParser::Mul1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAdd2(SysYParser::Add2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAdd1(SysYParser::Add1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRel2(SysYParser::Rel2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRel1(SysYParser::Rel1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEq1(SysYParser::Eq1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEq2(SysYParser::Eq2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLAnd2(SysYParser::LAnd2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLAnd1(SysYParser::LAnd1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLOr1(SysYParser::LOr1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLOr2(SysYParser::LOr2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConstExp(SysYParser::ConstExpContext *ctx) override {
    return visitChildren(ctx);
  }


};

