
// Generated from SysY.g4 by ANTLR 4.10.1

#pragma once


#include "antlr4-runtime.h"
#include "SysYParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by SysYParser.
 */
class  SysYVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by SysYParser.
   */
    virtual std::any visitCompUnit(SysYParser::CompUnitContext *context) = 0;

    virtual std::any visitDecl(SysYParser::DeclContext *context) = 0;

    virtual std::any visitConstDecl(SysYParser::ConstDeclContext *context) = 0;

    virtual std::any visitBType(SysYParser::BTypeContext *context) = 0;

    virtual std::any visitConstDef(SysYParser::ConstDefContext *context) = 0;

    virtual std::any visitScalarConstInitVal(SysYParser::ScalarConstInitValContext *context) = 0;

    virtual std::any visitListConstInitVal(SysYParser::ListConstInitValContext *context) = 0;

    virtual std::any visitVarDecl(SysYParser::VarDeclContext *context) = 0;

    virtual std::any visitUninitVarDef(SysYParser::UninitVarDefContext *context) = 0;

    virtual std::any visitInitVarDef(SysYParser::InitVarDefContext *context) = 0;

    virtual std::any visitScalarInitVal(SysYParser::ScalarInitValContext *context) = 0;

    virtual std::any visitListInitval(SysYParser::ListInitvalContext *context) = 0;

    virtual std::any visitFuncDef(SysYParser::FuncDefContext *context) = 0;

    virtual std::any visitFuncType(SysYParser::FuncTypeContext *context) = 0;

    virtual std::any visitFuncFParams(SysYParser::FuncFParamsContext *context) = 0;

    virtual std::any visitFuncFParam(SysYParser::FuncFParamContext *context) = 0;

    virtual std::any visitBlock(SysYParser::BlockContext *context) = 0;

    virtual std::any visitBlockItem(SysYParser::BlockItemContext *context) = 0;

    virtual std::any visitAssignment(SysYParser::AssignmentContext *context) = 0;

    virtual std::any visitExpStmt(SysYParser::ExpStmtContext *context) = 0;

    virtual std::any visitBlockStmt(SysYParser::BlockStmtContext *context) = 0;

    virtual std::any visitIfStmt1(SysYParser::IfStmt1Context *context) = 0;

    virtual std::any visitIfStmt2(SysYParser::IfStmt2Context *context) = 0;

    virtual std::any visitWhileStmt(SysYParser::WhileStmtContext *context) = 0;

    virtual std::any visitBreakStmt(SysYParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(SysYParser::ContinueStmtContext *context) = 0;

    virtual std::any visitReturnStmt(SysYParser::ReturnStmtContext *context) = 0;

    virtual std::any visitExp(SysYParser::ExpContext *context) = 0;

    virtual std::any visitCond(SysYParser::CondContext *context) = 0;

    virtual std::any visitLVal(SysYParser::LValContext *context) = 0;

    virtual std::any visitPrimaryExp1(SysYParser::PrimaryExp1Context *context) = 0;

    virtual std::any visitPrimaryExp2(SysYParser::PrimaryExp2Context *context) = 0;

    virtual std::any visitPrimaryExp3(SysYParser::PrimaryExp3Context *context) = 0;

    virtual std::any visitNumber(SysYParser::NumberContext *context) = 0;

    virtual std::any visitUnary1(SysYParser::Unary1Context *context) = 0;

    virtual std::any visitUnary2(SysYParser::Unary2Context *context) = 0;

    virtual std::any visitUnary3(SysYParser::Unary3Context *context) = 0;

    virtual std::any visitUnaryOp(SysYParser::UnaryOpContext *context) = 0;

    virtual std::any visitFuncRParams(SysYParser::FuncRParamsContext *context) = 0;

    virtual std::any visitExpAsRParam(SysYParser::ExpAsRParamContext *context) = 0;

    virtual std::any visitStringAsRParam(SysYParser::StringAsRParamContext *context) = 0;

    virtual std::any visitMul2(SysYParser::Mul2Context *context) = 0;

    virtual std::any visitMul1(SysYParser::Mul1Context *context) = 0;

    virtual std::any visitAdd2(SysYParser::Add2Context *context) = 0;

    virtual std::any visitAdd1(SysYParser::Add1Context *context) = 0;

    virtual std::any visitRel2(SysYParser::Rel2Context *context) = 0;

    virtual std::any visitRel1(SysYParser::Rel1Context *context) = 0;

    virtual std::any visitEq1(SysYParser::Eq1Context *context) = 0;

    virtual std::any visitEq2(SysYParser::Eq2Context *context) = 0;

    virtual std::any visitLAnd2(SysYParser::LAnd2Context *context) = 0;

    virtual std::any visitLAnd1(SysYParser::LAnd1Context *context) = 0;

    virtual std::any visitLOr1(SysYParser::LOr1Context *context) = 0;

    virtual std::any visitLOr2(SysYParser::LOr2Context *context) = 0;

    virtual std::any visitConstExp(SysYParser::ConstExpContext *context) = 0;


};

