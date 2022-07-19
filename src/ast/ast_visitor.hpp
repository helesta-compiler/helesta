#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ast/exp_value.hpp"
#include "ast/symbol_table.hpp"
#include "common/common.hpp"
#include "common/errors.hpp"
#include "ir/ir.hpp"
#include "parser/SysYBaseVisitor.h"

class ASTVisitor : public SysYBaseVisitor {
  enum ValueMode { normal, compile_time, condition } mode;

  IR::NormalFunc *init_func;
  IR::BB *init_bb;

  FunctionTable functions;
  VariableTable global_var;
  int string_literal_n;
  int float_literal_n;
  IR::CompileUnit &ir;

  std::string cur_func_name;
  IR::NormalFunc *cur_func;
  IR::BB *cur_bb, *return_bb;
  std::vector<std::pair<IR::Reg, IR::BB *>> return_value;
  std::vector<VariableTable *>
      local_var; // local var table in current function, clear and destruct on
                 // finishing processing the function
  VariableTable *cur_local_table;
  // when cur_func == nullptr, cur_bb is nullptr, local_var is empty,
  // cur_local_table is nullptr
  bool in_init, found_main;
  std::vector<IR::BB *> break_target, continue_target;
  ScalarType currentScalarType;

  void
  register_lib_function(std::string name, ScalarType return_type,
                        std::vector<std::variant<Type, StringType>> params);
  void register_lib_functions();
  std::vector<MemSize>
  get_array_dims(std::vector<SysYParser::ConstExpContext *> dims);
  IRValue
  to_IRValue(antlrcpp::Any value); // check null, IRValue and CondJumpList
  CondJumpList
  to_CondJumpList(antlrcpp::Any value); // check null, IRValue and CondJumpList,
                                        // after this call, cur_bb is nullptr
  IR::Reg get_value(const IRValue &value); // check array
  IR::Reg get_value(ScalarType type,
                    const IRValue &value); // implicit type cast
  IR::Reg new_reg();
  IR::BB *new_BB();
  VariableTable *new_variable_table(VariableTable *parent);
  VariableTableEntry *resolve(const std::string &name);

  template <typename ScalarType>
  std::vector<ScalarType>
  parse_const_init(SysYParser::ConstInitValContext *root,
                   const std::vector<MemSize> &shape);
  template <typename Scalar>
  void dfs_const_init(SysYParser::ListConstInitValContext *node,
                      const std::vector<MemSize> &shape,
                      std::vector<Scalar> &result);
  std::vector<std::optional<IR::Reg>>
  parse_var_init(ScalarType type, SysYParser::InitValContext *root,
                 const std::vector<MemSize> &shape);
  void dfs_var_init(ScalarType type, SysYParser::ListInitvalContext *node,
                    const std::vector<MemSize> &shape,
                    std::vector<std::optional<IR::Reg>> &result);
  void gen_var_init_ir(const std::vector<std::optional<IR::Reg>> &init,
                       IR::MemObject *obj, bool ignore_zero);

public:
  ASTVisitor(IR::CompileUnit &_ir);

  using node = antlr4::tree::ParseTree;

  virtual antlrcpp::Any visitChildren(antlr4::tree::ParseTree *ctx) override;

  // node visitors:

  virtual antlrcpp::Any
  visitCompUnit(SysYParser::CompUnitContext *ctx) override;

  virtual antlrcpp::Any visitDecl(SysYParser::DeclContext *ctx) override;

  virtual antlrcpp::Any
  visitConstDecl(SysYParser::ConstDeclContext *ctx) override;

  virtual antlrcpp::Any visitBType(SysYParser::BTypeContext *ctx) override;

  template <typename Scalar>
  antlrcpp::Any visitConstDefGeneric(SysYParser::ConstDefContext *ctx) {
    std::string name = ctx->Identifier()->getText();
    IR::MemObject *ir_obj;
    Type type(currentScalarType);
    type.is_const = true;
    type.array_dims = get_array_dims(ctx->constExp());
    std::vector<Scalar> init_value =
        parse_const_init<Scalar>(ctx->constInitVal(), type.array_dims);
    assert(init_value.size() == type.count_elements());
    if (cur_func) {
      if (cur_local_table->resolve(name))
        _throw DuplicateLocalName(name);
      ir_obj = cur_func->scope.new_MemObject(name);
      ir_obj->size = type.size();
      ir_obj->scalar_type = ScalarType::Int;
      cur_bb->push(new IR::LocalVarDef(ir_obj));
      IR::Reg start_addr = new_reg();
      cur_bb->push(new IR::LoadAddr(start_addr, ir_obj));
      for (size_t i = 0; i < init_value.size(); ++i) {
        IR::Reg temp = new_reg();
        cur_bb->push(new IR::LoadConst(temp, init_value[i]));
        if (i == 0) {
          cur_bb->push(new IR::StoreInstr(start_addr, temp));
          continue;
        }
        IR::Reg index = new_reg();
        cur_bb->push(new IR::LoadConst(index, static_cast<int32_t>(i)));
        IR::Reg cur_addr = new_reg();
        cur_bb->push(
            new IR::ArrayIndex(cur_addr, start_addr, index, INT_SIZE, -1));
        cur_bb->push(new IR::StoreInstr(cur_addr, temp));
      }
      cur_local_table->register_const(name, ir_obj, type,
                                      std::move(init_value));
    } else {
      if (global_var.resolve(name) || functions.resolve(name))
        _throw DuplicateGlobalName(name);
      ir_obj = ir.scope.new_MemObject(name);
      Scalar *buf = new Scalar[init_value.size()];
      for (size_t i = 0; i < init_value.size(); ++i)
        buf[i] = init_value[i];
      ir_obj->init(buf, type.size());
      global_var.register_const(name, ir_obj, type, std::move(init_value));
    }
    for (MemSize i : type.array_dims)
      ir_obj->dims.push_back(static_cast<Scalar>(i));
    return nullptr;
  }

