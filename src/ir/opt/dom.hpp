#pragma once

#include <vector>

#include "ir/ir.hpp"

struct DomTreeContext;

struct DomTreeNode : Traversable<DomTreeNode> {
  std::vector<DomTreeNode *> out_nodes;
  DomTreeNode *dom_fa;
  DomTreeContext *ctx;

  DomTreeNode(DomTreeContext *ctx_) : ctx{ctx_} {}
  const std::vector<DomTreeNode *> getOutNodes() const override {
    return out_nodes;
  }
};

struct DomTreeContext {
  std::vector<std::unique_ptr<DomTreeNode>> nodes;

  DomTreeContext(IR::NormalFunc *func);
};
