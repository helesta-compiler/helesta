#include "ast/ast_visitor.hpp"

#include <variant>

#include "ast/exp_value.hpp"
#include "ast/init_value.hpp"
#include "ast/symbol_table.hpp"
#include "common/common.hpp"
#include "common/errors.hpp"

using std::optional;
using std::pair;
using std::string;
using std::variant;
using std::vector;

void ASTVisitor::register_lib_function(
    string name, ScalarType return_type,
    vector<variant<Type, StringType>> params) {
  FunctionInterface interface;
  interface.return_type = return_type;
  interface.args_type = params;
  assert(ir.lib_funcs.find(name) != ir.lib_funcs.end());
  IR::LibFunc *ir_func = ir.lib_funcs[name].get();
  functions.register_func(name, ir_func, interface);
}
void ASTVisitor::register_lib_functions() {
  register_lib_function("getint", ScalarType::Int, {});
  register_lib_function("getch", ScalarType::Char, {});
  register_lib_function("getarray", ScalarType::Int,
                        {Type::UnknownLengthIntArray});
  register_lib_function("putint", ScalarType::Void, {Type(ScalarType::Int)});
  register_lib_function("putch", ScalarType::Void, {Type(ScalarType::Int)});
  register_lib_function("putarray", ScalarType::Void,
                        {Type(ScalarType::Int), Type::UnknownLengthFloatArray});
  register_lib_function("putf", ScalarType::Void, {StringType{}});
  functions.resolve("putf")->interface.variadic = true;
  register_lib_function("starttime", ScalarType::Void, {});
  register_lib_function("stoptime", ScalarType::Void, {});
}
vector<MemSize>
ASTVisitor::get_array_dims(vector<SysYParser::ConstExpContext *> dims) {
  vector<MemSize> ret;
  ret.reserve(dims.size());
  for (auto i : dims) {
    CompileTimeValue<int32_t> cur = i->accept(this).as<CompileTimeValueAny>();
    if (cur.value < 0)
      throw NegativeArraySize();
    ret.push_back(static_cast<MemSize>(cur.value));
  }
  return ret;
}

IRValue ASTVisitor::to_IRValue(antlrcpp::Any value) {
  if (value.isNull())
    throw VoidFuncReturnValueUsed();
  if (value.is<IRValue>())
    return value.as<IRValue>();
  assert(value.is<CondJumpList>());
  CondJumpList jump_list = value;
  IR::BB *true_bb = new_BB(), *false_bb = new_BB(), *res_bb = new_BB();
  for (IR::BB **i : jump_list.true_list)
    (*i) = true_bb;
  for (IR::BB **i : jump_list.false_list)
    (*i) = false_bb;
  IR::Reg true_reg = new_reg(), false_reg = new_reg(), res_reg = new_reg();
  true_bb->push(new IR::LoadConst(true_reg, static_cast<int32_t>(1)));
  true_bb->push(new IR::JumpInstr(res_bb));
  false_bb->push(new IR::LoadConst(false_reg, static_cast<int32_t>(0)));
  false_bb->push(new IR::JumpInstr(res_bb));
  IR::PhiInstr *phi_inst = new IR::PhiInstr(res_reg);
  phi_inst->uses.emplace_back(true_reg, true_bb);
  phi_inst->uses.emplace_back(false_reg, false_bb);
  res_bb->push(phi_inst);
  cur_bb = res_bb;
  IRValue ret(ScalarType::Int);
  ret.is_left_value = false;
  ret.reg = res_reg;
  return ret;
}

CondJumpList ASTVisitor::to_CondJumpList(antlrcpp::Any value) {
  if (value.is<IRValue>()) {
    debug << __FUNCTION__ << " value: " << value.as<IRValue>() << '\n';
  } else {
    debug << __FUNCTION__ << " value: " << value.isNull() << '\n';
  }
  if (value.isNull())
    throw VoidFuncReturnValueUsed();
  if (value.is<CondJumpList>())
    return value.as<CondJumpList>();
  assert(value.is<IRValue>());
  IRValue irv = value;
  IR::Reg reg = get_value(irv);
  IR::BranchInstr *inst = new IR::BranchInstr(reg, nullptr, nullptr);
  cur_bb->push(inst);
  cur_bb = nullptr;
  CondJumpList jump_list;
  jump_list.true_list.push_back(&inst->target1);
  jump_list.false_list.push_back(&inst->target0);
  return jump_list;
}

IR::Reg ASTVisitor::get_value(const IRValue &value) {
  if (value.type.is_array())
    throw ArrayTypedValueUsed();
  IR::Reg ret = value.reg;
  if (value.is_left_value) {
    IR::Reg temp = new_reg();
    cur_bb->push(new IR::LoadInstr(temp, ret));
    ret = temp;
  }
  return ret;
}

IR::Reg ASTVisitor::get_value(ScalarType type, const IRValue &value) {
  IR::Reg ret = get_value(value);
  switch (value.type.scalar_type) {
  case ScalarType::Int:
    switch (type) {
    case ScalarType::Int:
      break;
    case ScalarType::Float: {
      IR::Reg temp = new_reg();
      cur_bb->push(
          new IR::UnaryOpInstr(temp, ret, IR::UnaryOp(IR::UnaryOp::I2F)));
      ret = temp;
      break;
    }
    default:
      assert(false);
    }
    break;
  case ScalarType::Float:
    switch (type) {
    case ScalarType::Int: {
      IR::Reg temp = new_reg();
      cur_bb->push(
          new IR::UnaryOpInstr(temp, ret, IR::UnaryOp(IR::UnaryOp::F2I)));
      ret = temp;
      break;
    }
    case ScalarType::Float:
      break;
    default:
      assert(false);
    }
    break;
  default:
    debug << "???\n";
    debug << value.type.scalar_type << '\n';
    debug << value.type << '\n';
    assert(false);
  }
  return ret;
}

IR::Reg ASTVisitor::new_reg() {
  if (in_init && cur_func)
    throw RuntimeError("in global init state when visiting a function");
  if (in_init)
    return init_func->new_Reg();
  if (cur_func)
    return cur_func->new_Reg();
  throw RuntimeError("fail to new Reg");
  return IR::Reg{};
}

IR::BB *ASTVisitor::new_BB() {
  if (in_init && cur_func)
    throw RuntimeError("in global init state when visiting a function");
  if (in_init)
    return init_func->new_BB();
  if (cur_func)
    return cur_func->new_BB();
  throw RuntimeError("fail to new BB");
  return nullptr;
}

VariableTable *ASTVisitor::new_variable_table(VariableTable *parent) {
  VariableTable *ret = new VariableTable(parent);
  local_var.push_back(ret);
  return ret;
}

VariableTableEntry *ASTVisitor::resolve(const string &name) {
  if (cur_local_table)
    return cur_local_table->recursively_resolve(name);
  else
    return global_var.resolve(name);
}

