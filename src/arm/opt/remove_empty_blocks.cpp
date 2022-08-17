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

void relink_empty_blocks(Func *f) {
  std::vector<int> fa;
  std::map<Block *, int> b2idx;
  int n = f->blocks.size();
  for (int i = 0; i < n; i++) {
    fa.push_back(i);
    b2idx[f->blocks[i].get()] = i;
  }
  std::function<int(int)> getf = [&](int i) {
    return fa[i] == i ? i : (fa[i] = getf(fa[i]));
  };
  for (size_t i = 0; i < f->blocks.size(); i++) {
    if (f->blocks[i]->insts.size() == 0 && i + 1 < f->blocks.size()) {
      fa[getf(i)] = getf(i + 1);
    }
  }
  for (auto &b : f->blocks)
    for (auto &i : b->insts)
      if (auto branch = dynamic_cast<Branch *>(i.get()))
        branch->target = f->blocks[getf(b2idx[branch->target])].get();
}

void remove_empty_blocks(Program *prog) {
  for (auto &f : prog->funcs) {
    relink_empty_blocks(f.get());
    remove_empty_blocks(f.get());
  }
}
} // namespace ARMv7
