#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "parser/SysYBaseVisitor.h"
#include "ast/exp_value.hpp"
#include "ast/symbol_table.hpp"
#include "ir/ir.hpp"

class ASTVisitor : public SysYBaseVisitor {
  enum ValueMode { normal, compile_time, condition } mode;

  IR::NormalFunc *init_func;
  IR::BB *init_bb;

  FunctionTable functions;
  VariableTable global_var;
  int string_literal_n;
  IR::CompileUnit &ir;

  std::string cur_func_name;
  IR::NormalFunc *cur_func;
  IR::BB *cur_bb, *return_bb;
  std::vector<std::pair<IR::Reg, IR::BB *>> return_value;
  std::vector<VariableTable *>
      local_var;  // local var table in current function, clear and destruct on
                  // finishing processing the function
  VariableTable *cur_local_table;
  // when cur_func == nullptr, cur_bb is nullptr, local_var is empty,
  // cur_local_table is nullptr
  bool in_init, found_main;
  std::vector<IR::BB *> break_target, continue_target;

  void register_lib_function(
      std::string name, bool return_non_void,
      std::vector<std::variant<Type, StringType>> params);
  void register_lib_functions();
  std::vector<MemSize> get_array_dims(
      std::vector<SysYParser::ConstExpContext *> dims);
  IRValue to_IRValue(
      std::any value);  // check null, IRValue and CondJumpList
  CondJumpList to_CondJumpList(
      std::any value);  // check null, IRValue and CondJumpList, after this
                             // call, cur_bb is nullptr
  IR::Reg get_value(const IRValue &value);  // check array
  IR::Reg new_reg();
  IR::BB *new_BB();
  VariableTable *new_variable_table(VariableTable *parent);
  VariableTableEntry *resolve(const std::string &name);

  std::vector<int32_t> parse_const_init(SysYParser::ConstInitValContext *root,
                                        const std::vector<MemSize> &shape);
  void dfs_const_init(SysYParser::ListConstInitValContext *node,
                      const std::vector<MemSize> &shape,
                      std::vector<int32_t> &result);
  std::vector<std::optional<IR::Reg>> parse_var_init(
      SysYParser::InitValContext *root, const std::vector<MemSize> &shape);
  void dfs_var_init(SysYParser::ListInitvalContext *node,
                    const std::vector<MemSize> &shape,
                    std::vector<std::optional<IR::Reg>> &result);
  void gen_var_init_ir(const std::vector<std::optional<IR::Reg>> &init,
                       IR::MemObject *obj, bool ignore_zero);

 public:
  ASTVisitor(IR::CompileUnit &_ir);

  using node = antlr4::tree::ParseTree;

  virtual std::any visitChildren(antlr4::tree::ParseTree *ctx) override;

  // node visitors:

  virtual std::any visitCompUnit(
      SysYParser::CompUnitContext *ctx) override;

  virtual std::any visitDecl(SysYParser::DeclContext *ctx) override;

  virtual std::any visitConstDecl(
      SysYParser::ConstDeclContext *ctx) override;

  virtual std::any visitBType(SysYParser::BTypeContext *ctx) override;

  virtual std::any visitConstDef(
      SysYParser::ConstDefContext *ctx) override;

  virtual std::any visitScalarConstInitVal(
      SysYParser::ScalarConstInitValContext *ctx) override;

  virtual std::any visitListConstInitVal(
      SysYParser::ListConstInitValContext *ctx) override;

  virtual std::any visitVarDecl(SysYParser::VarDeclContext *ctx) override;

  virtual std::any visitUninitVarDef(
      SysYParser::UninitVarDefContext *ctx) override;

  virtual std::any visitInitVarDef(
      SysYParser::InitVarDefContext *ctx) override;

  virtual std::any visitScalarInitVal(
      SysYParser::ScalarInitValContext *ctx) override;

  virtual std::any visitListInitval(
      SysYParser::ListInitvalContext *ctx) override;