ASTVisitor::ASTVisitor(IR::CompileUnit &_ir) : global_var(nullptr), ir(_ir) {}

antlrcpp::Any ASTVisitor::visitChildren(antlr4::tree::ParseTree *ctx) {
  size_t n = ctx->children.size();
  for (size_t i = 0; i < n; ++i)
    ctx->children[i]->accept(this);
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitCompUnit(SysYParser::CompUnitContext *ctx) {
  mode = normal;
  init_func = ir.new_NormalFunc(".init");
  init_bb = init_func->new_BB();
  init_func->entry = init_bb;
  string_literal_n = 0;
  cur_func_name = string{};
  cur_func = nullptr;
  cur_bb = nullptr;
  return_bb = nullptr;
  return_value.clear();
  local_var.clear();
  cur_local_table = nullptr;
  in_init = false;
  found_main = false;
  break_target.clear();
  continue_target.clear();
  register_lib_functions();
  visitChildren(ctx);
  auto r = init_func->new_Reg();
  init_bb->push(new IR::LoadConst(r, static_cast<int32_t>(0)));
  init_bb->push(new IR::ReturnInstr(r, true));
  return found_main;
}

antlrcpp::Any ASTVisitor::visitDecl(SysYParser::DeclContext *ctx) {
  return visitChildren(ctx);
}

antlrcpp::Any ASTVisitor::visitConstDecl(SysYParser::ConstDeclContext *ctx) {
  if (ctx->bType()->Int()) {
    currentScalarType = ScalarType::Int;
  } else {
    currentScalarType = ScalarType::Float;
  }
  return visitChildren(ctx);
}

antlrcpp::Any ASTVisitor::visitBType(SysYParser::BTypeContext *ctx) {
  return visitChildren(ctx);
}

template <typename Scalar>
void ASTVisitor::dfs_const_init(SysYParser::ListConstInitValContext *node,
                                const vector<MemSize> &shape,
                                vector<Scalar> &result) {
  if (shape.size() == 0)
    throw InvalidInitList();
  MemSize total_size = 1, child_size = 1;
  for (size_t i = 0; i < shape.size(); ++i) {
    total_size *= shape[i];
    if (i > 0)
      child_size *= shape[i];
  }
  if (total_size == 0)
    return;
  result.reserve(result.size() + total_size);
  vector<MemSize> child_shape = shape;
  child_shape.erase(child_shape.begin());
  MemSize cnt = 0;
  for (auto child : node->constInitVal()) {
    if (auto scalar_child =
            dynamic_cast<SysYParser::ScalarConstInitValContext *>(child)) {
      if (cnt + 1 > total_size)
        throw InvalidInitList();
      result.push_back(scalar_child->constExp()
                           ->accept(this)
                           .as<CompileTimeValueAny>()
                           .value<Scalar>());
      ++cnt;
    } else {
      auto list_child =
          dynamic_cast<SysYParser::ListConstInitValContext *>(child);
      assert(list_child);
      if (cnt % child_size != 0 || cnt + child_size > total_size)
        throw InvalidInitList();
      dfs_const_init(list_child, child_shape, result);
      cnt += child_size;
    }
  }
  while (cnt < total_size) {
    result.push_back(0);
    ++cnt;
  }
}

template <typename Scalar>
vector<Scalar>
ASTVisitor::parse_const_init(SysYParser::ConstInitValContext *root,
                             const vector<MemSize> &shape) {
  vector<Scalar> ret;
  if (auto scalar_root =
          dynamic_cast<SysYParser::ScalarConstInitValContext *>(root)) {
    if (shape.size())
      throw InvalidInitList();
    ret.push_back(scalar_root->constExp()
                      ->accept(this)
                      .as<CompileTimeValueAny>()
                      .value<Scalar>());
    return ret;
  }
  auto list_root = dynamic_cast<SysYParser::ListConstInitValContext *>(root);
  assert(list_root);
  dfs_const_init(list_root, shape, ret);
  return ret;
}

antlrcpp::Any ASTVisitor::visitConstDef(SysYParser::ConstDefContext *ctx) {
  if (currentScalarType == ScalarType::Int) {
    return visitConstDefGeneric<int32_t>(ctx);
  } else {
    return visitConstDefGeneric<float>(ctx);
  }
}

antlrcpp::Any ASTVisitor::visitScalarConstInitVal(
    SysYParser::ScalarConstInitValContext *ctx) {
  throw RuntimeError(
      "ASTVisitor::visitScalarConstInitVal should be unreachable");
  return nullptr;
}

antlrcpp::Any
ASTVisitor::visitListConstInitVal(SysYParser::ListConstInitValContext *ctx) {
  throw RuntimeError("ASTVisitor::visitListConstInitVal should be unreachable");
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitVarDecl(SysYParser::VarDeclContext *ctx) {
  if (ctx->bType()->Int()) {
    currentScalarType = ScalarType::Int;
  } else {
    currentScalarType = ScalarType::Float;
  }
  return visitChildren(ctx);
}

antlrcpp::Any
ASTVisitor::visitUninitVarDef(SysYParser::UninitVarDefContext *ctx) {
  Type type(currentScalarType);
  type.array_dims = get_array_dims(ctx->constExp());
  string name = ctx->Identifier()->getText();
  IR::MemObject *ir_obj;
  if (cur_func) {
    if (cur_local_table->resolve(name))
      throw DuplicateLocalName(name);
    ir_obj = cur_func->scope.new_MemObject(name);
    cur_bb->push(new IR::LocalVarDef(ir_obj));
    cur_local_table->register_var(name, ir_obj, type);
  } else {
    if (global_var.resolve(name) || functions.resolve(name))
      throw DuplicateGlobalName(name);
    ir_obj = ir.scope.new_MemObject(name);
    ir_obj->initial_value = nullptr;
    global_var.register_var(name, ir_obj, type);
  }
  ir_obj->scalar_type = currentScalarType;
  ir_obj->size = type.size();
  for (MemSize i : type.array_dims)
    ir_obj->dims.push_back(static_cast<int>(i));
  return nullptr;
}

void ASTVisitor::dfs_var_init(SysYParser::ListInitvalContext *node,
                              const vector<MemSize> &shape,
                              vector<optional<IR::Reg>> &result) {
  if (shape.size() == 0)
    throw InvalidInitList();
  MemSize total_size = 1, child_size = 1;
  for (size_t i = 0; i < shape.size(); ++i) {
    total_size *= shape[i];
    if (i > 0)
      child_size *= shape[i];
  }
  if (total_size == 0)
    return;
  result.reserve(result.size() + total_size);
  vector<MemSize> child_shape = shape;
  child_shape.erase(child_shape.begin());
  MemSize cnt = 0;
  for (auto child : node->initVal()) {
    if (auto scalar_child =
            dynamic_cast<SysYParser::ScalarInitValContext *>(child)) {
      if (cnt + 1 > total_size)
        throw InvalidInitList();
      result.emplace_back(
          get_value(to_IRValue(scalar_child->exp()->accept(this))));
      ++cnt;
    } else {
      auto list_child = dynamic_cast<SysYParser::ListInitvalContext *>(child);
      assert(list_child);
      if (cnt % child_size != 0 || cnt + child_size > total_size)
        throw InvalidInitList();
      dfs_var_init(list_child, child_shape, result);
      cnt += child_size;
    }
  }
  while (cnt < total_size) {
    result.emplace_back();
    ++cnt;
  }
}

vector<optional<IR::Reg>>
ASTVisitor::parse_var_init(SysYParser::InitValContext *root,
                           const vector<MemSize> &shape) {
  vector<optional<IR::Reg>> result;
  if (auto scalar_root =
          dynamic_cast<SysYParser::ScalarInitValContext *>(root)) {
    if (shape.size())
      throw InvalidInitList();
    result.emplace_back(
        get_value(to_IRValue(scalar_root->exp()->accept(this))));
    return result;
  }
  auto list_root = dynamic_cast<SysYParser::ListInitvalContext *>(root);
  assert(list_root);
  dfs_var_init(list_root, shape, result);
  return result;
}

void ASTVisitor::gen_var_init_ir(const vector<optional<IR::Reg>> &init,
                                 IR::MemObject *obj, bool ignore_zero) {
  if (ignore_zero) {
    IR::Reg start_addr = new_reg();
    cur_bb->push(new IR::LoadAddr(start_addr, obj));
    for (size_t i = 0; i < init.size(); ++i)
      if (init[i]) {
        if (i == 0) {
          cur_bb->push(new IR::StoreInstr(start_addr, init[i].value()));
          continue;
        }
        IR::Reg index = new_reg(), addr = new_reg();
        cur_bb->push(new IR::LoadConst(index, static_cast<int32_t>(i)));
        cur_bb->push(new IR::ArrayIndex(addr, start_addr, index, INT_SIZE, -1));
        cur_bb->push(new IR::StoreInstr(addr, init[i].value()));
      }
  } else {
    IR::Reg start_addr = new_reg(), zero = new_reg();
    cur_bb->push(new IR::LoadAddr(start_addr, obj));
    cur_bb->push(new IR::LoadConst(zero, static_cast<int32_t>(0)));
    for (size_t i = 0; i < init.size(); ++i) {
      if (i == 0) {
        if (init[i]) {
          cur_bb->push(new IR::StoreInstr(start_addr, init[i].value()));
        } else {
          cur_bb->push(new IR::StoreInstr(start_addr, zero));
        }
        continue;
      }
      IR::Reg index = new_reg(), addr = new_reg();
      cur_bb->push(new IR::LoadConst(index, static_cast<int32_t>(i)));
      cur_bb->push(new IR::ArrayIndex(addr, start_addr, index, INT_SIZE, -1));
      if (init[i]) {
        cur_bb->push(new IR::StoreInstr(addr, init[i].value()));
      } else {
        cur_bb->push(new IR::StoreInstr(addr, zero));
      }
    }
  }
}

antlrcpp::Any ASTVisitor::visitInitVarDef(SysYParser::InitVarDefContext *ctx) {
  Type type(currentScalarType);
  type.array_dims = get_array_dims(ctx->constExp());
  string name = ctx->Identifier()->getText();
  IR::MemObject *ir_obj;
  if (cur_func) {
    if (cur_local_table->resolve(name))
      throw DuplicateLocalName(name);
    ir_obj = cur_func->scope.new_MemObject(name);
    cur_bb->push(new IR::LocalVarDef(ir_obj));
    cur_local_table->register_var(name, ir_obj, type);
    vector<optional<IR::Reg>> init =
        parse_var_init(ctx->initVal(), type.array_dims);
    assert(init.size() == type.count_elements());
    gen_var_init_ir(init, ir_obj, false);
  } else {
    if (global_var.resolve(name) || functions.resolve(name))
      throw DuplicateGlobalName(name);
    ir_obj = ir.scope.new_MemObject(name);
    ir_obj->initial_value = nullptr;
    global_var.register_var(name, ir_obj, type);
    cur_bb = init_bb;
    in_init = true;
    vector<optional<IR::Reg>> init =
        parse_var_init(ctx->initVal(), type.array_dims);
    assert(init.size() == type.count_elements());
    gen_var_init_ir(init, ir_obj, true);
    init_bb = cur_bb;
    cur_bb = nullptr;
    in_init = false;
  }
  ir_obj->scalar_type = currentScalarType;
  ir_obj->size = type.size();
  for (MemSize i : type.array_dims)
    ir_obj->dims.push_back(static_cast<int>(i));
  return nullptr;
}

antlrcpp::Any
ASTVisitor::visitScalarInitVal(SysYParser::ScalarInitValContext *ctx) {
  throw RuntimeError("ASTVisitor::visitScalarInitVal should be unreachable");
  return nullptr;
}

antlrcpp::Any
ASTVisitor::visitListInitval(SysYParser::ListInitvalContext *ctx) {
  throw RuntimeError("ASTVisitor::visitListInitval should be unreachable");
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitFuncDef(SysYParser::FuncDefContext *ctx) {
  bool return_value_non_void = (ctx->funcType()->getText() != "void");
  string name = ctx->Identifier()->getText();
  vector<pair<string, Type>> params;
  if (ctx->funcFParams())
    params = ctx->funcFParams()->accept(this).as<vector<pair<string, Type>>>();
  FunctionInterface interface;
  auto funcType = ctx->funcType()->getText();
  if (funcType == "int") {
    interface.return_type = ScalarType::Int;
  } else if (funcType == "float") {
    interface.return_type = ScalarType::Float;
  } else if (funcType == "void") {
    interface.return_type = ScalarType::Void;
  }
  for (auto &i : params)
    interface.args_type.emplace_back(i.second);
  cur_func = ir.new_NormalFunc(name);
  functions.register_func(name, cur_func, interface);
  cur_bb = cur_func->new_BB();
  cur_func->entry = cur_bb;
  cur_local_table = new_variable_table(&global_var);
  if (name == "main") {
    found_main = true;
    cur_bb->push(
        new IR::CallInstr(new_reg(), init_func, vector<IR::Reg>{}, true));
    if (!return_value_non_void)
      throw InvalidMainFuncInterface("main function should return int");
    if (params.size() > 0)
      throw InvalidMainFuncInterface("main function should have no parameters");
  }
  return_bb = cur_func->new_BB();
  for (int i = 0; i < static_cast<int>(params.size()); ++i) {
    if (params[i].second.is_array()) {
      IR::MemObject *obj =
          cur_func->scope.new_MemObject("arg_" + params[i].first);
      obj->size = 0;
      obj->scalar_type = params[i].second.scalar_type;
      obj->arg = true;
      obj->dims.push_back(-1);
      for (MemSize i : params[i].second.array_dims)
        obj->dims.push_back(static_cast<int>(i));
      cur_func->scope.set_arg(i, obj);
      cur_local_table->register_var(params[i].first, nullptr, params[i].second);
      cur_local_table->resolve(params[i].first)->arg_id = i;
      cur_bb->push(new IR::LocalVarDef(obj));
    } else {
      IR::MemObject *obj = cur_func->scope.new_MemObject(params[i].first);
      obj->size = params[i].second.size();
      obj->scalar_type = params[i].second.scalar_type;
      cur_local_table->register_var(params[i].first, obj, params[i].second);
      cur_bb->push(new IR::LocalVarDef(obj));
      IR::Reg value = new_reg(), addr = new_reg();
      cur_bb->push(new IR::LoadArg(value, i));
      cur_bb->push(new IR::LoadAddr(addr, obj));
      cur_bb->push(new IR::StoreInstr(addr, value));
    }
  }
  cur_func_name = name;
  ctx->block()->accept(this);
  assert(cur_bb != nullptr);
  if (return_value_non_void) {
    IR::Reg zero = new_reg();
    cur_bb->push(new IR::LoadConst(zero, static_cast<int32_t>(0)));
    cur_bb->push(new IR::JumpInstr(return_bb));
    return_value.emplace_back(zero, cur_bb);
    IR::Reg ret_value = cur_func->new_Reg();
    IR::PhiInstr *inst = new IR::PhiInstr(ret_value);
    inst->uses = return_value;
    return_bb->push(inst);
    return_bb->push(new IR::ReturnInstr(ret_value, false));
  } else {
    cur_bb->push(new IR::JumpInstr(return_bb));
    IR::Reg ret_value = cur_func->new_Reg();
    return_bb->push(new IR::LoadConst(ret_value, static_cast<int32_t>(0)));
    return_bb->push(new IR::ReturnInstr(ret_value, true));
  }
  cur_func = nullptr;
  cur_bb = nullptr;
  return_bb = nullptr;
  cur_local_table = nullptr;
  for (VariableTable *i : local_var)
    delete i;
  local_var.clear();
  return_value.clear();
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitFuncType(SysYParser::FuncTypeContext *ctx) {
  return nullptr;
}

antlrcpp::Any
ASTVisitor::visitFuncFParams(SysYParser::FuncFParamsContext *ctx) {
  vector<pair<string, Type>> ret;
  for (auto i : ctx->funcFParam())
    ret.push_back(i->accept(this));
  return ret;
}

antlrcpp::Any ASTVisitor::visitFuncFParam(SysYParser::FuncFParamContext *ctx) {
  Type type(ctx->bType()->Int() ? ScalarType::Int : ScalarType::Float);
  if (ctx->getText().find("[") != string::npos)
    type.omit_first_dim = true;
  type.array_dims = get_array_dims(ctx->constExp());
  return pair<string, Type>{ctx->Identifier()->getText(), type};
}

antlrcpp::Any ASTVisitor::visitBlock(SysYParser::BlockContext *ctx) {
  return visitChildren(ctx);
}

antlrcpp::Any ASTVisitor::visitBlockItem(SysYParser::BlockItemContext *ctx) {
  return visitChildren(ctx);
}

antlrcpp::Any ASTVisitor::visitAssignment(SysYParser::AssignmentContext *ctx) {
  assert(mode == normal);
  IRValue lhs = ctx->lVal()->accept(this),
          rhs = to_IRValue(ctx->exp()->accept(this));
  if (!lhs.assignable())
    throw AssignmentTypeError("left hand side '" + ctx->lVal()->getText() +
                              "' is not assignable");
  if (!lhs.type.check_assign(rhs.type))
    throw AssignmentTypeError("type error on assignment '" + ctx->getText() +
                              "'");
  IR::Reg rhs_value = get_value(rhs);
  cur_bb->push(new IR::StoreInstr(lhs.reg, rhs_value));
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitExpStmt(SysYParser::ExpStmtContext *ctx) {
  return visitChildren(ctx);
}

antlrcpp::Any ASTVisitor::visitBlockStmt(SysYParser::BlockStmtContext *ctx) {
  VariableTable *parent = cur_local_table;
  VariableTable *child = new_variable_table(parent);
  cur_local_table = child;
  ctx->block()->accept(this);
  cur_local_table = parent;
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitIfStmt1(SysYParser::IfStmt1Context *ctx) {
  CondJumpList cond = ctx->cond()->accept(this);
  VariableTable *parent = cur_local_table;
  VariableTable *child = new_variable_table(parent);
  IR::BB *true_entry = new_BB();
  cur_bb = true_entry;
  cur_local_table = child;
  ctx->stmt()->accept(this);
  IR::BB *true_end = cur_bb;
  IR::BB *out = new_BB();
  cur_local_table = parent;
  cur_bb = out;
  true_end->push(new IR::JumpInstr(out));
  for (IR::BB **i : cond.true_list)
    (*i) = true_entry;
  for (IR::BB **i : cond.false_list)
    (*i) = out;
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitIfStmt2(SysYParser::IfStmt2Context *ctx) {
  CondJumpList cond = ctx->cond()->accept(this);
  VariableTable *parent = cur_local_table;
  VariableTable *child_true = new_variable_table(parent),
                *child_false = new_variable_table(parent);
  IR::BB *true_entry = new_BB();
  cur_bb = true_entry;
  cur_local_table = child_true;
  ctx->stmt(0)->accept(this);
  IR::BB *true_end = cur_bb;
  IR::BB *false_entry = new_BB();
  cur_bb = false_entry;
  cur_local_table = child_false;
  ctx->stmt(1)->accept(this);
  IR::BB *false_end = cur_bb;
  IR::BB *out = new_BB();
  cur_local_table = parent;
  cur_bb = out;
  true_end->push(new IR::JumpInstr(out));
  false_end->push(new IR::JumpInstr(out));
  for (IR::BB **i : cond.true_list)
    (*i) = true_entry;
  for (IR::BB **i : cond.false_list)
    (*i) = false_entry;
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitWhileStmt(SysYParser::WhileStmtContext *ctx) {
  IR::BB *cond_entry = new_BB();
  cur_bb->push(new IR::JumpInstr(cond_entry));
  cur_bb = cond_entry;
  CondJumpList cond = ctx->cond()->accept(this);
  IR::BB *out = new_BB(), *body_entry = new_BB(), *jump_back = new_BB();
  VariableTable *parent = cur_local_table;
  VariableTable *child = new_variable_table(parent);
  cur_bb = body_entry;
  cur_local_table = child;
  break_target.push_back(out);
  continue_target.push_back(jump_back);
  ctx->stmt()->accept(this);
  break_target.pop_back();
  continue_target.pop_back();
  IR::BB *body_end = cur_bb;
  body_end->push(new IR::JumpInstr(jump_back));
  jump_back->push(new IR::JumpInstr(cond_entry));
  cur_bb = out;
  cur_local_table = parent;
  for (IR::BB **i : cond.true_list)
    (*i) = body_entry;
  for (IR::BB **i : cond.false_list)
    (*i) = out;
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitBreakStmt(SysYParser::BreakStmtContext *ctx) {
  if (break_target.size() == 0)
    throw InvalidBreak();
  cur_bb->push(new IR::JumpInstr(*std::prev(break_target.end())));
  cur_bb = new_BB(); // unreachable block
  return nullptr;
}

antlrcpp::Any
ASTVisitor::visitContinueStmt(SysYParser::ContinueStmtContext *ctx) {
  if (continue_target.size() == 0)
    throw InvalidContinue();
  cur_bb->push(new IR::JumpInstr(*std::prev(continue_target.end())));
  cur_bb = new_BB(); // unreachable block
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitReturnStmt(SysYParser::ReturnStmtContext *ctx) {
  assert(mode == normal);
  if (ctx->exp()) {
    if (functions.resolve(cur_func_name)->interface.return_type !=
        ScalarType::Void) {
      IR::Reg ret_value = get_value(to_IRValue(ctx->exp()->accept(this)));
      cur_bb->push(new IR::JumpInstr(return_bb));
      return_value.emplace_back(ret_value, cur_bb);
    } else
      throw InvalidReturn("return a value in void function");
  } else {
    if (functions.resolve(cur_func_name)->interface.return_type !=
        ScalarType::Void) {
      throw InvalidReturn("return value not found in a non-void function");
    } else {
      cur_bb->push(new IR::JumpInstr(return_bb));
    }
  }
  cur_bb = new_BB(); // unreachable block;
  return nullptr;
}

antlrcpp::Any ASTVisitor::visitExp(SysYParser::ExpContext *ctx) {
  return ctx->addExp()->accept(this);
}

antlrcpp::Any ASTVisitor::visitCond(SysYParser::CondContext *ctx) {
  debug << __FUNCTION__ << '\n';
  mode = condition;
  antlrcpp::Any value = ctx->lOrExp()->accept(this);
  mode = normal;
  auto ret = to_CondJumpList(std::move(value));
  return ret;
}

antlrcpp::Any ASTVisitor::visitLVal(SysYParser::LValContext *ctx) {
  VariableTableEntry *entry = resolve(ctx->Identifier()->getText());
  if (!entry)
    throw UnrecognizedVarName(ctx->Identifier()->getText());
  if (mode == compile_time) {
    if (!entry->type.is_const)
      throw CompileTimeValueEvalFail(
          "non-const variable used in compile-time constant expression");
    if (!entry->type.is_array()) {
      if (ctx->exp().size())
        throw InvalidIndexOperator();
      if (entry->type.scalar_type == ScalarType::Int) {
        return CompileTimeValueAny{
            std::get<std::vector<int32_t>>(entry->const_init)[0]};
      } else {
        return CompileTimeValueAny{
            std::get<std::vector<float>>(entry->const_init)[0]};
      }
    }
    vector<MemSize> index;
    for (auto i : ctx->exp()) {
      CompileTimeValue<int32_t> cur = i->accept(this).as<CompileTimeValueAny>();
      if (cur.value < 0)
        throw CompileTimeValueEvalFail(
            "negative array index for compile-time constant");
      index.push_back(static_cast<MemSize>(cur.value));
    }
    if (!entry->type.check_index(index))
      throw CompileTimeValueEvalFail(
          "invalid array index for compile-time constant");
    if (entry->type.scalar_type == ScalarType::Int) {
      return CompileTimeValueAny(std::get<std::vector<int32_t>>(
          entry->const_init)[entry->type.get_index(index)]);
    } else {
      return CompileTimeValueAny(std::get<std::vector<float>>(
          entry->const_init)[entry->type.get_index(index)]);
    }
  } else {
    IR::Reg addr = new_reg();
    if (entry->arg_id >= 0) {
      cur_bb->push(new IR::LoadArg(addr, entry->arg_id));
    } else {
      cur_bb->push(new IR::LoadAddr(addr, entry->ir_obj));
    }
    if (!entry->type.is_array()) {
      if (ctx->exp().size())
        throw InvalidIndexOperator();
      IRValue ret(entry->type.scalar_type);
      ret.type = entry->type;
      ret.is_left_value = true;
      ret.reg = addr;
      return ret;
    }
    if (ctx->exp().size() > entry->type.count_array_dims())
      throw InvalidIndexOperator();
    Type new_type = entry->type;
    MemSize step_size = 4;
    size_t cur = ctx->exp().size() - 1;
    if (entry->type.omit_first_dim)
      --cur;
    for (size_t i = cur + 1; i < entry->type.array_dims.size(); ++i)
      step_size *= entry->type.array_dims[i];
    for (size_t i = ctx->exp().size() - 1; i < ctx->exp().size(); --i) {
      new_type = new_type.deref_one_dim();
      IR::Reg cur_index = get_value(to_IRValue(ctx->exp()[i]->accept(this)));
      IR::Reg new_addr = new_reg();
      cur_bb->push(
          new IR::ArrayIndex(new_addr, addr, cur_index, step_size, -1));
      if (cur < entry->type.array_dims.size()) {
        step_size *= entry->type.array_dims[cur];
        --cur;
      }
      addr = new_addr;
    }
    IRValue ret(entry->type.scalar_type);
    ret.type = new_type;
    ret.is_left_value = true;
    ret.reg = addr;
    return ret;
  }
}

antlrcpp::Any
ASTVisitor::visitPrimaryExp1(SysYParser::PrimaryExp1Context *ctx) {
  return ctx->exp()->accept(this);
}

antlrcpp::Any
ASTVisitor::visitPrimaryExp2(SysYParser::PrimaryExp2Context *ctx) {
  return ctx->lVal()->accept(this);
}

antlrcpp::Any
ASTVisitor::visitPrimaryExp3(SysYParser::PrimaryExp3Context *ctx) {
  auto literal_value = ctx->number()->accept(this);
  if (literal_value.is<int32_t>()) {
    if (mode == compile_time) {
      return CompileTimeValueAny(literal_value.as<int32_t>());
    } else {
      IR::Reg value = new_reg();
      cur_bb->push(new IR::LoadConst(value, literal_value.as<int32_t>()));
      IRValue ret(ScalarType::Int);
      ret.is_left_value = false;
      ret.reg = value;
      return ret;
    }
  }
  if (literal_value.is<float>()) {
    if (mode == compile_time) {
      return CompileTimeValueAny(literal_value.as<float>());
    } else {
      IR::Reg value = new_reg();
      cur_bb->push(new IR::LoadConst(value, literal_value.as<float>()));
      IRValue ret(ScalarType::Float);
      ret.is_left_value = false;
      ret.reg = value;
      return ret;
    }
  } else {
    throw InvalidLiteral(ctx->number()->getText());
  }
}

antlrcpp::Any ASTVisitor::visitNumber(SysYParser::NumberContext *ctx) {
  if (ctx->IntLiteral() != 0) {
    return parse_int32_literal(ctx->getText());
  } else {
    return parse_float_literal(ctx->FloatLiteral()->getText());
    // int32_t(stof(ctx->FloatLiteral()->getText()));
  }
}

antlrcpp::Any ASTVisitor::visitUnary1(SysYParser::Unary1Context *ctx) {
  return ctx->primaryExp()->accept(this);
}

antlrcpp::Any ASTVisitor::visitUnary2(SysYParser::Unary2Context *ctx) {
  if (mode == compile_time)
    throw CompileTimeValueEvalFail(
        "function call occurs in compile-time constant expression");
  string func_name = ctx->Identifier()->getText();
  FunctionTableEntry *entry = functions.resolve(func_name);
  if (!entry)
    throw UnrecognizedFuncName(func_name);
  vector<variant<IR::MemObject *, IRValue>> args;
  if (ctx->funcRParams())
    args = ctx->funcRParams()
               ->accept(this)
               .as<vector<variant<IR::MemObject *, IRValue>>>();
  if (args.size() < entry->interface.args_type.size())
    throw InvalidFuncCallArg("wrong number of function arguments");
  if (args.size() > entry->interface.args_type.size() &&
      !entry->interface.variadic)
    throw InvalidFuncCallArg("wrong number of function arguments");
  vector<IR::Reg> arg_regs;
  for (size_t i = 0; i < args.size(); ++i) {
    if (IRValue *cur = std::get_if<IRValue>(&args[i])) {
      if (i < entry->interface.args_type.size()) {
        if (Type *interface_type =
                std::get_if<Type>(&entry->interface.args_type[i])) {
          if (!interface_type->check_assign(cur->type))
            throw InvalidFuncCallArg("type error on function argument");
          if (interface_type->is_array())
            arg_regs.push_back(cur->reg);
          else
            arg_regs.push_back(get_value(*cur));
        } else
          throw InvalidFuncCallArg("type error on function argument");
      } else {
        arg_regs.push_back(get_value(*cur));
      }
    } else {
      IR::MemObject *&cur_str = std::get<IR::MemObject *>(args[i]);
      if (i < entry->interface.args_type.size()) {
        if (std::get_if<StringType>(&entry->interface.args_type[i])) {
          IR::Reg addr = new_reg();
          cur_bb->push(new IR::LoadAddr(addr, cur_str));
          arg_regs.push_back(addr);
        } else
          throw InvalidFuncCallArg("type error on function argument");
      } else {
        IR::Reg addr = new_reg();
        cur_bb->push(new IR::LoadAddr(addr, cur_str));
        arg_regs.push_back(addr);
      }
    }
  }
  if (func_name == "starttime" || func_name == "stoptime") {
    int32_t line_no = static_cast<int32_t>(ctx->start->getLine());
    IR::Reg line_no_reg = new_reg();
    cur_bb->push(new IR::LoadConst(line_no_reg, line_no));
    arg_regs.push_back(line_no_reg);
  }
  if (entry->interface.return_type != ScalarType::Void) {
    IR::Reg return_value = new_reg();
    cur_bb->push(
        new IR::CallInstr(return_value, entry->ir_func, arg_regs, false));
    auto interface = functions.resolve(entry->ir_func->name)->interface;
    IRValue ret(interface.return_type);
    ret.is_left_value = false;
    ret.reg = return_value;
    return ret;
  } else {
    cur_bb->push(new IR::CallInstr(new_reg(), entry->ir_func, arg_regs, true));
    return nullptr;
  }
}

antlrcpp::Any ASTVisitor::visitUnary3(SysYParser::Unary3Context *ctx) {
  char op = ctx->unaryOp()->getText()[0];
  assert(op == '+' || op == '-' || op == '!');
  if (mode == compile_time) {
    auto ret = ctx->unaryExp()->accept(this);
    auto rhs = ret.as<CompileTimeValueAny>();
    switch (op) {
    case '-':
      rhs = -rhs;
      break;
    case '!':
      rhs = !rhs;
      break;
    default:; //+, do nothing
    }
    return rhs;
  } else if (mode == normal) {
    IRValue rhs = to_IRValue(ctx->unaryExp()->accept(this));
    ScalarType type = rhs.type.scalar_type;
    if (op == '!')
      type = ScalarType::Int;
    IR::Reg rhs_value = get_value(type, rhs);
    IR::Reg res_value = rhs_value;
    IRValue ret(type);
    ret.is_left_value = false;
    switch (op) {
    case '-':
      res_value = new_reg();
      cur_bb->push(new IR::UnaryOpInstr(res_value, rhs_value,
                                        IR::UnaryOp(type, IR::UnaryOp::NEG)));
      break;
    case '!':
      res_value = new_reg();
      cur_bb->push(new IR::UnaryOpInstr(res_value, rhs_value,
                                        IR::UnaryOp(type, IR::UnaryOp::LNOT)));
      break;
    default:; //+, do nothing
    }
    ret.reg = res_value;
    return ret;
  } else {
    if (op == '!') {
      CondJumpList rhs = to_CondJumpList(ctx->unaryExp()->accept(this));
      std::swap(rhs.true_list, rhs.false_list);
      return rhs;
    } else {
      ValueMode prev_mode = mode;
      mode = normal;
      IRValue rhs = to_IRValue(ctx->unaryExp()->accept(this));
      if (op == '+') {
        rhs.reg = get_value(rhs);
        rhs.is_left_value = false;
        mode = prev_mode;
        return rhs;
      } else {
        IR::Reg rhs_value = get_value(rhs);
        IR::Reg res_value = new_reg();
        cur_bb->push(new IR::UnaryOpInstr(res_value, rhs_value,
                                          IR::UnaryOp(IR::UnaryOp::NEG)));
        IRValue ret(rhs.type.scalar_type);
        ret.is_left_value = false;
        ret.reg = res_value;
        mode = prev_mode;
        return ret;
      }
    }
  }
}

antlrcpp::Any ASTVisitor::visitUnaryOp(SysYParser::UnaryOpContext *ctx) {
  return nullptr;
}

antlrcpp::Any
ASTVisitor::visitFuncRParams(SysYParser::FuncRParamsContext *ctx) {
  vector<variant<IR::MemObject *, IRValue>> ret;
  for (auto i : ctx->funcRParam()) {
    antlrcpp::Any cur = i->accept(this);
    if (cur.is<IRValue>())
      ret.emplace_back(cur.as<IRValue>());
    else
      ret.emplace_back(cur.as<IR::MemObject *>());
  }
  return ret;
}

antlrcpp::Any
ASTVisitor::visitExpAsRParam(SysYParser::ExpAsRParamContext *ctx) {
  assert(mode != compile_time);
  ValueMode prev_mode = mode;
  mode = normal;
  IRValue ret = to_IRValue(ctx->exp()->accept(this));
  mode = prev_mode;
  return ret;
}

antlrcpp::Any
ASTVisitor::visitStringAsRParam(SysYParser::StringAsRParamContext *ctx) {
  string string_literal = ctx->getText();
  char *buffer = new char[string_literal.length() + 1];
  char *i = buffer;
  for (char ch : string_literal)
    *(i++) = ch;
  *i = 0;
  IR::MemObject *ir_obj = ir.scope.new_MemObject(
      ".string_literal" + std::to_string(string_literal_n++));
  ir_obj->init(buffer, -1); // size unknown
  return ir_obj;
}

antlrcpp::Any ASTVisitor::visitMul1(SysYParser::Mul1Context *ctx) {
  return ctx->unaryExp()->accept(this);
}

antlrcpp::Any ASTVisitor::visitMul2(SysYParser::Mul2Context *ctx) {
  char op = ctx->children[1]->getText()[0];
  assert(op == '*' || op == '/' || op == '%');
  if (mode == compile_time) {
    CompileTimeValueAny lhs = ctx->mulExp()->accept(this),
                        rhs = ctx->unaryExp()->accept(this), res;
    switch (op) {
    case '*':
      res = lhs * rhs;
      break;
    case '/':
      res = lhs / rhs;
      break;
    case '%':
      res = lhs % rhs;
      break;
    }
    return res;
  }
  ValueMode prev_mode = mode;
  mode = normal;
  IRValue lhs = to_IRValue(ctx->mulExp()->accept(this)),
          rhs = to_IRValue(ctx->unaryExp()->accept(this));
  ScalarType type = (lhs.type.scalar_type == ScalarType::Float ||
                             rhs.type.scalar_type == ScalarType::Float
                         ? ScalarType::Float
                         : ScalarType::Int);
  IR::Reg lhs_reg = get_value(type, lhs), rhs_reg = get_value(type, rhs),
          res_reg = new_reg();
  switch (op) {
  case '*':
    cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                       IR::BinaryOp(type, IR::BinaryOp::MUL)));
    break;
  case '/':
    cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                       IR::BinaryOp(type, IR::BinaryOp::DIV)));
    break;
  case '%':
    cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                       IR::BinaryOp(type, IR::BinaryOp::MOD)));
    break;
  }
  IRValue ret(lhs.type.scalar_type);
  ret.is_left_value = false;
  ret.reg = res_reg;
  mode = prev_mode;
  return ret;
}

antlrcpp::Any ASTVisitor::visitAdd1(SysYParser::Add1Context *ctx) {
  return ctx->mulExp()->accept(this);
}

antlrcpp::Any ASTVisitor::visitAdd2(SysYParser::Add2Context *ctx) {
  char op = ctx->children[1]->getText()[0];
  assert(op == '+' || op == '-');
  if (mode == compile_time) {
    CompileTimeValueAny lhs = ctx->addExp()->accept(this),
                        rhs = ctx->mulExp()->accept(this), res;
    switch (op) {
    case '+':
      res = lhs + rhs;
      break;
    case '-':
      res = lhs - rhs;
      break;
    }
    return res;
  }
  ValueMode prev_mode = mode;
  mode = normal;
  IRValue lhs = to_IRValue(ctx->addExp()->accept(this)),
          rhs = to_IRValue(ctx->mulExp()->accept(this));
  ScalarType type = (lhs.type.scalar_type == ScalarType::Float ||
                             rhs.type.scalar_type == ScalarType::Float
                         ? ScalarType::Float
                         : ScalarType::Int);
  IR::Reg lhs_reg = get_value(type, lhs), rhs_reg = get_value(type, rhs),
          res_reg = new_reg();
  switch (op) {
  case '+':
    cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                       IR::BinaryOp(type, IR::BinaryOp::ADD)));
    break;
  case '-':
    cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                       IR::BinaryOp(type, IR::BinaryOp::SUB)));
    break;
  }
  IRValue ret(lhs.type.scalar_type);
  ret.is_left_value = false;
  ret.reg = res_reg;
  mode = prev_mode;
  return ret;
}

