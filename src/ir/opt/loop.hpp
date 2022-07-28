#pragma once

#include <vector>

#include "ir/ir.hpp"
#include "ir/opt/dom.hpp"

struct LoopTreeNode;

struct LoopTreeNode {
  LoopTreeNode *fa;
  int dep;

  LoopTreeNode(LoopTreeNode *fa_, int dep_) : fa(fa_), dep(dep_) {}
};

struct LoopTreeContext {
  std::vector<std::unique_ptr<LoopTreeNode>> nodes;
  LoopTreeNode *entry;
};

struct LoopTreeBuilderNode : Traversable<LoopTreeBuilderNode> {
  std::vector<LoopTreeBuilderNode *> outs;
  std::vector<LoopTreeBuilderNode *> ins;
  IR::BB *bb;
  DomTreeNode *dom;
  LoopTreeBuilderNode *fa;
  LoopTreeNode *node;
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

  LoopTreeBuilderContext(IR::NormalFunc *, DomTreeContext *);

  std::unique_ptr<LoopTreeContext> construct_loop_tree();

private:
  void dfs(LoopTreeBuilderNode *);
  void dfs(LoopTreeBuilderNode *, LoopTreeBuilderNode *);
};

std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func,
                                                     DomTreeContext *ctx);
