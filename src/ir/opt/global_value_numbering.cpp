#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

struct GVNNode : Traversable<GVNNode>, TreeNode<GVNNode> {
  std::vector<GVNNode *> outs;
  GVNNode *fa;
  DomTreeNode *dom;
  bool visited = false;

  GVNNode(DomTreeNode *dom_) : dom(dom_) {}

  const std::vector<GVNNode *> getOutNodes() const override { return outs; }

  void addOutNode(GVNNode *node) override { outs.push_back(node); }

  GVNNode *getFather() const override { return fa; }
};

struct GVNContext {
  std::vector<std::unique_ptr<GVNNode>> nodes;
  std::vector<GVNNode *> dfn;
  // <constant> -> <reg>
  std::map<int, int> constant_value_map;
  // op <reg> -> <reg>
  std::map<std::pair<IR::UnaryOp, int>, int> unary_value_map;
  // <reg> op <reg> -> <reg>
  std::map<std::tuple<IR::BinaryOp, int, int>, int> binary_value_map;
  GVNNode *entry;

  GVNContext(DomTreeContext *ctx) {
    nodes = transfer_graph<DomTreeNode, GVNNode>(ctx->nodes);
    for (auto &node : nodes) {
      auto outs = node->getOutNodes();
      for (auto out : outs) {
        out->fa = node.get();
      }
      if (node->dom == ctx->entry) {
        entry = node.get();
      }
    }
  }

  void dfs(GVNNode *node) {
    if (node->visited)
      return;
    node->visited = true;
    auto outs = node->getOutNodes();
    for (auto out : outs) {
      dfs(out);
    }
    dfn.push_back(node);
  }
};

void global_value_numbering_func(IR::NormalFunc *func) {
  auto dom_ctx = construct_dom_tree(func);
  auto ctx = std::make_unique<GVNContext>(dom_ctx.get());
  ctx->dfs(ctx->entry);
  for (auto it = ctx->dfn.rbegin(); it != ctx->dfn.rend(); it++) {
  }
}

void global_value_numbering(IR::CompileUnit *ir) {
  ir->for_each(global_value_numbering_func);
}
