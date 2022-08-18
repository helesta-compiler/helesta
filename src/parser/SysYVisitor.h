
// Generated from SysY.g4 by ANTLR 4.8

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
    virtual antlrcpp::Any visitCompUnit(SysYParser::CompUnitContext *context) = 0;

    virtual antlrcpp::Any visitDecl(SysYParser::DeclContext *context) = 0;

    virtual antlrcpp::Any visitConstDecl(SysYParser::ConstDeclContext *context) = 0;

    virtual antlrcpp::Any visitBType(SysYParser::BTypeContext *context) = 0;

    virtual antlrcpp::Any visitConstDef(SysYParser::ConstDefContext *context) = 0;

    virtual antlrcpp::Any visitScalarConstInitVal(SysYParser::ScalarConstInitValContext *context) = 0;

    virtual antlrcpp::Any visitListConstInitVal(SysYParser::ListConstInitValContext *context) = 0;

    virtual antlrcpp::Any visitVarDecl(SysYParser::VarDeclContext *context) = 0;

    virtual antlrcpp::Any visitUninitVarDef(SysYParser::UninitVarDefContext *context) = 0;

    virtual antlrcpp::Any visitInitVarDef(SysYParser::InitVarDefContext *context) = 0;

    virtual antlrcpp::Any visitScalarInitVal(SysYParser::ScalarInitValContext *context) = 0;

    virtual antlrcpp::Any visitListInitval(SysYParser::ListInitvalContext *context) = 0;

    virtual antlrcpp::Any visitFuncDef(SysYParser::FuncDefContext *context) = 0;

    virtual antlrcpp::Any visitFuncType(SysYParser::FuncTypeContext *context) = 0;

    virtual antlrcpp::Any visitFuncFParams(SysYParser::FuncFParamsContext *context) = 0;

    virtual antlrcpp::Any visitFuncFParam(SysYParser::FuncFParamContext *context) = 0;

    virtual antlrcpp::Any visitBlock(SysYParser::BlockContext *context) = 0;

    virtual antlrcpp::Any visitBlockItem(SysYParser::BlockItemContext *context) = 0;

    virtual antlrcpp::Any visitAssignment(SysYParser::AssignmentContext *context) = 0;

    virtual antlrcpp::Any visitExpStmt(SysYParser::ExpStmtContext *context) = 0;

    virtual antlrcpp::Any visitBlockStmt(SysYParser::BlockStmtContext *context) = 0;

    virtual antlrcpp::Any visitIfStmt1(SysYParser::IfStmt1Context *context) = 0;

    virtual antlrcpp::Any visitIfStmt2(SysYParser::IfStmt2Context *context) = 0;

    virtual antlrcpp::Any visitWhileStmt(SysYParser::WhileStmtContext *context) = 0;

    virtual antlrcpp::Any visitBreakStmt(SysYParser::BreakStmtContext *context) = 0;

    virtual antlrcpp::Any visitContinueStmt(SysYParser::ContinueStmtContext *context) = 0;

    virtual antlrcpp::Any visitReturnStmt(SysYParser::ReturnStmtContext *context) = 0;

    virtual antlrcpp::Any visitCond(SysYParser::CondContext *context) = 0;

    virtual antlrcpp::Any visitLVal(SysYParser::LValContext *context) = 0;

    virtual antlrcpp::Any visitPrimaryExp1(SysYParser::PrimaryExp1Context *context) = 0;

    virtual antlrcpp::Any visitPrimaryExp2(SysYParser::PrimaryExp2Context *context) = 0;

    virtual antlrcpp::Any visitPrimaryExp3(SysYParser::PrimaryExp3Context *context) = 0;

    virtual antlrcpp::Any visitNumber(SysYParser::NumberContext *context) = 0;

    virtual antlrcpp::Any visitUnary1(SysYParser::Unary1Context *context) = 0;

    virtual antlrcpp::Any visitUnary2(SysYParser::Unary2Context *context) = 0;

    virtual antlrcpp::Any visitUnary3(SysYParser::Unary3Context *context) = 0;

    virtual antlrcpp::Any visitFuncRParams(SysYParser::FuncRParamsContext *context) = 0;

    virtual antlrcpp::Any visitExpAsRParam(SysYParser::ExpAsRParamContext *context) = 0;

    virtual antlrcpp::Any visitStringAsRParam(SysYParser::StringAsRParamContext *context) = 0;

    virtual antlrcpp::Any visitEqExp(SysYParser::EqExpContext *context) = 0;

    virtual antlrcpp::Any visitLOrExp(SysYParser::LOrExpContext *context) = 0;

    virtual antlrcpp::Any visitLAndExp(SysYParser::LAndExpContext *context) = 0;

    virtual antlrcpp::Any visitExp1(SysYParser::Exp1Context *context) = 0;

    virtual antlrcpp::Any visitRelExp(SysYParser::RelExpContext *context) = 0;

    virtual antlrcpp::Any visitAddExp(SysYParser::AddExpContext *context) = 0;

    virtual antlrcpp::Any visitExp2(SysYParser::Exp2Context *context) = 0;

    virtual antlrcpp::Any visitMulExp(SysYParser::MulExpContext *context) = 0;

    virtual antlrcpp::Any visitConstExp(SysYParser::ConstExpContext *context) = 0;


};

