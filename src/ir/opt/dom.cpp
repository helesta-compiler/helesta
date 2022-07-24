#include <unordered_map>

#include "ir/opt/dom.hpp"

DomTreeContext::DomTreeContext(IR::NormalFunc *func) {
  std::unordered_map<IR::BB *, DomTreeNode *> bb2node;
  for (auto &bb : func->bbs) {
    nodes.push_back(std::make_unique<DomTreeNode>(this));
    bb2node.insert({bb.get(), nodes.back().get()});
  }
  for (auto &bb : func->bbs) {
    auto node = bb2node[bb.get()];
    auto outs = bb->getOutNodes();
    for (auto out : outs) {
      node->out_nodes.push_back(bb2node[out]);
    }
  }
}

std::unique_ptr<DomTreeContext> dominator_tree(IR::NormalFunc *func) {
  auto ctx = std::make_unique<DomTreeContext>(func);
  return ctx;
}
