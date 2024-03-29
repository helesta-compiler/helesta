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

struct DomTreeNode : Traversable<DomTreeNode>, TreeNode<DomTreeNode> {
  std::vector<DomTreeNode *> out_nodes;
  std::vector<DomTreeNode *> dom_frontiers;
  DomTreeNode *dom_fa;
  int dfn, size, depth;
  IR::BB *bb;

  DomTreeNode() = delete;
  DomTreeNode(IR::BB *bb_) : bb(bb_) {}

  const std::vector<DomTreeNode *> getOutNodes() const override {
    return out_nodes;
  }

  inline bool sdom(const DomTreeNode *node) const {
    return dfn < node->dfn && node->dfn <= dfn + size - 1;
  }

  inline bool dom(const DomTreeNode *node) const {
    return dfn <= node->dfn && node->dfn <= dfn + size - 1;
  }

  void addOutNode(DomTreeNode *node) override { out_nodes.push_back(node); }
  DomTreeNode *getFather() const override { return dom_fa; }
};

struct DomTreeContext {
  std::vector<std::unique_ptr<DomTreeNode>> nodes;
  std::vector<DomTreeNode *> dfn;
  DomTreeNode *entry;
};

std::unique_ptr<DomTreeContext> construct_dom_tree(IR::NormalFunc *);
