#include "arm/opt/foreplay.hpp"

namespace ARMv7 {

struct EBNode {
  int in_deg_cnt;
  std::list<std::unique_ptr<Inst>> insts;
  EBNode *next;
  Block *block;

  inline bool ok() const {
    if (insts.size() > 2) {
      return false;
    }
    if (in_deg_cnt > 0) {
      return false;
    }
    for (auto &i : insts) {
      if (i->use_cpsr()) {
        return false;
      }
      if (i->change_cpsr() && i.get() != insts.back().get()) {
        return false;
      }
      if (dynamic_cast<Return *>(i.get())) {
        return false;
      }
    }
    return true;
  }

  inline void set_conditional(const InstCond cond) {
    for (auto &i : insts)
      i->cond = cond;
  }
};

struct EBContext {
  std::vector<std::unique_ptr<EBNode>> nodes;
  EBNode *entry;

  EBContext(Func *f) {
    std::unordered_map<Block *, EBNode *> b2ebnode;
    for (auto &b : f->blocks) {
      auto node = std::make_unique<EBNode>();
      node->insts = std::move(b->insts);
      node->in_deg_cnt = 0;
      node->next = nullptr;
      node->block = b.get();
      b2ebnode[b.get()] = node.get();
      nodes.emplace_back(std::move(node));
      if (b.get() == f->entry)
        entry = nodes.back().get();
    }
    for (auto &node : nodes) {
      for (auto &i : node->insts) {
        if (auto branch = dynamic_cast<Branch *>(i.get())) {
          auto target = branch->target;
          assert(target != nullptr);
          auto it = b2ebnode.find(target);
          assert(it != b2ebnode.end());
          auto next = it->second;
          next->in_deg_cnt += 1;
        }
      }
      if (auto branch = dynamic_cast<Branch *>(node->insts.back().get())) {
        auto it = b2ebnode.find(branch->target);
        node->next = it->second;
      }
    }
  }

  bool eliminate_branch() {
    for (size_t i = 0; i < nodes.size(); i++) {
      auto next = nodes[i]->next;
      if (i + 1 >= nodes.size())
        continue;
      if (next == nodes[i + 1].get()) {
        nodes[i]->insts.pop_back();
        return true;
      } else if (next == nullptr && nodes[i + 1]->in_deg_cnt == 0) {
        std::move(nodes[i + 1]->insts.begin(), nodes[i + 1]->insts.end(),
                  std::back_inserter(nodes[i]->insts));
        nodes[i + 1]->insts.clear();
        return true;
      }
      if (i + 2 >= nodes.size())
        continue;
      if (next != nodes[i + 2].get()) {
        continue;
      }
      if (!nodes[i + 1]->ok()) {
        continue;
      }
      nodes[i + 1]->set_conditional(logical_not(nodes[i]->insts.back()->cond));
      nodes[i]->insts.pop_back();
      return true;
    }
    return false;
  }

  void reconstruct(Func *f) {
    for (size_t i = 0; i < nodes.size(); i++) {
      f->blocks[i]->insts = std::move(nodes[i]->insts);
    }
  }
};

bool eliminate_branch(Func *f) {
  auto ctx = std::make_unique<EBContext>(f);
  auto res = ctx->eliminate_branch();
  ctx->reconstruct(f);
  return res;
}

bool eliminate_branch(Program *prog) {
  auto worked = false;
  for (auto &f : prog->funcs) {
    if (eliminate_branch(f.get()))
      worked = true;
  }
  return worked;
}

} // namespace ARMv7
