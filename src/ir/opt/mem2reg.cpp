#include <unordered_map>

#include "ir/opt/opt.hpp"

void mem2reg_func(IR::NormalFunc *func) {
  std::unordered_map<IR::Reg, IR::Reg> addr2value;
  func->for_each([&](IR::BB *bb) {
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); it++) {
      if (auto load_addr_instr = dynamic_cast<IR::LoadAddr *>(it->get())) {
        auto mem_obj = load_addr_instr->offset;
        if (!mem_obj->global && mem_obj->is_single_var()) {
          if (addr2value.find(load_addr_instr->d1) == addr2value.end()) {
            auto value_reg = func->new_Reg(mem_obj->name);
            addr2value.insert({load_addr_instr->d1, value_reg});
          }
        }
      } else if (auto load_instr = dynamic_cast<IR::LoadInstr *>(it->get())) {
        if (addr2value.find(load_instr->addr) != addr2value.end()) {
          auto value_reg = addr2value[load_instr->addr];
          *it = std::make_unique<IR::UnaryOpInstr>(load_instr->d1, value_reg,
                                                   IR::UnaryOp::ID);
        }
      } else if (auto store_instr = dynamic_cast<IR::StoreInstr *>(it->get())) {
        if (addr2value.find(store_instr->addr) != addr2value.end()) {
          auto value_reg = addr2value[store_instr->addr];
          *it = std::make_unique<IR::UnaryOpInstr>(value_reg, store_instr->s1,
                                                   IR::UnaryOp::ID);
        }
      }
    }
  });
}

void mem2reg(IR::CompileUnit *ir) { ir->for_each(mem2reg_func); }
