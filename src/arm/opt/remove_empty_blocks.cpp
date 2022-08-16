#include <set>

#include "arm/opt/afterplay.hpp"

namespace ARMv7 {
void remove_empty_blocks(Func *f) {
  std::set<Block *> dests;
  for (auto &b : f->blocks) {
    for (auto &i : b->insts) {
      if (auto branch = dynamic_cast<Branch *>(i.get())) {
        dests.insert(branch->target);
      }
    }
  }
  f->blocks.erase(std::remove_if(f->blocks.begin(), f->blocks.end(),
                                 [&](auto &b) {
                                   return b->insts.size() == 0 &&
                                          dests.find(b.get()) == dests.end();
                                 }),
                  f->blocks.end());
}

void remove_empty_blocks(Program *prog) {
  for (auto &f : prog->funcs) {
    remove_empty_blocks(f.get());
  }
}
} // namespace ARMv7
