#include "arm/opt/foreplay.hpp"

namespace ARMv7 {

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

void dce(Func *func) {
  func->calc_live();
  for (auto &block : func->blocks) {
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

void dce(Program *prog) {
  for (auto &f : prog->funcs) {
    dce(f.get());
  }
}

} // namespace ARMv7
