#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

struct GVNNode;

struct GVNInstr {
  IR::Instr *i;
  GVNNode *node;
  bool removed = false;
};

struct GVNNode : Traversable<GVNNode>, TreeNode<GVNNode> {
  std::vector<GVNNode *> outs;
  GVNNode *fa;
  DomTreeNode *dom;
  std::vector<std::unique_ptr<GVNInstr>> instrs;
  bool visited = false;

  GVNNode(DomTreeNode *dom_) : dom(dom_) {}

  const std::vector<GVNNode *> getOutNodes() const override { return outs; }

  void addOutNode(GVNNode *node) override { outs.push_back(node); }

  GVNNode *getFather() const override { return fa; }
};

struct GVNContext {
  std::vector<std::unique_ptr<GVNNode>> nodes;
  // <constant> -> <reg>
  std::map<int32_t, int> int_constant_values;
  std::map<float, int> float_constant_values;
  // op <reg> -> <reg>
  std::map<std::pair<IR::UnaryOp::Type, int>, int> unary_values;
  // <reg> op <reg> -> <reg>
  std::map<std::tuple<IR::BinaryOp::Type, int, int>, int> binary_values;
  GVNNode *entry;
  std::vector<std::vector<GVNInstr *>> uses;
  IR::NormalFunc *func;

  GVNContext(DomTreeContext *ctx, IR::NormalFunc *func_) : func(func_) {
    nodes = transfer_graph<DomTreeNode, GVNNode>(ctx->nodes);
    for (auto &node : nodes) {
      auto outs = node->getOutNodes();
      for (auto out : outs) {
        out->fa = node.get();
      }
      if (node->dom == ctx->entry) {
        entry = node.get();
      }
      for (auto &i : node->dom->bb->instrs) {
        node->instrs.push_back(
            std::unique_ptr<GVNInstr>(new GVNInstr{i.release(), node.get()}));
      }
    }
  }

  void build_uses() {
    uses.resize(func->max_reg_id + 1);
    for (auto &node : nodes) {
      for (auto &i : node->instrs) {
        i->i->map_use([&](auto r) { uses[r.id].push_back(i.get()); });
      }
    }
  }

  void replace_same_value(int target_reg_id, int reference_reg_id) {
    for (auto use : uses[target_reg_id]) {
      use->i->map_use([&](auto &r) {
        if (r.id == target_reg_id)
          r.id = reference_reg_id;
      });
    }
  }

  void dfs(GVNNode *node) {
    auto outs = node->getOutNodes();
    std::vector<int32_t> new_ints;
    std::vector<float> new_floats;
    std::vector<std::pair<IR::UnaryOp::Type, int>> new_unaries;
    std::vector<std::tuple<IR::BinaryOp::Type, int, int>> new_binaries;
    for (auto &i : node->instrs) {
      if (auto load_const = dynamic_cast<IR::LoadConst<int32_t> *>(i->i)) {
        if (int_constant_values.find(load_const->value) !=
            int_constant_values.end()) {
          i->removed = true;
          replace_same_value(load_const->d1.id,
                             int_constant_values[load_const->value]);
        } else {
          int_constant_values[load_const->value] = load_const->d1.id;
          new_ints.push_back(load_const->value);
        }
      } else if (auto load_const = dynamic_cast<IR::LoadConst<float> *>(i->i)) {
        if (float_constant_values.find(load_const->value) !=
            float_constant_values.end()) {
          i->removed = true;
          replace_same_value(load_const->d1.id,
                             float_constant_values[load_const->value]);
        } else {
          float_constant_values[load_const->value] = load_const->d1.id;
          new_floats.push_back(load_const->value);
        }
      } else if (auto unary = dynamic_cast<IR::UnaryOpInstr *>(i->i)) {
        auto key = std::make_pair(unary->op.type, unary->s1.id);
        if (unary_values.find(key) != unary_values.end()) {
          i->removed = true;
          replace_same_value(unary->d1.id, unary_values[key]);
        } else {
          unary_values[key] = unary->d1.id;
          new_unaries.push_back(key);
        }
      } else if (auto binary = dynamic_cast<IR::BinaryOpInstr *>(i->i)) {
        auto key =
            std::make_tuple(binary->op.type, binary->s1.id, binary->s2.id);
        if (binary_values.find(key) != binary_values.end()) {
          i->removed = true;
          replace_same_value(binary->d1.id, binary_values[key]);
        } else {
          binary_values[key] = binary->d1.id;
          new_binaries.push_back(key);
        }
      }
    }
    for (auto out : outs) {
      dfs(out);
    }
    for (auto new_int : new_ints)
      int_constant_values.erase(new_int);
    for (auto new_float : new_floats)
      float_constant_values.erase(new_float);
    for (auto new_unary : new_unaries)
      unary_values.erase(new_unary);
    for (auto new_binary : new_binaries)
      binary_values.erase(new_binary);
  }

  void reconstruct() {
    for (size_t i = 0; i < func->bbs.size(); i++) {
      auto bb = func->bbs[i].get();
      auto node = nodes[i].get();
      bb->instrs.clear();
      for (auto &i : node->instrs) {
        auto raw_i = i.release();
        if (raw_i->removed)
          continue;
        bb->instrs.push_back(std::unique_ptr<IR::Instr>(raw_i->i));
      }
    }
  }
};

void global_value_numbering_func(IR::NormalFunc *func) {
  auto dom_ctx = construct_dom_tree(func);
  auto ctx = std::make_unique<GVNContext>(dom_ctx.get(), func);
  ctx->build_uses();
  ctx->dfs(ctx->entry);
  ctx->reconstruct();
}

void global_value_numbering(IR::CompileUnit *ir) {
  ir->for_each(global_value_numbering_func);
}
