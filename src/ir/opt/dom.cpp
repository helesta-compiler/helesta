#include <unordered_map>

#include "ir/opt/dom.hpp"

DomTreeBuilderContext::DomTreeBuilderContext(IR::NormalFunc *func) {
  std::unordered_map<IR::BB *, DomTreeBuilderNode *> bb2node;
  for (auto &bb : func->bbs) {
    nodes.push_back(std::make_unique<DomTreeBuilderNode>());
    bb2node.insert({bb.get(), nodes.back().get()});
    if (bb.get() == func->entry) {
      entry = nodes.back().get();
    }
  }
  for (auto &bb : func->bbs) {
    auto node = bb2node[bb.get()];
    auto outs = bb->getOutNodes();
    node->out_nodes.clear();
    for (auto out : outs) {
      node->out_nodes.push_back(bb2node[out]);
    }
  }
}

void DomTreeBuilderContext::dfs(DomTreeBuilderNode *node) {
  if (node->tag == tag)
    return;
  if (tag == 1)
    dfn.push_back(node);
  node->tag = tag;
  auto outs = node->getOutNodes();
  for (auto out : outs) {
    dfs(out);
  }
}

int DomTreeBuilderContext::dfs(DomTreeNode *node) {
  dom_dfn.push_back(node);
  node->dfn = dom_dfn.size() - 1;
  node->size = 1;
  auto outs = node->getOutNodes();
  for (auto out : outs) {
    node->size += dfs(out);
  }
  return node->size;
}

std::unique_ptr<DomTreeContext> DomTreeBuilderContext::construct_dom_tree() {
  for (auto &node : nodes) {
    node->tag = 0;
    node->dom_fa = nullptr;
  }
  tag = 1;
  dfn.clear();
  dfn.reserve(nodes.size());
  dfs(entry);
  assert(dfn.front() == entry);
  for (auto node : dfn) {
    if (node == entry)
      continue;
    tag += 1;
    node->tag = tag;
    dfs(entry);
    for (auto &dom : nodes) {
      if (dom->tag != tag) {
        dom->dom_fa = node;
      }
    }
  }
  auto ctx = std::make_unique<DomTreeContext>();
  ctx->nodes.clear();
  ctx->nodes.reserve(nodes.size());
  std::unordered_map<DomTreeBuilderNode *, DomTreeNode *> builder2node;
  for (auto &node : nodes) {
    ctx->nodes.push_back(std::make_unique<DomTreeNode>());
    builder2node.insert({node.get(), ctx->nodes.back().get()});
    if (node.get() == entry) {
      ctx->entry = ctx->nodes.back().get();
    }
  }
  for (auto &node : nodes) {
    if (node->dom_fa == nullptr) {
      continue;
    }
    auto fa = builder2node[node->dom_fa];
    fa->out_nodes.push_back(builder2node[node.get()]);
  }
  dom_dfn.clear();
  dom_dfn.reserve(nodes.size());
  dfs(ctx->entry);
  return ctx;
}

std::unique_ptr<DomTreeContext> dominator_tree(IR::NormalFunc *func) {
  auto builder_ctx = std::make_unique<DomTreeBuilderContext>(func);
  auto ctx = builder_ctx->construct_dom_tree();
  return ctx;
}