  virtual antlrcpp::Any
  visitConstDef(SysYParser::ConstDefContext *ctx) override;

  virtual antlrcpp::Any
  visitScalarConstInitVal(SysYParser::ScalarConstInitValContext *ctx) override;

  virtual antlrcpp::Any
  visitListConstInitVal(SysYParser::ListConstInitValContext *ctx) override;

  virtual antlrcpp::Any visitVarDecl(SysYParser::VarDeclContext *ctx) override;

  virtual antlrcpp::Any
  visitUninitVarDef(SysYParser::UninitVarDefContext *ctx) override;

  virtual antlrcpp::Any
  visitInitVarDef(SysYParser::InitVarDefContext *ctx) override;

  virtual antlrcpp::Any
  visitScalarInitVal(SysYParser::ScalarInitValContext *ctx) override;

  virtual antlrcpp::Any
  visitListInitval(SysYParser::ListInitvalContext *ctx) override;

  virtual antlrcpp::Any visitFuncDef(SysYParser::FuncDefContext *ctx) override;

  virtual antlrcpp::Any
  visitFuncType(SysYParser::FuncTypeContext *ctx) override;

  virtual antlrcpp::Any
  visitFuncFParams(SysYParser::FuncFParamsContext *ctx) override;

  virtual antlrcpp::Any
  visitFuncFParam(SysYParser::FuncFParamContext *ctx) override;

  virtual antlrcpp::Any visitBlock(SysYParser::BlockContext *ctx) override;

  virtual antlrcpp::Any
  visitBlockItem(SysYParser::BlockItemContext *ctx) override;

  virtual antlrcpp::Any
  visitAssignment(SysYParser::AssignmentContext *ctx) override;

  virtual antlrcpp::Any visitExpStmt(SysYParser::ExpStmtContext *ctx) override;

  virtual antlrcpp::Any
  visitBlockStmt(SysYParser::BlockStmtContext *ctx) override;

  virtual antlrcpp::Any visitIfStmt1(SysYParser::IfStmt1Context *ctx) override;

  virtual antlrcpp::Any visitIfStmt2(SysYParser::IfStmt2Context *ctx) override;

  virtual antlrcpp::Any
  visitWhileStmt(SysYParser::WhileStmtContext *ctx) override;

  virtual antlrcpp::Any
  visitBreakStmt(SysYParser::BreakStmtContext *ctx) override;

  virtual antlrcpp::Any
  visitContinueStmt(SysYParser::ContinueStmtContext *ctx) override;

  virtual antlrcpp::Any
  visitReturnStmt(SysYParser::ReturnStmtContext *ctx) override;

  virtual antlrcpp::Any visitExp(SysYParser::ExpContext *ctx) override;

  virtual antlrcpp::Any visitCond(SysYParser::CondContext *ctx) override;

  virtual antlrcpp::Any visitLVal(SysYParser::LValContext *ctx) override;

  virtual antlrcpp::Any
  visitPrimaryExp1(SysYParser::PrimaryExp1Context *ctx) override;

  virtual antlrcpp::Any
  visitPrimaryExp2(SysYParser::PrimaryExp2Context *ctx) override;

  virtual antlrcpp::Any
  visitPrimaryExp3(SysYParser::PrimaryExp3Context *ctx) override;

  virtual antlrcpp::Any visitNumber(SysYParser::NumberContext *ctx) override;

  virtual antlrcpp::Any visitUnary1(SysYParser::Unary1Context *ctx) override;

  virtual antlrcpp::Any visitUnary2(SysYParser::Unary2Context *ctx) override;

  virtual antlrcpp::Any visitUnary3(SysYParser::Unary3Context *ctx) override;

  virtual antlrcpp::Any visitUnaryOp(SysYParser::UnaryOpContext *ctx) override;

  virtual antlrcpp::Any
  visitFuncRParams(SysYParser::FuncRParamsContext *ctx) override;

  virtual antlrcpp::Any
  visitExpAsRParam(SysYParser::ExpAsRParamContext *ctx) override;

  virtual antlrcpp::Any
  visitStringAsRParam(SysYParser::StringAsRParamContext *ctx) override;

  virtual antlrcpp::Any visitMul2(SysYParser::Mul2Context *ctx) override;

  virtual antlrcpp::Any visitMul1(SysYParser::Mul1Context *ctx) override;

  virtual antlrcpp::Any visitAdd2(SysYParser::Add2Context *ctx) override;

  virtual antlrcpp::Any visitAdd1(SysYParser::Add1Context *ctx) override;

  virtual antlrcpp::Any visitRel2(SysYParser::Rel2Context *ctx) override;

  virtual antlrcpp::Any visitRel1(SysYParser::Rel1Context *ctx) override;

  virtual antlrcpp::Any visitEq1(SysYParser::Eq1Context *ctx) override;

  virtual antlrcpp::Any visitEq2(SysYParser::Eq2Context *ctx) override;

  virtual antlrcpp::Any visitLAnd2(SysYParser::LAnd2Context *ctx) override;

  virtual antlrcpp::Any visitLAnd1(SysYParser::LAnd1Context *ctx) override;

  virtual antlrcpp::Any visitLOr1(SysYParser::LOr1Context *ctx) override;

  virtual antlrcpp::Any visitLOr2(SysYParser::LOr2Context *ctx) override;

  virtual antlrcpp::Any
  visitConstExp(SysYParser::ConstExpContext *ctx) override;
};
