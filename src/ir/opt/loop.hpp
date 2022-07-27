#pragma once

#include <vector>

#include "ir/ir.hpp"

struct LoopTreeNode;

struct LoopTreeNodeProxy : Traversable<LoopTreeNodeProxy> {
  std::vector<LoopTreeNodeProxy *> outs;
  IR::BB *bb;
  LoopTreeNode *loop_node;

  LoopTreeNodeProxy(IR::BB *bb_) : bb(bb_) {}

  const std::vector<LoopTreeNodeProxy *> getOutNodes() const override {
    return outs;
  }

  void addOutNode(LoopTreeNodeProxy *node) override { outs.push_back(node); }
};

struct LoopTreeNode : Traversable<LoopTreeNode>, TreeNode<LoopTreeNode> {
  LoopTreeNode *fa;
  std::vector<LoopTreeNode *> outs;
  int dep;

  LoopTreeNode(LoopTreeNode *fa_) : fa(fa_) {}

  LoopTreeNode *getFather() const override { return fa; }

  const std::vector<LoopTreeNode *> getOutNodes() const override {
    return outs;
  }

  void addOutNode(LoopTreeNode *node) override { outs.push_back(node); }
};

struct LoopTreeContext {
  std::vector<std::unique_ptr<LoopTreeNode>> nodes;
  std::vector<std::unique_ptr<LoopTreeNodeProxy>> proxies;
};

struct LoopTreeBuilderNode : Traversable<LoopTreeBuilderNode> {
  std::vector<LoopTreeBuilderNode *> outs;
  IR::BB *bb;
  int dfn;
  bool visited;
  LoopTreeNode *loop_node;
  LoopTreeBuilderNode *fa;

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

  std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func);

private:
  void dfs(LoopTreeBuilderNode *);
};

std::unique_ptr<LoopTreeContext> construct_loop_tree(IR::NormalFunc *func);