  virtual std::any visitFuncDef(SysYParser::FuncDefContext *ctx) override;

  virtual std::any visitFuncType(
      SysYParser::FuncTypeContext *ctx) override;

  virtual std::any visitFuncFParams(
      SysYParser::FuncFParamsContext *ctx) override;

  virtual std::any visitFuncFParam(
      SysYParser::FuncFParamContext *ctx) override;

  virtual std::any visitBlock(SysYParser::BlockContext *ctx) override;

  virtual std::any visitBlockItem(
      SysYParser::BlockItemContext *ctx) override;

  virtual std::any visitAssignment(
      SysYParser::AssignmentContext *ctx) override;

  virtual std::any visitExpStmt(SysYParser::ExpStmtContext *ctx) override;

  virtual std::any visitBlockStmt(
      SysYParser::BlockStmtContext *ctx) override;

  virtual std::any visitIfStmt1(SysYParser::IfStmt1Context *ctx) override;

  virtual std::any visitIfStmt2(SysYParser::IfStmt2Context *ctx) override;

  virtual std::any visitWhileStmt(
      SysYParser::WhileStmtContext *ctx) override;

  virtual std::any visitBreakStmt(
      SysYParser::BreakStmtContext *ctx) override;

  virtual std::any visitContinueStmt(
      SysYParser::ContinueStmtContext *ctx) override;

  virtual std::any visitReturnStmt(
      SysYParser::ReturnStmtContext *ctx) override;

  virtual std::any visitExp(SysYParser::ExpContext *ctx) override;

  virtual std::any visitCond(SysYParser::CondContext *ctx) override;

  virtual std::any visitLVal(SysYParser::LValContext *ctx) override;

  virtual std::any visitPrimaryExp1(
      SysYParser::PrimaryExp1Context *ctx) override;

  virtual std::any visitPrimaryExp2(
      SysYParser::PrimaryExp2Context *ctx) override;

  virtual std::any visitPrimaryExp3(
      SysYParser::PrimaryExp3Context *ctx) override;

  virtual std::any visitNumber(SysYParser::NumberContext *ctx) override;

  virtual std::any visitUnary1(SysYParser::Unary1Context *ctx) override;

  virtual std::any visitUnary2(SysYParser::Unary2Context *ctx) override;

  virtual std::any visitUnary3(SysYParser::Unary3Context *ctx) override;

  virtual std::any visitUnaryOp(SysYParser::UnaryOpContext *ctx) override;

  virtual std::any visitFuncRParams(
      SysYParser::FuncRParamsContext *ctx) override;

  virtual std::any visitExpAsRParam(
      SysYParser::ExpAsRParamContext *ctx) override;

  virtual std::any visitStringAsRParam(
      SysYParser::StringAsRParamContext *ctx) override;

  virtual std::any visitMul2(SysYParser::Mul2Context *ctx) override;

  virtual std::any visitMul1(SysYParser::Mul1Context *ctx) override;

  virtual std::any visitAdd2(SysYParser::Add2Context *ctx) override;

  virtual std::any visitAdd1(SysYParser::Add1Context *ctx) override;

  virtual std::any visitRel2(SysYParser::Rel2Context *ctx) override;

  virtual std::any visitRel1(SysYParser::Rel1Context *ctx) override;

  virtual std::any visitEq1(SysYParser::Eq1Context *ctx) override;

  virtual std::any visitEq2(SysYParser::Eq2Context *ctx) override;

  virtual std::any visitLAnd2(SysYParser::LAnd2Context *ctx) override;

  virtual std::any visitLAnd1(SysYParser::LAnd1Context *ctx) override;

  virtual std::any visitLOr1(SysYParser::LOr1Context *ctx) override;

  virtual std::any visitLOr2(SysYParser::LOr2Context *ctx) override;

  virtual std::any visitConstExp(
      SysYParser::ConstExpContext *ctx) override;
};
