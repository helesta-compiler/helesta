#include <unordered_map>

#include "arm/opt/afterplay.hpp"

namespace ARMv7 {

void remove_trivial_inst(Func *func) {
  for (auto &block : func->blocks) {
    std::unordered_map<int32_t, int32_t> const_info;
    block->for_each([&](Inst *inst) {
      if (auto mov = dynamic_cast<MoveReg *>(inst)) {
        if (mov->dst == mov->src) {
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

void remove_trivial_inst(Program *prog) {
  for (auto &f : prog->funcs) {
    remove_trivial_inst(f.get());
  }
}
} // namespace ARMv7