antlrcpp::Any ASTVisitor::visitRel1(SysYParser::Rel1Context *ctx) {
  debug << __FUNCTION__ << '\n';
  return ctx->addExp()->accept(this);
}

antlrcpp::Any ASTVisitor::visitRel2(SysYParser::Rel2Context *ctx) {
  string ops = ctx->children[1]->getText();
  assert(ops == "<" || ops == ">" || ops == "<=" || ops == ">=");
  debug << __FUNCTION__ << " op: " << ops << '\n';
  IR::BinaryOp::Type opt;
  bool rev;
  if (ops == "<") {
    opt = IR::BinaryOp::LESS;
    rev = false;
  } else if (ops == ">") {
    opt = IR::BinaryOp::LESS;
    rev = true;
  } else if (ops == "<=") {
    opt = IR::BinaryOp::LEQ;
    rev = false;
  } else {
    opt = IR::BinaryOp::LEQ;
    rev = true;
  }
  if (mode == compile_time) {
    CompileTimeValueAny lhs = ctx->relExp()->accept(this),
                        rhs = ctx->addExp()->accept(this), res;
    if (rev)
      std::swap(lhs, rhs);
    if (opt == IR::BinaryOp::LESS)
      res = (lhs < rhs);
    else
      res = (lhs <= rhs);
    return res;
  }
  ValueMode prev_mode = mode;
  mode = normal;
  IRValue lhs = to_IRValue(ctx->relExp()->accept(this)),
          rhs = to_IRValue(ctx->addExp()->accept(this));
  ScalarType type = (lhs.type.scalar_type == ScalarType::Float ||
                             rhs.type.scalar_type == ScalarType::Float
                         ? ScalarType::Float
                         : ScalarType::Int);
  IR::Reg lhs_reg = get_value(type, lhs), rhs_reg = get_value(type, rhs),
          res_reg = new_reg();
  if (rev)
    std::swap(lhs_reg, rhs_reg);
  cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                     IR::BinaryOp(type, opt)));
  IRValue ret(ScalarType::Int);
  debug << __FUNCTION__ << " ret.type.scalar_type: " << ret.type.scalar_type
        << " == " << ScalarType::Int << '\n';
  debug << __FUNCTION__ << " ret.type: " << ret.type << '\n';
  ret.is_left_value = false;
  ret.reg = res_reg;
  mode = prev_mode;
  debug << __FUNCTION__ << " ret: " << ret << '\n';
  return ret;
}

