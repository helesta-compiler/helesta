#include "ir/opt/dom.hpp"
#include "ir/opt/loop.hpp"
#include "ir/opt/opt.hpp"

struct GCMNode;

struct GCMInstr {
  IR::Instr *i;
  GCMNode *node;
  bool visited;

  inline bool pinned() const {
    if (dynamic_cast<IR::PhiInstr *>(i))
      return true;
    if (dynamic_cast<IR::ControlInstr *>(i))
      return true;
    return false;
  }
};

struct GCMNode {
  std::vector<GCMNode *> ins;
  std::vector<std::unique_ptr<GCMInstr>> instrs;
  GCMNode *fa;
  IR::BB *bb;
  int dom_depth;
  int loop_depth;
};

struct GCMContext {
  std::vector<std::unique_ptr<GCMNode>> nodes;
  std::vector<GCMInstr *> defs;
  std::vector<std::vector<GCMInstr *>> uses;
  GCMNode *entry;
};

GCMNode *LCA(GCMNode *node1, GCMNode *node2) {
  assert(node1 != nullptr);
  assert(node2 != nullptr);
  while (node1 != node2) {
    if (node1->dom_depth > node2->dom_depth) {
      node1 = node1->fa;
    } else if (node1->dom_depth < node2->dom_depth) {
      node2 = node2->fa;
    } else {
      node1 = node1->fa;
      node2 = node2->fa;
    }
    assert(node1 != nullptr);
    assert(node2 != nullptr);
  }
  return node1;
}

std::unique_ptr<GCMContext> construct_gcm_context(IR::NormalFunc *func) {
  auto dom_ctx = construct_dom_tree(func);
  auto loop_ctx = construct_loop_tree(func);
  auto ctx = std::make_unique<GCMContext>();
  std::unordered_map<IR::BB *, GCMNode *> bb2gcm;
  ctx->nodes.reserve(func->bbs.size());
  ctx->defs.resize(func->max_reg_id + 1, nullptr);
  ctx->uses.resize(func->max_reg_id + 1);
  for (size_t i = 0; i < func->bbs.size(); i++) {
    auto node = std::make_unique<GCMNode>();
    node->dom_depth = dom_ctx->nodes[i]->depth;
    node->loop_depth = loop_ctx->nodes[i]->dep;
    node->bb = func->bbs[i].get();
    bb2gcm[func->bbs[i].get()] = node.get();
    if (func->bbs[i].get() == func->entry) {
      ctx->entry = node.get();
    }
    for (auto &i : func->bbs[i]->instrs) {
      auto raw_i = i.release();
      node->instrs.push_back(
          std::unique_ptr<GCMInstr>(new GCMInstr{raw_i, node.get(), false}));
      if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(raw_i)) {
        ctx->defs[reg_write_instr->d1.id] = node->instrs.back().get();
      }
      i->map_use([&](auto r) {
        ctx->uses[r.id].push_back(node->instrs.back().get());
      });
    }
    ctx->nodes.push_back(std::move(node));
    func->bbs[i]->instrs.clear();
  }
  for (size_t i = 0; i < ctx->nodes.size(); i++) {
    if (dom_ctx->nodes[i]->dom_fa != nullptr) {
      ctx->nodes[i]->fa = bb2gcm[dom_ctx->nodes[i]->dom_fa->bb];
    }
    auto outs = func->bbs[i]->getOutNodes();
    for (auto out : outs) {
      bb2gcm[out]->ins.push_back(ctx->nodes[i].get());
    }
  }
  return ctx;
}

void move_instr(GCMInstr *i, GCMNode *dst, bool in_front) {
  auto it = std::find_if(i->node->instrs.begin(), i->node->instrs.end(),
                         [&](auto &instr) { return instr.get() == i; });
  assert(it != i->node->instrs.end());
  if (in_front) {
    dst->instrs.insert(dst->instrs.begin(), std::move(*it));
  } else {
    dst->instrs.push_back(std::move(*it));
  }
  i->node->instrs.erase(it);
  i->node = dst;
}

GCMNode *schedule_early(GCMInstr *i, GCMContext *ctx) {
  if (i->visited) {
    return i->node;
  }
  i->visited = true;
  auto target = ctx->entry;
  i->i->map_use([&](auto r) {
    auto def = ctx->defs[r.id];
    assert(def != nullptr);
    auto res = schedule_early(def, ctx);
    if (res->dom_depth > target->dom_depth) {
      target = res;
    }
  });
  move_instr(i, target, false);
  return i->node;
}

void schedule_early(GCMContext *ctx) {
  for (auto &node : ctx->nodes) {
    for (auto &i : node->instrs) {
      if (i->visited)
        continue;
      i->visited = true;
      if (!i->pinned())
        continue;
      i->i->map_use([&](auto r) {
        auto def = ctx->defs[r.id];
        assert(def != nullptr);
        schedule_early(def, ctx);
      });
    }
  }
}

GCMNode *schedule_late(GCMInstr *i, GCMContext *ctx) {
  if (i->visited) {
    return i->node;
  }
  i->visited = true;
  GCMNode *lca = nullptr;
  if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i->i)) {
    for (auto use : ctx->uses[reg_write_instr->d1.id]) {
      auto res = schedule_late(use, ctx);
      if (auto phi_instr = dynamic_cast<IR::PhiInstr *>(use->i)) {
        IR::BB *pre_bb = nullptr;
        for (auto kv : phi_instr->uses) {
          if (kv.first == reg_write_instr->d1) {
            pre_bb = kv.second;
          }
        }
        assert(pre_bb != nullptr);
        for (auto in : use->node->ins) {
          if (in->bb == pre_bb) {
            res = in;
          }
        }
      }
      if (lca == nullptr) {
        res = lca;
      } else {
        res = LCA(lca, res);
      }
    }
  }
  assert(lca != nullptr);
  auto best = lca;
  auto cur = lca;
  while (cur != i->node) {
    if (cur->loop_depth < best->loop_depth) {
      best = cur;
    }
    cur = cur->fa;
  }
  move_instr(i, best, true);
  return i->node;
}

void schedule_late(GCMContext *ctx) {
  for (auto &node : ctx->nodes) {
    for (auto &i : node->instrs) {
      i->visited = false;
    }
  }
  for (auto &node : ctx->nodes) {
    for (auto &i : node->instrs) {
      if (i->visited)
        continue;
      i->visited = true;
      if (!i->pinned())
        continue;
      if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i->i)) {
        for (auto use : ctx->uses[reg_write_instr->d1.id]) {
          schedule_late(use, ctx);
        }
      }
    }
  }
}

void reconstruct(GCMContext *ctx, IR::NormalFunc *func) {
  assert(ctx->nodes.size() == func->bbs.size());
  for (size_t i = 0; i < ctx->nodes.size(); i++) {
    for (auto &j : ctx->nodes[i]->instrs) {
      func->bbs[i]->instrs.push_back(std::unique_ptr<IR::Instr>(j->i));
    }
  }
}

void global_code_motion_func(IR::NormalFunc *func) {
  auto ctx = construct_gcm_context(func);
  schedule_early(ctx.get());
  schedule_late(ctx.get());
  reconstruct(ctx.get(), func);
}

void global_code_motion(IR::CompileUnit *ir) {
  ir->for_each(global_code_motion_func);
}
