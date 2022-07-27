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
  int dom_depth;
  int loop_depth;
};

struct GCMContext {
  std::vector<std::unique_ptr<GCMNode>> nodes;
  std::vector<GCMInstr *> defs;
  GCMNode *entry;
};

std::unique_ptr<GCMContext> construct_gcm_context(IR::NormalFunc *func) {
  auto dom_ctx = construct_dom_tree(func);
  auto loop_ctx = construct_loop_tree(func);
  auto ctx = std::make_unique<GCMContext>();
  std::unordered_map<IR::BB *, GCMNode *> bb2gcm;
  ctx->nodes.reserve(func->bbs.size());
  ctx->defs.resize(func->max_reg_id + 1, nullptr);
  for (size_t i = 0; i < func->bbs.size(); i++) {
    auto node = std::make_unique<GCMNode>();
    node->dom_depth = dom_ctx->nodes[i]->depth;
    node->loop_depth = loop_ctx->nodes[i]->dep;
    bb2gcm[func->bbs[i].get()] = node.get();
    if (func->bbs[i].get() == func->entry) {
      ctx->entry = node.get();
    }
    func->bbs[i]->for_each([&](auto i) {
      node->instrs.push_back(
          std::unique_ptr<GCMInstr>(new GCMInstr{i, node.get(), false}));
      if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i)) {
        ctx->defs[reg_write_instr->d1.id] = node->instrs.back().get();
      }
    });
    ctx->nodes.push_back(std::move(node));
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
  auto it = std::find_if(i->node->instrs.begin(), i->node->instrs.end(),
                         [&](auto &instr) { return instr.get() == i; });
  assert(it != i->node->instrs.end());
  target->instrs.push_back(std::move(*it));
  i->node->instrs.erase(it);
  i->node = target;
  return i->node;
}

void schedule_early(GCMContext *ctx) {
  for (auto &node : ctx->nodes) {
    for (auto &i : node->instrs) {
      if (i->visited)
        continue;
      if (!i->pinned())
        continue;
      i->visited = true;
      i->i->map_use([&](auto r) {
        auto def = ctx->defs[r.id];
        assert(def != nullptr);
        schedule_early(def, ctx);
      });
    }
  }
}

void global_code_motion_func(IR::NormalFunc *func) {
  auto ctx = construct_gcm_context(func);
  schedule_early(ctx.get());
}

void global_code_motion(IR::CompileUnit *ir) {
  ir->for_each(global_code_motion_func);
}
