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

void Program::allocate_register() {
  for (auto &f : funcs) {
    f->allocate_register();
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
  out << R"(
SYS_clone = 120
CLONE_VM = 256
SIGCHLD = 17
__create_threads:
    push {r4, r5, r6, r7}
    mov r0, #(CLONE_VM | SIGCHLD)
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r6, #0
    mov r7, #SYS_clone
    swi #0
    pop {r4, r5, r6, r7}
    bx lr

SYS_waitid = 280
SYS_exit = 1
P_ALL = 0
WEXITED = 4
__join_threads:
    sub sp, sp, #16
    cmp r0, #0
	bne .L01
	vmov s31, r7
    mov r0, #P_ALL
    mov r1, #0
    mov r2, #0
    mov r3, #WEXITED
    mov r7, #SYS_waitid
    swi #0
    vmov r7, s31
    add sp, sp, #16
    bx lr
.L01:
    mov r0, #0
    mov r7, #SYS_exit
    swi #0

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
