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

MappingInfo::MappingInfo()
    : reg_n(std::max(RegConvention<ScalarType::Int>::Count,
                     RegConvention<ScalarType::Float>::Count)) {}

Reg MappingInfo::new_reg() { return Reg(reg_n++, ScalarType::Int); }

Reg MappingInfo::from_ir_reg(IR::Reg ir_reg) {
  auto it = reg_mapping.find(ir_reg.id);
  if (it != reg_mapping.end()) {
    Reg ret = it->second;
    return ret;
  }
  Reg ret = new_reg();
  reg_mapping.insert({ir_reg.id, ret});
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

void handle_params(Func *ctx, MappingInfo &info, Block *entry,
                   IR::NormalFunc *ir_func) {
  int int_arg_cnt = 0, float_arg_cnt = 0;
  for (auto arg_type : ir_func->arg_types) {
    Reg cur_reg = info.new_reg();
    if (arg_type == ScalarType::Int) {
      if (int_arg_cnt <
          RegConvention<ScalarType::Int>::ARGUMENT_REGISTER_COUNT) {
        entry->push_back(std::make_unique<MoveReg>(
            cur_reg,
            Reg(RegConvention<ScalarType::Int>::ARGUMENT_REGISTERS[int_arg_cnt],
                ScalarType::Int)));
      } else {
        unique_ptr<StackObject> t = make_unique<StackObject>();
        t->size = INT_SIZE;
        t->position = -1;
        entry->push_back(make_unique<LoadStack>(cur_reg, 0, t.get()));
        ctx->caller_stack_object.push_back(std::move(t));
      }
      int_arg_cnt += 1;
    } else if (arg_type == ScalarType::Float) {
      info.set_float(cur_reg);
      if (float_arg_cnt <
          RegConvention<ScalarType::Float>::ARGUMENT_REGISTER_COUNT) {
        entry->push_back(std::make_unique<MoveReg>(
            cur_reg,
            Reg(RegConvention<
                    ScalarType::Float>::ARGUMENT_REGISTERS[float_arg_cnt],
                ScalarType::Float)));
      } else {
        unique_ptr<StackObject> t = make_unique<StackObject>();
        t->size = INT_SIZE;
        t->position = -1;
        entry->push_back(make_unique<LoadStack>(cur_reg, 0, t.get()));
        ctx->caller_stack_object.push_back(std::move(t));
      }
      float_arg_cnt += 1;
    } else
      assert(false);
    ctx->args.push_back(cur_reg);
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
  handle_params(this, info, entry, ir_func);
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
  reg_n = info.reg_n;
  float_regs = std::move(info.float_regs);
  for (auto &block : blocks) {
    for (auto &inst : block->insts) {
      for (Reg *r : inst->regs()) {
        if (r->is_pseudo())
          r->type = float_regs.count(*r) ? ScalarType::Float : ScalarType::Int;
      }
    }
  }

  PassEnabled("mi") merge_inst();
  replace_pseduo_inst();
  PassEnabled("dce") dce();
  if (global_config.args.count("ir2")) {
    ir_func->for_each([&](IR::BB *bb0) {
      std::cerr << "================================\n";
      std::cerr << *bb0;
      info.block_mapping[bb0]->print(std::cerr);
    });
  }
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
      if (auto cmp = dynamic_cast<RegRegCmp *>(inst)) {
        if (!constant_reg.count(cmp->rhs))
          continue;
        int32_t v = constant_reg[cmp->rhs];
        if (is_legal_immediate(v)) {
          Ins(new RegImmCmp(RegImmCmp::Cmp, cmp->lhs, v));
          Del();
        }
      } else if (auto bop = dynamic_cast<RegRegInst *>(inst)) {
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
            Reg lo = Reg(r4, ScalarType::Int);
            Reg hi = Reg(r5, ScalarType::Int);
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
            Reg lo = Reg(r4, ScalarType::Int);
            Reg hi = Reg(r5, ScalarType::Int);
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
}
void Func::replace_pseduo_inst() {
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
        if ((r.is_machine() && !r.is_allocable()) || live.count(r))
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
        if (r.is_pseudo() || r.is_allocable()) {
          block->live_use.erase(r);
          block->def.insert(r);
        }
      for (Reg r : (*it)->use_reg())
        if (r.is_pseudo() || r.is_allocable()) {
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
          Reg imm(reg_n++, ScalarType::Int);
          block->insts.insert(
              i, set_cond(make_unique<LoadStackOffset>(imm, store_stk->offset,
                                                       store_stk->target),
                          cond));
          *i = set_cond(make_unique<ComplexStore>(
                            store_stk->src, Reg(sp, ScalarType::Int), imm),
                        cond);
          ret = false;
        }
      }
    }
  }
  return ret;
}

