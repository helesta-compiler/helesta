#include <memory>

#include "arm/opt/foreplay.hpp"

namespace ARMv7 {

void replace_pseduo_inst(Func *func) {
  for (auto &block : func->blocks) {
    auto &insts = block->insts;
    for (auto it = insts.begin(); it != insts.end(); ++it) {
      Inst *inst = it->get();
      if (auto bop = dynamic_cast<RegRegInst *>(inst)) {
        if (bop->op == RegRegInst::Mod) {
          auto dst = bop->dst;
          auto s1 = bop->lhs;
          auto s2 = bop->rhs;
          insts.insert(
              it, std::make_unique<RegRegInst>(RegRegInst::Div, dst, s1, s2));
          *it = std::make_unique<ML>(ML::Mls, dst, s2, dst, s1);
        }
      }
    }
  }
}

void replace_pseduo_inst(Program *prog) {
  for (auto &f : prog->funcs) {
    replace_pseduo_inst(f.get());
  }
}
} // namespace ARMv7