antlrcpp::Any ASTVisitor::visitEq1(SysYParser::Eq1Context *ctx) {
  debug << __FUNCTION__ << '\n';
  auto ret = ctx->relExp()->accept(this);
  if (ret.is<IRValue>()) {
    debug << __FUNCTION__ << " ret: " << ret.as<IRValue>() << '\n';
  }
  return ret;
}

antlrcpp::Any ASTVisitor::visitEq2(SysYParser::Eq2Context *ctx) {
  string ops = ctx->children[1]->getText();
  assert(ops == "==" || ops == "!=");
  debug << __FUNCTION__ << " op: " << ops << '\n';
  IR::BinaryOp::Type opt;
  if (ops == "==")
    opt = IR::BinaryOp::EQ;
  else
    opt = IR::BinaryOp::NEQ;
  if (mode == compile_time) {
    CompileTimeValueAny lhs = ctx->eqExp()->accept(this),
                        rhs = ctx->relExp()->accept(this), res;
    if (opt == IR::BinaryOp::EQ)
      res = (lhs == rhs);
    else
      res = (lhs != rhs);
    return res;
  }
  ValueMode prev_mode = mode;
  mode = normal;
  IRValue lhs = to_IRValue(ctx->eqExp()->accept(this)),
          rhs = to_IRValue(ctx->relExp()->accept(this));
  ScalarType type = (lhs.type.scalar_type == ScalarType::Float ||
                             rhs.type.scalar_type == ScalarType::Float
                         ? ScalarType::Float
                         : ScalarType::Int);
  IR::Reg lhs_reg = get_value(type, lhs), rhs_reg = get_value(type, rhs),
          res_reg = new_reg();
  cur_bb->push(new IR::BinaryOpInstr(res_reg, lhs_reg, rhs_reg,
                                     IR::BinaryOp(type, opt)));
  IRValue ret(ScalarType::Int);
  ret.is_left_value = false;
  ret.reg = res_reg;
  mode = prev_mode;
  return ret;
}

