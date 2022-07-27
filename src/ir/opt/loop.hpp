#pragma once

#include <vector>

#include "ir/ir.hpp"

struct LoopTreeNode {};

struct LoopTreeContext {};

struct LoopTreeBuilderNode : Traversable<LoopTreeBuilderNode> {
  std::vector<LoopTreeBuilderNode *> outs;
  IR::BB *bb;
  int dfn;
  bool visited;

  const std::vector<LoopTreeBuilderNode *> getOutNodes() const override {
    return outs;
  }

  void addOutNode(LoopTreeBuilderNode *node) override { outs.push_back(node); }

  LoopTreeBuilderNode(IR::BB *bb_) : bb(bb_) {}
};

struct LoopTreeBuilderContext {
  std::vector<std::unique_ptr<LoopTreeBuilderNode>> nodes;
  std::vector<LoopTreeBuilderNode *> dfn;
  LoopTreeBuilderNode *entry;

  LoopTreeBuilderContext(IR::NormalFunc *);

  std::unique_ptr<LoopTreeContext> construct_loop_tree();

private:
  void dfs(LoopTreeBuilderNode *);
};

std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func);
