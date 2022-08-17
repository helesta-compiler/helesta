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

void Func::replace_with_reg_alloc(const vector<int> &int_reg_alloc,
                                  const vector<int> &float_reg_alloc) {
  for (auto &block : blocks)
    for (auto &inst : block->insts)
      for (Reg *i : inst->regs())
        if (i->is_pseudo())
          i->id = i->type == ScalarType::Int ? int_reg_alloc[i->id]
                                             : float_reg_alloc[i->id];
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

SYS_sched_setaffinity = 241
__bind_core:
	vmov s28, r4
	vmov s29, r5
	vmov s30, r6
	vmov s31, r7
	sub sp, sp, #1024
	add r2, sp, r0, LSL #2
	str r1, [r2,#0]
	mov r0, #0
	mov r1, #4
	mov r7, #SYS_sched_setaffinity
	swi #0
	add sp, sp, #1024
    vmov r4, s28
    vmov r5, s29
    vmov r6, s30
    vmov r7, s31
    bx lr

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

__divpow2:
	mov     r2, r0
	mov     r0, #0
	cmp     r1, #31
	bxgt    lr
	cmp     r2, #0
	ble     .L04
	lsr     r0, r2, r1
	bx      lr
.L04:
	mov     r0, #1
	add     r0, r2, r0, lsl r1
	sub     r0, r0, #1
	asr     r0, r0, r1
	bx      lr
)";
  for (auto &func : funcs)
    func->gen_asm(out);
}

} // namespace ARMv7