antlrcpp::Any ASTVisitor::visitLAnd1(SysYParser::LAnd1Context *ctx) {
  debug << __FUNCTION__ << '\n';
  auto ret = ctx->eqExp()->accept(this);
  if (ret.is<IRValue>()) {
    debug << __FUNCTION__ << " ret: " << ret.as<IRValue>() << '\n';
  }
  return ret;
}

antlrcpp::Any ASTVisitor::visitLAnd2(SysYParser::LAnd2Context *ctx) {
  debug << __FUNCTION__ << '\n';
  if (mode == compile_time) {
    CompileTimeValue<int32_t>
        lhs = ctx->lAndExp()->accept(this).as<CompileTimeValueAny>(),
        rhs = ctx->eqExp()->accept(this).as<CompileTimeValueAny>();
    return lhs && rhs;
  } else if (mode == normal) {
    IRValue lhs = to_IRValue(ctx->lAndExp()->accept(this));
    IR::Reg lhs_value = get_value(lhs);
    IR::BB *lhs_end = cur_bb, *rhs_entry = new_BB(), *res_entry = new_BB();
    lhs_end->push(new IR::BranchInstr(lhs_value, rhs_entry, res_entry));
    cur_bb = rhs_entry;
    IRValue rhs = to_IRValue(ctx->eqExp()->accept(this));
    IR::Reg rhs_value = get_value(rhs);
    cur_bb->push(new IR::JumpInstr(res_entry));
    IR::BB *rhs_end = cur_bb;
    cur_bb = res_entry;
    IR::Reg res_reg = new_reg();
    IR::PhiInstr *phi_inst = new IR::PhiInstr(res_reg);
    phi_inst->uses.emplace_back(lhs_value, lhs_end);
    phi_inst->uses.emplace_back(rhs_value, rhs_end);
    cur_bb->push(phi_inst);
    IRValue ret(ScalarType::Int);
    ret.is_left_value = false;
    ret.reg = res_reg;
    return ret;
  } else {
    /*auto j1 = ctx->lAndExp()->accept(this);
        if(j1.is<IRValue>()){
                debug<<__FUNCTION__<<" j1: "<<j1.as<IRValue>()<<'\n';
        }else{
                debug<<__FUNCTION__<<" j1: "<<j1.isNull()<<'\n';
        }*/
    CondJumpList lhs = to_CondJumpList(ctx->lAndExp()->accept(this));
    cur_bb = new_BB();
    for (IR::BB **i : lhs.true_list)
      (*i) = cur_bb;
    CondJumpList rhs = to_CondJumpList(ctx->eqExp()->accept(this));
    CondJumpList ret = std::move(rhs);
    for (IR::BB **i : lhs.false_list)
      ret.false_list.push_back(i);
    // debug<<__FUNCTION__<<" ret: "<<ret<<'\n';
    return ret;
  }
}

