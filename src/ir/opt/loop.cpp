#include <memory>

#include "common/common.hpp"
#include "ir/opt/loop.hpp"

LoopTreeBuilderContext::LoopTreeBuilderContext(IR::NormalFunc *func,
                                               DomTreeContext *ctx) {
  nodes = transfer_graph<IR::BB, LoopTreeBuilderNode>(func->bbs);
  for (auto &node : nodes) {
    if (node->bb == func->entry) {
      entry = node.get();
    }
    node->visited = false;
  }
  std::unordered_map<DomTreeNode *, LoopTreeBuilderNode *> dom2builder;
  assert(ctx->nodes.size() == nodes.size());
  for (size_t i = 0; i < ctx->nodes.size(); i++) {
    auto builder = nodes[i].get();
    auto dom = ctx->nodes[i].get();
    dom2builder[dom] = builder;
    builder->dom = dom;
  }
  for (auto &node : nodes) {
    if (node->dom->dom_fa == nullptr) {
      node->fa = nullptr;
    } else {
      node->fa = dom2builder[node->dom->dom_fa];
    }
  }
  dfn.clear();
}

void LoopTreeBuilderContext::dfs(LoopTreeBuilderNode *node) {
  if (node->visited)
    return;
  node->visited = true;
  dfn.push_back(node);
  auto outs = node->getOutNodes();
  for (auto out : outs) {
    dfs(out);
  }
}

void LoopTreeBuilderContext::dfs(LoopTreeBuilderNode *node,
                                 LoopTreeBuilderNode *header) {
  if (node == header) {
    return;
  }
  if (!node->visited) {
    node->fa = header;
    dfs(node->fa, header);
  } else {
    node->visited = true;
    for (auto in : node->ins) {
      dfs(in, header);
    }
  }
}

std::unique_ptr<LoopTreeContext> LoopTreeBuilderContext::construct_loop_tree() {
  dfs(entry);
  for (auto &node : nodes) {
    node->visited = false;
    auto outs = node->getOutNodes();
    for (auto out : outs) {
      out->ins.push_back(node.get());
    }
  }
  std::cout << "finding loops" << std::endl;
  for (auto it = dfn.rbegin(); it != dfn.rend(); it++) {
    auto node = *it;
    for (auto in : node->ins) {
      if (node->dom->dom(in->dom)) {
        dfs(in, node);
      }
    }
  }
  std::cout << "calc depths" << std::endl;
  auto ctx = std::make_unique<LoopTreeContext>();
  for (size_t i = 0; i < nodes.size(); i++) {
    ctx->nodes.push_back(std::make_unique<LoopTreeNode>(nullptr, 0));
    nodes[i]->node = ctx->nodes.back().get();
  }
  for (auto node : dfn) {
    if (node->fa != nullptr) {
      node->node->fa = node->fa->node;
      node->node->dep = node->fa->node->dep + 1;
    }
  }
  return ctx;
}

std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func,
                                                     DomTreeContext *ctx) {
  auto builder_ctx = std::make_unique<LoopTreeBuilderContext>(func, ctx);
  auto loop_ctx = builder_ctx->construct_loop_tree();
  return loop_ctx;
}
