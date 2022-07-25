#include <unordered_map>

#include "common/common.hpp"
#include "ir/opt/dom.hpp"

DomTreeBuilderContext::DomTreeBuilderContext(IR::NormalFunc *func) {
  nodes = transfer_graph<IR::BB, DomTreeBuilderNode>(func->bbs);
  for (size_t idx = 0; idx < func->bbs.size(); idx++) {
    if (func->bbs[idx].get() == func->entry)
      entry = nodes[idx].get();
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

void DomTreeBuilderContext::construct_dom_frontiers() {
  // fill in nodes for each node
  for (auto &node : nodes) {
    auto outs = node->getOutNodes();
    for (auto out : outs) {
      out->in_nodes.push_back(node.get());
    }
  }
  for (auto &node_i : nodes) {
    for (auto &node_j : nodes) {
      if (node_i->node->sdom(node_j->node)) {
        continue;
      }
      bool is_df = false;
      for (auto node_k : node_j->in_nodes) {
        if (node_i->node->sdom(node_k->node)) {
          is_df = true;
        }
      }
      if (is_df) {
        node_i->node->dom_frontiers.push_back(node_j->node);
      }
    }
  }
}

std::unique_ptr<DomTreeContext> DomTreeBuilderContext::construct_dom_tree() {
  // 1. find direct father for each node in dom tree
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
  // 2. construct outs from father
  auto ctx = std::make_unique<DomTreeContext>();
  ctx->nodes.clear();
  ctx->nodes.reserve(nodes.size());
  std::unordered_map<DomTreeBuilderNode *, DomTreeNode *> builder2node;
  for (auto &node : nodes) {
    ctx->nodes.push_back(std::make_unique<DomTreeNode>());
    node->node = ctx->nodes.back().get();
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
  // 3. get dfn position and sub-tree size for each node
  dom_dfn.clear();
  dom_dfn.reserve(nodes.size());
  dfs(ctx->entry);
  // 4. construct dominance frontiers
  construct_dom_frontiers();
  return ctx;
}

std::unique_ptr<DomTreeContext> dominator_tree(IR::NormalFunc *func) {
  auto builder_ctx = std::make_unique<DomTreeBuilderContext>(func);
  auto ctx = builder_ctx->construct_dom_tree();
  return ctx;
}