void Func::replace_with_reg_alloc(const vector<int> &int_reg_alloc,
                                  const vector<int> &float_reg_alloc) {
  for (auto &block : blocks)
    for (auto &inst : block->insts)
      for (Reg *i : inst->regs())
        if (i->is_pseudo())
          i->id = i->type == ScalarType::Int ? int_reg_alloc[i->id]
                                             : float_reg_alloc[i->id];
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
          tmp.type = ScalarType::Int;
          insert(block->insts, i, set_cond(load_imm(tmp, total_offset), cond));
          *i = set_cond(
              make_unique<ComplexLoad>(dst, Reg(sp, ScalarType::Int), tmp),
              cond);
        }
      } else if (auto load_stk_addr = (*i)->as<LoadStackAddr>()) {
        int32_t total_offset =
            load_stk_addr->src->position + load_stk_addr->offset - sp_offset;
        Reg dst = load_stk_addr->dst;
        replace(
            block->insts, i,
            set_cond(reg_imm_sum(dst, Reg(sp, ScalarType::Int), total_offset),
                     cond));
      } else if (auto load_stk_offset = (*i)->as<LoadStackOffset>()) {
        int32_t total_offset = load_stk_offset->src->position +
                               load_stk_offset->offset - sp_offset;
        Reg dst = load_stk_offset->dst;
        replace(block->insts, i, set_cond(load_imm(dst, total_offset), cond));
      }
    }
  }
}

void Func::remove_trivial_inst() {
  for (auto &block : blocks) {
    std::unordered_map<int32_t, int32_t> const_info;
    block->for_each([&](Inst *inst) {
      if (auto mov = dynamic_cast<MoveReg *>(inst)) {
        if (mov->dst.type == mov->src.type && mov->dst == mov->src) {
          block->del();
          return;
        }
      }
      if (auto mov = dynamic_cast<MoveImm *>(inst)) {
        if (mov->op == MoveImm::Mov && const_info.count(mov->dst.id)) {
          if (const_info.at(mov->dst.id) == mov->src) {
            block->del();
            return;
          }
        }
        if (mov->op == MoveImm::Mov && mov->cond == InstCond::Always) {
          const_info[mov->dst.id] = mov->src;
        } else {
          const_info.erase(mov->dst.id);
        }
        return;
      }
      for (auto r : inst->def_reg())
        const_info.erase(r.id);
    });
  }
}

template <ScalarType type>
std::vector<int> reg_allocate(RegAllocStat *stat, Func *ctx) {
  info << "register allocation for function: " << ctx->name << '\n';
  info << "reg_n = " << ctx->reg_n << '\n';
  stat->spill_cnt = 0;
  info << "using SimpleColoringAllocator\n";
  while (true) {
    SimpleColoringAllocator<type> allocator(ctx);
    std::vector<int> ret = allocator.run(stat);
    if (stat->succeed)
      return ret;
  }
}

