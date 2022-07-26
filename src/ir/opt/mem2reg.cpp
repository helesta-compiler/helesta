#include <unordered_map>

#include "ir/opt/opt.hpp"

std::unordered_set<IR::Reg> mem2reg_func(IR::NormalFunc *func) {
  std::unordered_map<IR::Reg, IR::MemObject *> addr2mem;
  std::unordered_map<IR::MemObject *, IR::Reg> mem2value;
  std::unordered_set<IR::Reg> value_regs;
  func->for_each([&](IR::BB *bb) {
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); it++) {
      if (auto load_addr_instr = dynamic_cast<IR::LoadAddr *>(it->get())) {
        auto mem_obj = load_addr_instr->offset;
        if (!mem_obj->global && mem_obj->is_single_var()) {
          if (mem2value.find(mem_obj) == mem2value.end()) {
            auto value_reg = func->new_Reg(mem_obj->name);
            value_regs.insert(value_reg);
            mem2value.insert({mem_obj, value_reg});
            if (mem_obj->scalar_type == ScalarType::Int) {
              func->entry->push_front(
                  new IR::UnaryOpInstr(value_reg, value_reg, IR::UnaryOp::I2F));
              func->entry->push_front(new IR::LoadConst(value_reg, 0));
            } else {
              func->entry->push_front(new IR::LoadConst(value_reg, 0));
            }
          }
          addr2mem.insert({load_addr_instr->d1, mem_obj});
        }
      } else if (auto load_instr = dynamic_cast<IR::LoadInstr *>(it->get())) {
        if (addr2mem.find(load_instr->addr) != addr2mem.end()) {
          auto mem_obj = addr2mem[load_instr->addr];
          assert(mem2value.find(mem_obj) != mem2value.end());
          auto value_reg = mem2value[mem_obj];
          *it = std::make_unique<IR::UnaryOpInstr>(load_instr->d1, value_reg,
                                                   IR::UnaryOp::ID);
        }
      } else if (auto store_instr = dynamic_cast<IR::StoreInstr *>(it->get())) {
        if (addr2mem.find(store_instr->addr) != addr2mem.end()) {
          auto mem_obj = addr2mem[store_instr->addr];
          assert(mem2value.find(mem_obj) != mem2value.end());
          auto value_reg = mem2value[mem_obj];
          *it = std::make_unique<IR::UnaryOpInstr>(value_reg, store_instr->s1,
                                                   IR::UnaryOp::ID);
        }
      }
    }
  });
  return value_regs;
}

void mem2reg(IR::CompileUnit *ir) {
  ir->for_each([&](IR::NormalFunc *func) {
    auto value_regs = mem2reg_func(func);
    ssa_construction(func, value_regs);
  });
}
