#include <memory>

#include "arm/opt/afterplay.hpp"

namespace ARMv7 {

void replace_complex_inst(Func *func) {
  for (auto &block : func->blocks) {
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
              std::make_unique<ComplexLoad>(dst, Reg(sp, ScalarType::Int), tmp),
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

void replace_complex_inst(Program *prog) {
  for (auto &f : prog->funcs) {
    replace_complex_inst(f.get());
  }
}
} // namespace ARMv7