void Func::gen_asm(ostream &out) {
  RegAllocStat int_stat, float_stat;
  vector<int> int_reg_alloc, float_reg_alloc;
  AsmContext ctx;
  std::function<void(ostream & out)> prologue;
  while (true) {
    int_reg_alloc = reg_allocate<ScalarType::Int>(&int_stat, this);
    float_reg_alloc = reg_allocate<ScalarType::Float>(&float_stat, this);
    int32_t stack_size = 0;
    for (auto i = stack_objects.rbegin(); i != stack_objects.rend(); ++i) {
      (*i)->position = stack_size;
      stack_size += (*i)->size;
    }
    vector<Reg> save_int_regs, save_float_regs;
    bool used_int[RegConvention<ScalarType::Int>::Count] = {};
    bool used_float[RegConvention<ScalarType::Float>::Count] = {};
    for (int i : int_reg_alloc)
      if (i >= 0)
        used_int[i] = true;
    for (int i = 0; i < RegConvention<ScalarType::Int>::Count; ++i)
      if (RegConvention<ScalarType::Int>::REGISTER_USAGE[i] ==
              RegisterUsage::callee_save &&
          used_int[i])
        save_int_regs.emplace_back(Reg(i, ScalarType::Int));
    for (int i : float_reg_alloc)
      if (i >= 0)
        used_float[i] = true;
    for (int i = 0; i < RegConvention<ScalarType::Float>::Count; ++i)
      if (RegConvention<ScalarType::Float>::REGISTER_USAGE[i] ==
              RegisterUsage::callee_save &&
          used_float[i])
        save_float_regs.emplace_back(Reg(i, ScalarType::Float));
    size_t save_reg_cnt = save_int_regs.size() + save_float_regs.size();
    if ((stack_size + save_reg_cnt * 4) % 8)
      stack_size += 4;
    prologue = [save_int_regs, save_float_regs, stack_size](ostream &out) {
      if (save_int_regs.size()) {
        out << "push {";
        for (size_t i = 0; i < save_int_regs.size(); ++i) {
          if (i > 0)
            out << ',';
          out << save_int_regs[i];
        }
        out << "}\n";
      }
      for (auto reg : save_float_regs) {
        out << "vpush {" << reg << "}" << std::endl;
      }
      if (stack_size != 0)
        sp_move_asm(-stack_size, out);
    };
    ctx.epilogue = [save_int_regs, save_float_regs,
                    stack_size](ostream &out) -> bool {
      if (stack_size != 0)
        sp_move_asm(stack_size, out);
      for (auto it = save_float_regs.rbegin(); it != save_float_regs.rend();
           it++) {
        auto reg = *it;
        out << "vpop {" << reg << "}" << std::endl;
      }
      bool pop_lr = false;
      if (save_int_regs.size()) {
        out << "pop {";
        for (size_t i = 0; i < save_int_regs.size(); ++i) {
          if (i > 0)
            out << ',';
          if (save_int_regs[i].id == lr) {
            pop_lr = true;
            out << "pc";
          } else
            out << save_int_regs[i];
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
  replace_with_reg_alloc(int_reg_alloc, float_reg_alloc);
  replace_complex_inst();
  remove_trivial_inst();
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
  out <<
      R"(.arch armv7ve
.fpu neon
.arm
)";
  gen_global_var_asm(out);
  out <<
      R"(.global main
.section .text
SYS_clone = 120
CLONE_VM = 256
SIGCHLD = 17
__create_threads:
	vmov s28, r4
	vmov s29, r5
	vmov s30, r6
	vmov s31, r7
    mov r0, #(CLONE_VM | SIGCHLD)
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r6, #0
    mov r7, #SYS_clone
    swi #0
    vmov r4, s28
    vmov r5, s29
    vmov r6, s30
    vmov r7, s31
    bx lr

SYS_waitid = 280
SYS_exit = 1
P_ALL = 0
WEXITED = 4
__join_threads:
    dsb
	isb
	sub sp, sp, #16
    cmp r0, #0
	bne .L01
	vmov s28, r4
	vmov s29, r5
	vmov s30, r6
	vmov s31, r7
    mov r0, #-1
    mov r1, #0
    mov r2, #0
    mov r3, #WEXITED
    mov r7, #SYS_waitid
    swi #0
    vmov r4, s28
    vmov r5, s29
    vmov r6, s30
    vmov r7, s31
    add sp, sp, #16
    bx lr
.L01:
    mov r0, #0
    mov r7, #SYS_exit
    swi #0

__lock:
    ldrex r1, [r0]
	cmp r1, #1
	beq __lock
	mov r1, #1
	strex r2, r1, [r0]
	cmp r2, #0
	bne __lock
	dmb
	bx lr

__unlock:
    dmb
	mov r1, #0
	str r1, [r0]
	bx lr

__barrier:
	ldrex r3, [r0]
	cmp r3, #0
	moveq r3, r1
	sub r3, r3, #1
	strex r2, r3, [r0]
	cmp r2, #0
	bne __barrier
	dmb
.L03:
	ldr r1, [r0]
	cmp r1, #0
	bne .L03
	bx lr

__nop:
    mov r0, #64
.L02:
    sub r0, r0, #1
	cmp r0, #0
	bne .L02
	bx lr

__umulmod:
	push    {r11, lr}
	umull   r3, r12, r1, r0
	mov     r0, r3
	mov     r1, r12
	mov     r3, #0
	bl      __aeabi_uldivmod
	mov     r0, r2
	pop     {r11, lr}
	bx      lr

__u_c_np1_2_mod:
	push    {r11, lr}
	mov     r2, r1
	mov     r3, r0
	mov     r1, #0
	umlal   r3, r1, r0, r0
	lsrs    r1, r1, #1
	rrx     r0, r3
	mov     r3, #0
	bl      __aeabi_uldivmod
	mov     r0, r2
	pop     {r11, lr}
	bx      lr

__s_c_np1_2:
	asr     r1, r0, #31
	mov     r2, r0
	smlal   r2, r1, r0, r0
	adds    r0, r2, r1, lsr #31
	adc     r1, r1, #0
	lsrs    r1, r1, #1
	rrx     r0, r0
	bx      lr

__fixmod:
	push    {r4, lr}
	mov     r4, r1
	bl      __aeabi_idivmod
	mov     r0, r1
	cmp     r1, #0
	addmi   r0, r0, r4
	pop     {r4, lr}
	bx      lr

__umod:
	push    {r11, lr}
	bl      __aeabi_uidivmod
	mov     r0, r1
	pop     {r11, lr}
	bx      lr
)";
  for (auto &func : funcs)
    func->gen_asm(out);
}

} // namespace ARMv7
