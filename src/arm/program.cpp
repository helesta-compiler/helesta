#include "arm/program.hpp"

#include <bitset>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "arm/archinfo.hpp"
#include "arm/inst.hpp"
#include "arm/simple_coloring_alloc.hpp"
#include "common/common.hpp"

using std::deque;
using std::make_unique;
using std::map;
using std::ostream;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

namespace ARMv7 {

Block::Block(string _name) : name(_name), label_used(false) {}

void Block::construct(IR::BB *ir_bb, Func *func, MappingInfo *info,
                      Block *next_block, map<Reg, CmpInfo> &cmp_info) {
  for (auto &i : ir_bb->instrs) {
    IR::Instr *cur = i.get();
    // std::cerr << *cur << std::endl;
    if (auto loadaddr = dynamic_cast<IR::LoadAddr *>(cur)) {
      Reg dst = info->from_ir_reg(loadaddr->d1);
      if (loadaddr->offset->global) {
        push_back(load_symbol_addr(
            dst, mangle_global_var_name(loadaddr->offset->name)));
        func->symbol_reg[dst] = mangle_global_var_name(loadaddr->offset->name);
      } else {
        push_back(make_unique<LoadStackAddr>(
            dst, 0, info->obj_mapping[loadaddr->offset]));
        func->stack_addr_reg[dst] = std::pair<StackObject *, int32_t>{
            info->obj_mapping[loadaddr->offset], 0};
      }
    } else if (auto loadconst = dynamic_cast<IR::LoadConst<int32_t> *>(cur)) {
      Reg dst = info->from_ir_reg(loadconst->d1);
      func->constant_reg[dst] = loadconst->value;
      push_back(load_imm(dst, loadconst->value));
    } else if (auto loadconst = dynamic_cast<IR::LoadConst<float> *>(cur)) {
      int as_int;
      memcpy(&as_int, &loadconst->value, sizeof(float));
      auto tmp = info->new_reg();
      Reg dst = info->from_ir_reg(loadconst->d1);
      func->constant_reg[tmp] = as_int;
      push_back(load_imm(tmp, as_int));
      push_back(std::make_unique<MoveReg>(dst, tmp));
    } else if (auto loadarg = dynamic_cast<IR::LoadArg *>(cur)) {
      push_back(make_unique<MoveReg>(info->from_ir_reg(loadarg->d1),
                                     func->arg_reg[loadarg->id]));
    } else if (auto unary = dynamic_cast<IR::UnaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(unary->d1),
          src = info->from_ir_reg(unary->s1);
      switch (unary->op.type) {
      case IR::UnaryCompute::LNOT:
        push_back(make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(make_unique<RegImmCmp>(RegImmCmp::Cmp, src, 0));
        push_back(set_cond(make_unique<MoveImm>(MoveImm::Mov, dst, 1), Eq));
        // TODO: this can be done better with "rsbs dst, src, #0; adc dst,
        // src, dst" or "clz dst, src; lsr dst, dst, #5"
        break;
      case IR::UnaryCompute::NEG:
        push_back(make_unique<RegImmInst>(RegImmInst::RevSub, dst, src, 0));
        break;
      case IR::UnaryCompute::FNEG:
        info->set_float(dst);
        info->set_float(src);
        push_back(make_unique<FRegInst>(FRegInst::Neg, dst, src));
        break;
      case IR::UnaryCompute::ID:
        info->set_maybe_float_assign(dst, src);
        push_back(make_unique<MoveReg>(dst, src));
        break;
      case IR::UnaryCompute::I2F: {
        Reg tmp = info->new_reg();
        info->set_float(dst);
        info->set_float(tmp);
        push_back(make_unique<MoveReg>(tmp, src));
        push_back(make_unique<FRegInst>(FRegInst::I2F, dst, tmp));
        break;
      }
      case IR::UnaryCompute::F2I: {
        Reg tmp = info->new_reg();
        info->set_float(src);
        info->set_float(tmp);
        push_back(make_unique<FRegInst>(FRegInst::F2I, tmp, src));
        push_back(make_unique<MoveReg>(dst, tmp));
        break;
      }
      case IR::UnaryCompute::F2D0:
        info->set_float(dst);
        info->set_float(src);
        push_back(make_unique<FRegInst>(FRegInst::F2D0, dst, src));
        break;
      case IR::UnaryCompute::F2D1:
        info->set_float(dst);
        info->set_float(src);
        push_back(make_unique<FRegInst>(FRegInst::F2D1, dst, src));
        break;
      default:
        unreachable();
      }
    } else if (auto binary = dynamic_cast<IR::BinaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(binary->d1),
          s1 = info->from_ir_reg(binary->s1),
          s2 = info->from_ir_reg(binary->s2);
      if (binary->op.type == IR::BinaryCompute::ADD ||
          binary->op.type == IR::BinaryCompute::SUB ||
          binary->op.type == IR::BinaryCompute::MUL ||
          binary->op.type == IR::BinaryCompute::DIV) {
        push_back(make_unique<RegRegInst>(
            RegRegInst::from_ir_binary_op(binary->op.type), dst, s1, s2));
      } else if (binary->op.type == IR::BinaryCompute::FADD ||
                 binary->op.type == IR::BinaryCompute::FSUB ||
                 binary->op.type == IR::BinaryCompute::FMUL ||
                 binary->op.type == IR::BinaryCompute::FDIV) {
        info->set_float(dst);
        info->set_float(s1);
        info->set_float(s2);
        push_back(make_unique<FRegRegInst>(
            FRegRegInst::from_ir_binary_op(binary->op.type), dst, s1, s2));
      } else if (binary->op.type == IR::BinaryCompute::LESS ||
                 binary->op.type == IR::BinaryCompute::LEQ ||
                 binary->op.type == IR::BinaryCompute::EQ ||
                 binary->op.type == IR::BinaryCompute::NEQ) {
        push_back(make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(make_unique<RegRegCmp>(RegRegCmp::Cmp, s1, s2));
        push_back(set_cond(make_unique<MoveImm>(MoveImm::Mov, dst, 1),
                           from_ir_binary_op(binary->op.type)));
        cmp_info[dst].cond = from_ir_binary_op(binary->op.type);
        cmp_info[dst].lhs = s1;
        cmp_info[dst].rhs = s2;
        cmp_info[dst].is_float = 0;
      } else if (binary->op.type == IR::BinaryCompute::FLESS ||
                 binary->op.type == IR::BinaryCompute::FLEQ ||
                 binary->op.type == IR::BinaryCompute::FEQ ||
                 binary->op.type == IR::BinaryCompute::FNEQ) {
        info->set_float(s1);
        info->set_float(s2);
        push_back(make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(make_unique<FRegRegCmp>(s1, s2));
        push_back(set_cond(make_unique<MoveImm>(MoveImm::Mov, dst, 1),
                           from_ir_binary_op(binary->op.type)));
        cmp_info[dst].cond = from_ir_binary_op(binary->op.type);
        cmp_info[dst].lhs = s1;
        cmp_info[dst].rhs = s2;
        cmp_info[dst].is_float = 1;
      } else if (binary->op.type == IR::BinaryCompute::MOD) {
        Reg k = info->new_reg();
        push_back(make_unique<RegRegInst>(RegRegInst::Div, k, s1, s2));
        push_back(make_unique<ML>(ML::Mls, dst, s2, k, s1));
      } else
        unreachable();
    } else if (auto load = dynamic_cast<IR::LoadInstr *>(cur)) {
      Reg dst = info->from_ir_reg(load->d1),
          addr = info->from_ir_reg(load->addr);
      push_back(make_unique<Load>(dst, addr, 0));
    } else if (auto store = dynamic_cast<IR::StoreInstr *>(cur)) {
      Reg addr = info->from_ir_reg(store->addr),
          src = info->from_ir_reg(store->s1);
      push_back(make_unique<Store>(src, addr, 0));
    } else if (auto jump = dynamic_cast<IR::JumpInstr *>(cur)) {
      Block *jump_target = info->block_mapping[jump->target];
      if (jump_target != next_block)
        push_back(make_unique<Branch>(jump_target));
      out_edge.push_back(jump_target);
      jump_target->in_edge.push_back(this);
    } else if (auto branch = dynamic_cast<IR::BranchInstr *>(cur)) {
      Reg cond = info->from_ir_reg(branch->cond);
      Block *true_target = info->block_mapping[branch->target1],
            *false_target = info->block_mapping[branch->target0];
      if (cmp_info.find(cond) != cmp_info.end()) {
        if (cmp_info[cond].is_float) {
          push_back(
              make_unique<FRegRegCmp>(cmp_info[cond].lhs, cmp_info[cond].rhs));
        } else {
          push_back(make_unique<RegRegCmp>(RegRegCmp::Cmp, cmp_info[cond].lhs,
                                           cmp_info[cond].rhs));
        }
        if (false_target == next_block)
          push_back(
              set_cond(make_unique<Branch>(true_target), cmp_info[cond].cond));
        else if (true_target == next_block)
          push_back(set_cond(make_unique<Branch>(false_target),
                             logical_not(cmp_info[cond].cond)));
        else {
          push_back(
              set_cond(make_unique<Branch>(true_target), cmp_info[cond].cond));
          push_back(make_unique<Branch>(false_target));
        }
      } else {
        push_back(make_unique<RegImmCmp>(RegImmCmp::Cmp, cond, 0));
        if (false_target == next_block)
          push_back(set_cond(make_unique<Branch>(true_target), Ne));
        else if (true_target == next_block)
          push_back(set_cond(make_unique<Branch>(false_target), Eq));
        else {
          push_back(set_cond(make_unique<Branch>(true_target), Ne));
          push_back(make_unique<Branch>(false_target));
        }
      }
      out_edge.push_back(true_target);
      out_edge.push_back(false_target);
      true_target->in_edge.push_back(this);
      false_target->in_edge.push_back(this);
    } else if (auto ret = dynamic_cast<IR::ReturnInstr *>(cur)) {
      if (ret->ignore_return_value) {
        push_back(make_unique<Return>(false));
      } else {
        push_back(make_unique<MoveReg>(Reg{ARGUMENT_REGISTERS[0]},
                                       info->from_ir_reg(ret->s1)));
        push_back(make_unique<Return>(true));
      }
    } else if (auto call = dynamic_cast<IR::CallInstr *>(cur)) {
      for (size_t i = call->args.size() - 1; i < call->args.size(); --i)
        if (static_cast<int>(i) >= ARGUMENT_REGISTER_COUNT) {
          push_back(
              make_unique<Push>(vector<Reg>{info->from_ir_reg(call->args[i])}));
        } else {
          push_back(make_unique<MoveReg>(Reg{ARGUMENT_REGISTERS[i]},
                                         info->from_ir_reg(call->args[i])));
        }
      if (call->f->name == "putfloat") {
        push_back(make_unique<MoveReg>(Reg(0, 1), Reg{ARGUMENT_REGISTERS[0]}));
      }
      push_back(make_unique<FuncCall>(call->f->name,
                                      static_cast<int>(call->args.size())));
      if (static_cast<int>(call->args.size()) > ARGUMENT_REGISTER_COUNT)
        push_back(sp_move(
            (static_cast<int>(call->args.size()) - ARGUMENT_REGISTER_COUNT) *
            INT_SIZE));
      if (!call->ignore_return_value) {
        Reg ret{ARGUMENT_REGISTERS[0]};
        if (call->f->name == "getfloat") {
          ret = Reg(0, 1);
        }
        push_back(make_unique<MoveReg>(info->from_ir_reg(call->d1), ret));
      }
      if (call->f->name == "__create_threads") {
        func->spilling_reg.insert(info->from_ir_reg(call->d1));
        debug << "thread_id: " << call->d1 << " -> "
              << info->from_ir_reg(call->d1) << " is forbidden to be spilled\n";
      }
    } else if (dynamic_cast<IR::LocalVarDef *>(cur)) {
      // do nothing
    } else if (auto array_index = dynamic_cast<IR::ArrayIndex *>(cur)) {
      Reg dst = info->from_ir_reg(array_index->d1),
          s1 = info->from_ir_reg(array_index->s1),
          s2 = info->from_ir_reg(array_index->s2);
      // TODO: optimize when size=2^k
      if (func->constant_reg.count(s2)) {
        int32_t v2 = func->constant_reg[s2] * array_index->size;
        if (is_legal_immediate(v2)) {
          push_back(make_unique<RegImmInst>(RegImmInst::Add, dst, s1, v2));
          continue;
        }
      }

      if (array_index->size == 4) {
        push_back(make_unique<RegRegInst>(RegRegInst::Add, dst, s1, s2,
                                          Shift(Shift::LSL, 2)));
      } else {
        Reg step = info->new_reg();
        push_back(load_imm(step, array_index->size));
        push_back(make_unique<ML>(ML::Mla, dst, s2, step, s1));
      }
    } else if (dynamic_cast<IR::PhiInstr *>(cur)) {
      // do nothing
    } else
      unreachable();
  }
}

void Block::push_back(unique_ptr<Inst> inst) {
  insts.push_back(std::move(inst));
}

void Block::push_back(std::list<unique_ptr<Inst>> inst_list) {
  for (auto &i : inst_list) {
    insts.push_back(std::move(i));
  }
}

void Block::insert_before_jump(unique_ptr<Inst> inst) {
  auto i = insts.end();
  while (i != insts.begin()) {
    auto prev_i = std::prev(i);
    if ((*prev_i)->as<Branch>()) {
      i = prev_i;
    } else {
      break;
    }
  }
  insts.insert(i, std::move(inst));
}

void Block::gen_asm(ostream &out, AsmContext *ctx) {
  ctx->temp_sp_offset = 0;
  if (label_used)
    out << name << ":\n";
  for (auto &i : insts)
    i->gen_asm(out, ctx);
}

void Block::print(ostream &out) {
  out << '\n' << name << ":\n";
  for (auto &i : insts)
    i->print(out);
}

MappingInfo::MappingInfo() : reg_n(RegCount) {}

Reg MappingInfo::new_reg() { return Reg{reg_n++, 0}; }

Reg MappingInfo::from_ir_reg(IR::Reg ir_reg) {
  auto it = reg_mapping.find(ir_reg.id);
  if (it != reg_mapping.end()) {
    Reg ret = it->second;
    return ret;
  }
  Reg ret = new_reg();
  reg_mapping[ir_reg.id] = ret;
  return ret;
}
void MappingInfo::set_float(Reg reg) {
  if (!float_regs.insert(reg).second)
    return;
  // std::cerr << reg.id << ": float\n";
  for (Reg r : maybe_float_assign[reg]) {
    set_float(r);
  }
  maybe_float_assign.erase(reg);
}
void MappingInfo::set_maybe_float_assign(Reg &r1, Reg &r2) {
  // std::cerr << r1.id << ',' << r2.id << ": =\n";
  if (float_regs.count(r1) || float_regs.count(r2)) {
    set_float(r1);
    set_float(r2);
  } else {
    maybe_float_assign[r1].push_back(r2);
    maybe_float_assign[r2].push_back(r1);
  }
}

Func::Func(Program *prog, std::string _name, IR::NormalFunc *ir_func)
    : name(_name), entry(nullptr), reg_n(0) {
  MappingInfo info;
  for (size_t i = 0; i < ir_func->scope.objects.size(); ++i) {
    IR::MemObject *cur = ir_func->scope.objects[i].get();
    if (cur->size == 0)
      continue;
    unique_ptr<StackObject> res = make_unique<StackObject>();
    res->size = cur->size;
    res->position = -1;
    info.obj_mapping[cur] = res.get();
    stack_objects.push_back(std::move(res));
  }
  entry = new Block(".entry_" + name);
  blocks.emplace_back(entry);
  for (size_t i = 0; i < ir_func->bbs.size(); ++i) {
    IR::BB *cur = ir_func->bbs[i].get();
    string cur_name = ".L" + std::to_string(prog->block_n++);
    unique_ptr<Block> res = make_unique<Block>(cur_name);
    info.block_mapping[cur] = res.get();
    info.rev_block_mapping[res.get()] = cur;
    blocks.push_back(std::move(res));
  }
  int arg_n = 0;
  for (auto &bb : ir_func->bbs)
    for (auto &inst : bb->instrs)
      if (auto *cur = dynamic_cast<IR::LoadArg *>(inst.get()))
        arg_n = std::max(arg_n, cur->id + 1);
  for (int i = 0; i < arg_n; ++i) {
    Reg cur_arg = info.new_reg();
    if (i < ARGUMENT_REGISTER_COUNT) {
      entry->push_back(
          make_unique<MoveReg>(cur_arg, Reg{ARGUMENT_REGISTERS[i]}));
    } else {
      unique_ptr<StackObject> t = make_unique<StackObject>();
      t->size = INT_SIZE;
      t->position = -1;
      entry->push_back(make_unique<LoadStack>(cur_arg, 0, t.get()));
      caller_stack_object.push_back(std::move(t));
    }
    arg_reg.push_back(cur_arg);
  }
  Block *real_entry = info.block_mapping[ir_func->entry];
  if (blocks[1].get() != real_entry)
    entry->push_back(make_unique<Branch>(real_entry));
  entry->out_edge.push_back(real_entry);
  real_entry->in_edge.push_back(entry);
  map<Reg, CmpInfo> cmp_info;
  for (size_t i = 0; i < blocks.size(); ++i)
    if (blocks[i].get() != entry) {
      IR::BB *cur_ir_bb = info.rev_block_mapping[blocks[i].get()];
      Block *next_block = nullptr;
      if (i + 1 < blocks.size())
        next_block = blocks[i + 1].get();
      blocks[i]->construct(cur_ir_bb, this, &info, next_block,
                           cmp_info); // maintain in_edge, out_edge,
                                      // reg_mapping, ignore phi function
    }
  struct PendingMove {
    Block *block;
    Reg to, from;
  };
  vector<PendingMove> pending_moves;
  for (auto &bb : ir_func->bbs)
    for (auto &inst : bb->instrs)
      if (auto *cur = dynamic_cast<IR::PhiInstr *>(inst.get()))
        for (auto &prev : cur->uses) {
          Block *b = info.block_mapping[prev.second];
          Reg mid = info.new_reg();
          b->insert_before_jump(
              make_unique<MoveReg>(mid, info.from_ir_reg(prev.first.id)));
          pending_moves.push_back({b, info.from_ir_reg(cur->d1.id), mid});
        }
  for (PendingMove &i : pending_moves)
    i.block->insert_before_jump(make_unique<MoveReg>(i.to, i.from));
  reg_n = info.reg_n;
  float_regs = std::move(info.float_regs);
  for (auto &block : blocks) {
    for (auto &inst : block->insts) {
      for (Reg *r : inst->regs()) {
        if (r->is_pseudo())
          r->is_float = float_regs.count(*r);
      }
    }
  }
  merge_inst();
  dce();
}

std::pair<int64_t, int> div_opt(int32_t A0) {
  int ex = __builtin_ctz(A0);
  int32_t A = A0 >> ex;
  int64_t L = 1ll << 32;
  int log2L = 32;
  while (L / (A - L % A) < (1ll << 31))
    L <<= 1, ++log2L;
  int64_t B = L / A + 1;
  int s = ex + log2L - 32;
  assert(0 <= B && B < (1ll << 32));
  // std::cerr << ">>> div_any: " << A0 << ' ' << B << ' ' << s << std::endl;
  return {B, s};
}

void Func::merge_inst() {
  for (auto &block : blocks) {
    auto &insts = block->insts;
    for (auto it = insts.begin(); it != insts.end(); ++it) {
      visit(insts, it);
      Inst *inst = it->get();
      if (auto bop = dynamic_cast<RegRegInst *>(inst)) {
        if (bop->shift.w)
          continue;
        if (!constant_reg.count(bop->rhs))
          continue;
        int32_t v = constant_reg[bop->rhs];
        switch (bop->op) {
        case RegRegInst::Add:
        case RegRegInst::Sub: {
          auto op =
              bop->op == RegRegInst::Add ? RegImmInst::Add : RegImmInst::Sub;
          if (is_legal_immediate(v)) {
            RegImm(op, bop->dst, bop->lhs, v);
            Del();
          }
          break;
        }
        case RegRegInst::Mul: {
          if (v <= 1)
            break;
          int32_t log2v = __builtin_ctz(v);
          int32_t v0 = 1 << log2v;
          if (v == v0) {
            RegImm(RegImmInst::Lsl, bop->dst, bop->lhs, log2v);
            Del();
          } else if (__builtin_popcount(v - v0) == 1) {
            int32_t s = __builtin_ctz(v - v0);
            RegReg(RegRegInst::Add, bop->dst, bop->lhs, bop->lhs,
                   Shift(Shift::LSL, s - log2v));
            if (log2v)
              RegImm(RegImmInst::Lsl, bop->dst, bop->dst, log2v);
            Del();
          } else if (__builtin_popcount(v + v0) == 1) {
            int32_t s = __builtin_ctz(v + v0);
            RegReg(RegRegInst::RevSub, bop->dst, bop->lhs, bop->lhs,
                   Shift(Shift::LSL, s - log2v));
            if (log2v)
              RegImm(RegImmInst::Lsl, bop->dst, bop->dst, log2v);
            Del();
          }
          break;
        }
        case RegRegInst::Div:
          if (v > 1 && v == (v & -v)) {
            int32_t log2v = __builtin_ctz(v);
            assert(v == (1 << log2v));
            auto r0 = bop->lhs;
            auto r1 = bop->dst;
            auto r2 = bop->dst;
            if (v == 2) {
              RegReg(RegRegInst::Add, r2, r0, r0, Shift(Shift::LSR, 31));
            } else {
              RegImm(RegImmInst::Asr, r1, r0, 31);
              RegReg(RegRegInst::Add, r2, r0, r1,
                     Shift(Shift::LSR, 32 - log2v));
            }
            RegImm(RegImmInst::Asr, bop->dst, r2, log2v);
            Del();
          } else if (v > 1) {
            auto [B, s] = div_opt(v);
            Reg lo = Reg{r4};
            Reg hi = Reg{r5};
            int32_t B0 = B & 0x7fffffff;
            Reg x = bop->lhs;
            Ins(load_imm(lo, B0));
            Ins(new SMulL(lo, hi, x, lo));
            if (B & (1ll << 31)) {
              RegReg(RegRegInst::Add, hi, hi, x, Shift(Shift::ASR, 1));
              RegReg(RegRegInst::And, lo, x, lo, Shift(Shift::LSR, 31));
              RegReg(RegRegInst::Add, hi, hi, lo);
            }
            RegImm(RegImmInst::Asr, bop->dst, hi, s);
            RegReg(RegRegInst::Add, bop->dst, bop->dst, hi,
                   Shift(Shift::LSR, 31));
            Del();
          }
          break;
        case RegRegInst::Mod:
          if (v > 1 && v == (v & -v)) {
            int32_t log2v = __builtin_ctz(v);
            assert(v == (1 << log2v));
            auto r0 = bop->lhs;
            auto r1 = bop->dst;
            auto r2 = bop->dst;
            auto r3 = bop->dst;
            if (v == 2) {
              RegReg(RegRegInst::Add, r2, r0, r0, Shift(Shift::LSR, 31));
            } else {
              RegImm(RegImmInst::Asr, r1, r0, 31);
              RegReg(RegRegInst::Add, r2, r0, r1,
                     Shift(Shift::LSR, 32 - log2v));
            }
            if (is_legal_immediate(v - 1)) {
              RegImm(RegImmInst::Bic, r3, r2, v - 1);
              RegReg(RegRegInst::Sub, bop->dst, r0, r3);
            } else {
              RegImm(RegImmInst::Lsr, r3, r2, log2v);
              RegReg(RegRegInst::Sub, bop->dst, r0, r3,
                     Shift(Shift::LSL, log2v));
            }
            Del();
          } else if (v > 1) {
            auto [B, s] = div_opt(v);
            Reg lo = Reg{r4};
            Reg hi = Reg{r5};
            int32_t B0 = B & 0x7fffffff;
            Reg x = bop->lhs;
            Ins(load_imm(lo, B0));
            Ins(new SMulL(lo, hi, x, lo));
            if (B & (1ll << 31)) {
              RegReg(RegRegInst::Add, hi, hi, x, Shift(Shift::ASR, 1));
              RegReg(RegRegInst::And, lo, x, lo, Shift(Shift::LSR, 31));
              RegReg(RegRegInst::Add, hi, hi, lo);
            }
            RegImm(RegImmInst::Asr, bop->dst, hi, s);
            RegReg(RegRegInst::Add, bop->dst, bop->dst, hi,
                   Shift(Shift::LSR, 31));
            Ins(load_imm(lo, v));
            RegReg(RegRegInst::Mul, bop->dst, bop->dst, lo);
            RegReg(RegRegInst::Sub, bop->dst, x, bop->dst);
            Del();
          }
          break;
        default:
          break;
        }
      }
    }
  }
  for (auto &block : blocks) {
    auto &insts = block->insts;
    for (auto it = insts.begin(); it != insts.end(); ++it) {
      Inst *inst = it->get();
      if (auto bop = dynamic_cast<RegRegInst *>(inst)) {
        if (bop->op == RegRegInst::Mod) {
          auto dst = bop->dst;
          auto s1 = bop->lhs;
          auto s2 = bop->rhs;
          insts.insert(it,
                       make_unique<RegRegInst>(RegRegInst::Div, dst, s1, s2));
          *it = make_unique<ML>(ML::Mls, dst, s2, dst, s1);
        }
      }
    }
  }
}

template <class T, class F>
void reverse_for_each_del(std::list<T> &ls, const F &f) {
  for (auto it = ls.end(); it != ls.begin();) {
    auto it0 = it;
    if (f(*--it)) {
      ls.erase(it);
      it = it0;
    }
  }
}

void Func::dce() {
  calc_live();
  for (auto &block : blocks) {
    auto live = block->live_out;
    bool use_cpsr = false;
    reverse_for_each_del(block->insts, [&](std::unique_ptr<Inst> &cur) -> bool {
      bool used = cur->side_effect();
      used |= (cur->change_cpsr() && use_cpsr);
      for (Reg r : cur->def_reg())
        if ((r.is_machine() && !allocable(r.id)) || live.count(r))
          used = true;
      if (!used)
        return 1;
      use_cpsr &= !cur->change_cpsr();
      use_cpsr |= cur->use_cpsr();
      cur->update_live(live);
      return 0;
    });
  }
}

void Func::erase_def_use(const OccurPoint &p, Inst *inst) {
  for (Reg r : inst->def_reg())
    reg_def[r.id].erase(p);
  for (Reg r : inst->use_reg())
    reg_use[r.id].erase(p);
}

void Func::add_def_use(const OccurPoint &p, Inst *inst) {
  for (Reg r : inst->def_reg())
    reg_def[r.id].insert(p);
  for (Reg r : inst->use_reg())
    reg_use[r.id].insert(p);
}

void Func::build_def_use() {
  reg_def.clear();
  reg_use.clear();
  reg_def.resize(reg_n);
  reg_use.resize(reg_n);
  OccurPoint p;
  for (auto &block : blocks) {
    p.b = block.get();
    p.pos = 0;
    for (p.it = block->insts.begin(); p.it != block->insts.end();
         ++p.it, ++p.pos) {
      add_def_use(p, p.it->get());
    }
  }
}

void Func::calc_live() {
  deque<pair<Block *, Reg>> update;
  for (auto &block : blocks) {
    block->live_use.clear();
    block->def.clear();
    for (auto it = block->insts.rbegin(); it != block->insts.rend(); ++it) {
      for (Reg r : (*it)->def_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          block->live_use.erase(r);
          block->def.insert(r);
        }
      for (Reg r : (*it)->use_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          block->def.erase(r);
          block->live_use.insert(r);
        }
    }
    for (Reg r : block->live_use)
      update.emplace_back(block.get(), r);
    block->live_in = block->live_use;
    block->live_out.clear();
  }
  while (!update.empty()) {
    pair<Block *, Reg> cur = update.front();
    update.pop_front();
    for (Block *prev : cur.first->in_edge)
      if (prev->live_out.find(cur.second) == prev->live_out.end()) {
        prev->live_out.insert(cur.second);
        if (prev->def.find(cur.second) == prev->def.end() &&
            prev->live_in.find(cur.second) == prev->live_in.end()) {
          prev->live_in.insert(cur.second);
          update.emplace_back(prev, cur.second);
        }
      }
  }
}

vector<int> Func::get_in_deg() {
  size_t n = blocks.size();
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i)
    pos[blocks[i].get()] = i;
  vector<int> ret;
  ret.resize(n, 0);
  ret[0] = 1;
  for (size_t i = 0; i < n; ++i) {
    auto &block = blocks[i];
    bool go_next = true;
    for (auto &inst : block->insts)
      if (Branch *b = inst->as<Branch>()) {
        ++ret[pos[b->target]];
        if (b->cond == InstCond::Always)
          go_next = false;
      } else if (inst->as<Return>()) {
        go_next = false;
      }
    if (go_next) {
      assert(i + 1 < n);
      ++ret[i + 1];
    }
  }
  return ret;
}

vector<int> Func::get_branch_in_deg() {
  size_t n = blocks.size();
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i)
    pos[blocks[i].get()] = i;
  vector<int> ret;
  ret.resize(n, 0);
  for (size_t i = 0; i < n; ++i) {
    auto &block = blocks[i];
    for (auto &inst : block->insts)
      if (Branch *b = inst->as<Branch>()) {
        ++ret[pos[b->target]];
      }
  }
  return ret;
}

vector<int> Func::reg_allocate(RegAllocStat *stat) {
  info << "register allocation for function: " << name << '\n';
  info << "reg_n = " << reg_n << '\n';
  stat->spill_cnt = 0;
  info << "using SimpleColoringAllocator\n";
  while (true) {
    SimpleColoringAllocator allocator(this);
    vector<int> ret = allocator.run(stat);
    if (stat->succeed)
      return ret;
  }
}

bool Func::check_store_stack() {
  bool ret = true;
  for (auto &block : blocks) {
    int32_t sp_offset = 0;
    for (auto i = block->insts.begin(); i != block->insts.end(); ++i) {
      (*i)->maintain_sp(sp_offset);
      InstCond cond = (*i)->cond;
      if (auto store_stk = (*i)->as<StoreStack>()) {
        int32_t total_offset =
            store_stk->target->position + store_stk->offset - sp_offset;
        if (!load_store_offset_range(total_offset)) {
          Reg imm{reg_n++};
          block->insts.insert(
              i, set_cond(make_unique<LoadStackOffset>(imm, store_stk->offset,
                                                       store_stk->target),
                          cond));
          *i = set_cond(make_unique<ComplexStore>(store_stk->src, Reg{sp}, imm),
                        cond);
          ret = false;
        }
      }
    }
  }
  return ret;
}

void Func::replace_with_reg_alloc(const vector<int> &reg_alloc) {
  for (auto &block : blocks)
    for (auto &inst : block->insts)
      for (Reg *i : inst->regs())
        if (i->is_pseudo())
          i->id = reg_alloc[i->id];
}

void Func::replace_complex_inst() {
  for (auto &block : blocks) {
    int32_t sp_offset = 0;
    for (auto i = block->insts.begin(); i != block->insts.end(); ++i) {
      (*i)->maintain_sp(sp_offset);
      InstCond cond = (*i)->cond;
      if (auto load_stk = (*i)->as<LoadStack>()) {
        int32_t total_offset =
            load_stk->src->position + load_stk->offset - sp_offset;
        if (!load_store_offset_range(total_offset)) {
          Reg dst = load_stk->dst;
          Reg tmp = dst;
          tmp.is_float = 0;
          insert(block->insts, i, set_cond(load_imm(tmp, total_offset), cond));
          *i = set_cond(make_unique<ComplexLoad>(dst, Reg{sp}, tmp), cond);
        }
      } else if (auto load_stk_addr = (*i)->as<LoadStackAddr>()) {
        int32_t total_offset =
            load_stk_addr->src->position + load_stk_addr->offset - sp_offset;
        Reg dst = load_stk_addr->dst;
        replace(block->insts, i,
                set_cond(reg_imm_sum(dst, Reg{sp}, total_offset), cond));
      } else if (auto load_stk_offset = (*i)->as<LoadStackOffset>()) {
        int32_t total_offset = load_stk_offset->src->position +
                               load_stk_offset->offset - sp_offset;
        Reg dst = load_stk_offset->dst;
        replace(block->insts, i, set_cond(load_imm(dst, total_offset), cond));
      }
    }
  }
}

void Func::gen_asm(ostream &out) {
  RegAllocStat stat;
  vector<int> reg_alloc;
  AsmContext ctx;
  std::function<void(ostream & out)> prologue;
  while (true) {
    reg_alloc = reg_allocate(&stat);
    int32_t stack_size = 0;
    for (auto i = stack_objects.rbegin(); i != stack_objects.rend(); ++i) {
      (*i)->position = stack_size;
      stack_size += (*i)->size;
    }
    vector<Reg> save_regs;
    bool used[RegCount] = {};
    for (int i : reg_alloc)
      if (i >= 0)
        used[i] = true;
    for (int i = 0; i < RegCount; ++i)
      if (REGISTER_USAGE[i] == callee_save && used[i])
        save_regs.emplace_back(i);
    size_t save_reg_cnt = save_regs.size();
    if (save_reg_cnt)
      save_reg_cnt += 16;
    if ((stack_size + save_reg_cnt * 4) % 8)
      stack_size += 4;
    prologue = [save_regs, stack_size](ostream &out) {
      if (save_regs.size()) {
        out << "push {";
        for (size_t i = 0; i < save_regs.size(); ++i) {
          if (i > 0)
            out << ',';
          out << save_regs[i];
        }
        out << "}\n";
        out << "vpush {d0,d1,d2,d3,d4,d5,d6,d7}\n";
      }
      if (stack_size != 0)
        sp_move_asm(-stack_size, out);
    };
    ctx.epilogue = [save_regs, stack_size](ostream &out) -> bool {
      if (stack_size != 0)
        sp_move_asm(stack_size, out);
      bool pop_lr = false;
      if (save_regs.size()) {
        out << "vpop {d0,d1,d2,d3,d4,d5,d6,d7}\n";
        out << "pop {";
        for (size_t i = 0; i < save_regs.size(); ++i) {
          if (i > 0)
            out << ',';
          if (save_regs[i].id == lr) {
            pop_lr = true;
            out << "pc";
          } else
            out << save_regs[i];
        }
        out << "}\n";
      }
      return pop_lr;
    };
    int cur_pos = stack_size + save_reg_cnt * INT_SIZE;
    for (auto &i : caller_stack_object) {
      i->position = cur_pos;
      cur_pos += i->size;
    }
    if (check_store_stack())
      break;
  }
  info << "Register allocation:\n"
       << "spill: " << stat.spill_cnt << '\n'
       << "move instructions eliminated: " << stat.move_eliminated << '\n'
       << "callee-save registers used: " << stat.callee_save_used << '\n';
  replace_with_reg_alloc(reg_alloc);
  replace_complex_inst();
  out << '\n' << name << ":\n";
  prologue(out);
  for (auto &block : blocks)
    block->gen_asm(out, &ctx);
}

void Func::print(ostream &out) {
  out << '\n' << name << ":\n[prologue]\n";
  for (auto &block : blocks)
    block->print(out);
}

Program::Program(IR::CompileUnit *ir) : block_n(0) {
  for (size_t i = 0; i < ir->scope.objects.size(); ++i) {
    IR::MemObject *cur = ir->scope.objects[i].get();
    if (cur->size == 0)
      continue;
    unique_ptr<GlobalObject> res = make_unique<GlobalObject>();
    res->name = mangle_global_var_name(cur->name);
    res->size = cur->size;
    res->init = cur->initial_value;
    res->scalar_type = cur->scalar_type;
    res->is_const = cur->is_const;
    global_objects.push_back(std::move(res));
  }
  for (auto &i : ir->funcs) {
    unique_ptr<Func> res = make_unique<Func>(this, i.first, i.second.get());
    funcs.push_back(std::move(res));
  }
}

void Program::gen_global_var_asm(ostream &out) {
  bool exist_bss = false, exist_data = false;
  for (auto &obj : global_objects) {
    if (obj->init)
      exist_data = true;
    else
      exist_bss = true;
  }
  if (exist_data) {
    out << ".section .data\n";
    for (auto &obj : global_objects)
      if (obj->init) {
        if (obj->scalar_type == ScalarType::Int) {
          int32_t *init = reinterpret_cast<int32_t *>(obj->init);
          out << ".align\n";
          out << obj->name << ":\n";
          out << "    .4byte ";
          for (int i = 0; i < obj->size / 4; ++i) {
            if (i > 0)
              out << ',';
            out << init[i];
          }
          out << '\n';
        } else if (obj->scalar_type == ScalarType::Float) {
          auto *init = reinterpret_cast<uint32_t *>(obj->init);
          out << ".align\n";
          out << obj->name << ":\n";
          out << "    .4byte ";
          char repr[16];
          for (int i = 0; i < obj->size / 4; ++i) {
            if (i > 0)
              out << ',';
            sprintf(repr, "0x%x", init[i]);
            out << repr;
          }
          out << '\n';
        } else {
          char *init = reinterpret_cast<char *>(obj->init);
          out << obj->name << ":\n";
          out << "    .asciz " << init << '\n';
        }
      }
  }
  if (exist_bss) {
    out << ".section .bss\n";
    for (auto &obj : global_objects)
      if (!obj->init) {
        assert(obj->scalar_type == ScalarType::Int ||
               obj->scalar_type == ScalarType::Float);
        out << ".align\n";
        out << obj->name << ":\n";
        out << "    .space " << obj->size << '\n';
      }
  }
}

void Program::gen_asm(ostream &out) {
  out << ".arch armv7ve\n.arm\n";
  gen_global_var_asm(out);
  out << ".global main\n";
  out << ".section .text\n";
  for (auto &func : funcs)
    func->gen_asm(out);
}

} // namespace ARMv7
