#include <memory>

#include "common/common.hpp"
#include "ir/opt/loop.hpp"

LoopTreeBuilderContext::LoopTreeBuilderContext(IR::NormalFunc *func) {
  nodes = transfer_graph<IR::BB, LoopTreeBuilderNode>(func->bbs);
  for (auto &node : nodes) {
    node->visited = false;
    node->loop_node = nullptr;
    if (node->bb == func->entry) {
      entry = node.get();
    }
  }
}

void LoopTreeBuilderContext::dfs(LoopTreeBuilderNode *node) {
  if (node->visited) {
    return;
  }
  node->visited = true;
  dfn.push_back(node);
  node->dfn = dfn.size() - 1;
  auto outs = node->getOutNodes();
  for (auto out : outs) {
    out->fa = node;
    dfs(out);
  }
}

void LoopTreeBuilderContext::dfs(LoopTreeNode *node) {
  auto outs = node->getOutNodes();
  for (auto out : outs) {
    out->dep = node->dep + 1;
    dfs(out);
  }
}

std::unique_ptr<LoopTreeContext>
LoopTreeBuilderContext::construct_loop_tree(IR::NormalFunc *func) {
  dfn.clear();
  dfn.reserve(nodes.size());
  dfs(entry);
  auto ctx = std::make_unique<LoopTreeContext>();
  ctx->proxies = transfer_graph<IR::BB, LoopTreeNodeProxy>(func->bbs);
  for (auto &node : ctx->proxies) {
    if (node->bb == func->entry) {
      ctx->proxy_entry = node.get();
    }
  }
  for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
    auto node = it->get();
    auto outs = node->getOutNodes();
    for (auto out : outs) {
      if (out->dfn <= node->dfn) {
        ctx->nodes.push_back(std::make_unique<LoopTreeNode>(out->loop_node));
        out->loop_node = ctx->nodes.back().get();
        if (out == node)
          continue;
        auto cur = node->fa;
        while (cur->fa != out->fa) {
          if (cur->loop_node != nullptr) {
            assert(cur->loop_node->fa == nullptr);
            cur->loop_node->fa = out->loop_node;
          }
          cur = cur->fa;
          assert(cur != nullptr);
        }
      }
    }
  }
  auto root = std::make_unique<LoopTreeNode>(nullptr);
  root->dep = 0;
  for (auto &node : ctx->nodes) {
    if (node->fa == nullptr) {
      node->fa = root.get();
    }
  }
  dfs(root.get());
  ctx->nodes.push_back(std::move(root));
  construct_outs_for_tree(ctx->nodes);
  assert(nodes.size() == ctx->proxies.size());
  for (size_t i = 0; i < nodes.size(); i++) {
    ctx->proxies[i]->loop_node = nodes[i]->loop_node;
  }
  return ctx;
}

std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func) {
  auto builder_ctx = std::make_unique<LoopTreeBuilderContext>(func);
  auto ctx = builder_ctx->construct_loop_tree(func);
  return ctx;
}
