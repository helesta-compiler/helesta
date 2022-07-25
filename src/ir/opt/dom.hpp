#pragma once

#include <vector>

#include "ir/ir.hpp"

struct DomTreeContext;
struct DomTreeNode;

struct DomTreeBuilderNode : Traversable<DomTreeBuilderNode> {
  std::vector<DomTreeBuilderNode *> out_nodes;
  DomTreeBuilderNode *dom_fa;
  int tag;

  DomTreeBuilderNode() = default;
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
};

struct DomTreeNode : Traversable<DomTreeNode> {
  std::vector<DomTreeNode *> out_nodes;
  int dfn, size;

  const std::vector<DomTreeNode *> getOutNodes() const override {
    return out_nodes;
  }

  void addOutNode(DomTreeNode *node) override { out_nodes.push_back(node); }
};

struct DomTreeContext {
  std::vector<std::unique_ptr<DomTreeNode>> nodes;
  DomTreeNode *entry;
};
