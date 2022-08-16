#include "arm/opt/afterplay.hpp"

namespace ARMv7 {
void remove_empty_blocks(Func *f) {
  f->blocks.erase(std::remove_if(f->blocks.begin(), f->blocks.end(),
                                 [](auto &b) { return b->insts.size() == 0; }),
                  f->blocks.end());
}

void remove_empty_blocks(Program *prog) {
  for (auto &f : prog->funcs) {
    remove_empty_blocks(f.get());
  }
}
} // namespace ARMv7