antlrcpp::Any ASTVisitor::visitLOr1(SysYParser::LOr1Context *ctx) {
  debug << __FUNCTION__ << '\n';
  return ctx->lAndExp()->accept(this);
}

antlrcpp::Any ASTVisitor::visitLOr2(SysYParser::LOr2Context *ctx) {
  debug << __FUNCTION__ << '\n';
  if (mode == compile_time) {
    CompileTimeValue<int32_t>
        lhs = ctx->lOrExp()->accept(this).as<CompileTimeValueAny>(),
        rhs = ctx->lAndExp()->accept(this).as<CompileTimeValueAny>();
    return lhs || rhs;
  } else if (mode == normal) {
    IRValue lhs = to_IRValue(ctx->lOrExp()->accept(this));
    IR::Reg lhs_value = get_value(lhs);
    IR::BB *lhs_end = cur_bb, *rhs_entry = new_BB(), *res_entry = new_BB();
    lhs_end->push(new IR::BranchInstr(lhs_value, res_entry, rhs_entry));
    cur_bb = rhs_entry;
    IRValue rhs = to_IRValue(ctx->lAndExp()->accept(this));
    IR::Reg rhs_value = get_value(rhs);
    cur_bb->push(new IR::JumpInstr(res_entry));
    IR::BB *rhs_end = cur_bb;
    cur_bb = res_entry;
    IR::Reg res_reg = new_reg();
    IR::PhiInstr *phi_inst = new IR::PhiInstr(res_reg);
    phi_inst->uses.emplace_back(lhs_value, lhs_end);
    phi_inst->uses.emplace_back(rhs_value, rhs_end);
    cur_bb->push(phi_inst);
    IRValue ret(ScalarType::Int);
    ret.is_left_value = false;
    ret.reg = res_reg;
    return ret;
  } else {
    CondJumpList lhs = to_CondJumpList(ctx->lOrExp()->accept(this));
    cur_bb = new_BB();
    for (IR::BB **i : lhs.false_list)
      (*i) = cur_bb;
    CondJumpList rhs = to_CondJumpList(ctx->lAndExp()->accept(this));
    CondJumpList ret = std::move(rhs);
    for (IR::BB **i : lhs.true_list)
      ret.true_list.push_back(i);
    return ret;
  }
}

antlrcpp::Any ASTVisitor::visitConstExp(SysYParser::ConstExpContext *ctx) {
  assert(mode == normal);
  mode = compile_time;
  auto ret = ctx->addExp()->accept(this);
  mode = normal;
  return ret;
}
