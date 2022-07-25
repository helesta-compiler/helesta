#pragma once

#include <vector>

#include "ir/ir.hpp"

struct DomTreeContext;
struct DomTreeNode;

struct DomTreeBuilderNode : Traversable<DomTreeBuilderNode> {
  std::vector<DomTreeBuilderNode *> out_nodes;
  DomTreeBuilderNode *dom_fa;
  DomTreeNode *node;
  int tag;
  IR::BB *bb;

  DomTreeBuilderNode() = delete;
  DomTreeBuilderNode(IR::BB *bb_) : bb(bb_) {}
  const std::vector<DomTreeBuilderNode *> getOutNodes() const override {
    return out_nodes;
  }

  void addOutNode(DomTreeBuilderNode *node) override {
    out_nodes.push_back(node);
  }
};

struct DomTreeBuilderContext {
  std::vector<std::unique_ptr<DomTreeBuilderNode>> nodes;
  std::vector<DomTreeBuilderNode *> dfn;
  std::vector<DomTreeNode *> dom_dfn;
  DomTreeBuilderNode *entry;
  int tag;

  DomTreeBuilderContext(IR::NormalFunc *func);

  void dfs(DomTreeBuilderNode *node);
  std::unique_ptr<DomTreeContext> construct_dom_tree();
  int dfs(DomTreeNode *node);

private:
  void construct_dom_frontiers();
};

struct DomTreeNode : Traversable<DomTreeNode> {
  std::vector<DomTreeNode *> out_nodes;
  std::vector<DomTreeNode *> dom_frontiers;
  DomTreeNode *dom_fa;
  int dfn, size;
  IR::BB *bb;

  DomTreeNode() = delete;
  DomTreeNode(IR::BB *bb_) : bb(bb_) {}

  const std::vector<DomTreeNode *> getOutNodes() const override {
    return out_nodes;
  }

  inline bool sdom(const DomTreeNode *node) const {
    return dfn < node->dfn && node->dfn <= dfn + size - 1;
  }

  void addOutNode(DomTreeNode *node) override { out_nodes.push_back(node); }
};

struct DomTreeContext {
  std::vector<std::unique_ptr<DomTreeNode>> nodes;
  DomTreeNode *entry;
};
