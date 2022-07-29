#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

struct GVNNode : Traversable<GVNNode>, TreeNode<GVNNode> {
  std::vector<GVNNode *> outs;
  GVNNode *fa;
  DomTreeNode *dom;

  GVNNode(DomTreeNode *dom_) : dom(dom_) { }

  const std::vector<GVNNode *> getOutNodes() const override { return outs; }

  void addOutNode(GVNNode *node) override { outs.push_back(node); }

  GVNNode *getFather() const override { return fa; }
};

struct GVNContext {
  std::vector<std::unique_ptr<GVNNode>> nodes;
  std::vector<GVNNode *> dfn;

  GVNContext(DomTreeContext *ctx) {
      nodes = transfer_graph<DomTreeNode, GVNNode>(ctx->nodes);
      for (auto &node: nodes) {
          auto outs = node->getOutNodes();
          for (auto out: outs) {
              out->fa = node.get();
          }
      }
  }
};

void global_value_numbering_func(IR::NormalFunc *func) {
  auto dom_ctx = construct_dom_tree(func);
}

void global_value_numbering(IR::CompileUnit *ir) {
  ir->for_each(global_value_numbering_func);
}
