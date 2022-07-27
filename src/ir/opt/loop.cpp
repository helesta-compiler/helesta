#include <memory>

#include "common/common.hpp"
#include "ir/opt/loop.hpp"

LoopTreeBuilderContext::LoopTreeBuilderContext(IR::NormalFunc *func) {
  nodes = transfer_graph<IR::BB, LoopTreeBuilderNode>(func->bbs);
  for (auto &node : nodes) {
    node->visited = false;
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
    dfs(out);
  }
}

std::unique_ptr<LoopTreeContext> LoopTreeBuilderContext::construct_loop_tree() {
  dfn.clear();
  dfn.reserve(nodes.size());
  dfs(entry);
  for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
    auto node = it->get();
    auto outs = node->getOutNodes();
    for (auto out : outs) {
      if (out->dfn <= node->dfn) {
      }
    }
  }
  return nullptr;
}

std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func) {
  auto builder_ctx = std::make_unique<LoopTreeBuilderContext>(func);
  builder_ctx->construct_loop_tree();
  return nullptr;
}
