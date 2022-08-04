#include <variant>

#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

struct GVNNode;

struct GVNInstr {
  IR::Instr *i;
  GVNNode *node;
  bool removed = false;
  void setLoadConst(IR::Reg d1, IR::typed_scalar_t computed) {
    if (std::holds_alternative<int32_t>(computed)) {
      i = new IR::LoadConst<int32_t>(d1, std::get<int32_t>(computed));
    } else if (std::holds_alternative<float>(computed)) {
      i = new IR::LoadConst<float>(d1, std::get<float>(computed));
    }
  }
};

std::optional<IR::Reg> find_same_value(IR::PhiInstr *phi) {
  std::optional<IR::Reg> r0;
  for (auto &[r, bb] : phi->uses) {
    if (!(r == phi->d1)) {
      if (r0 && !(*r0 == r))
        return std::nullopt;
      r0 = r;
    }
  }
  return r0;
}

struct GVNNode : Traversable<GVNNode>, TreeNode<GVNNode> {
  std::vector<GVNNode *> outs;
  GVNNode *fa;
  DomTreeNode *dom;
  std::vector<std::unique_ptr<GVNInstr>> instrs;
  std::vector<IR::typed_scalar_t> new_scalars;
  std::vector<int> new_const_regs;
  std::vector<std::pair<IR::UnaryCompute, int>> new_unaries;
  std::vector<std::tuple<IR::BinaryCompute, int, int>> new_binaries;
  std::vector<std::tuple<int, int, int>> new_array_indexs;
  std::vector<int> new_args;
  std::vector<IR::MemObject *> new_addrs;
  bool visited = false;

  GVNNode(DomTreeNode *dom_) : dom(dom_) {}

  const std::vector<GVNNode *> getOutNodes() const override { return outs; }

  void addOutNode(GVNNode *node) override { outs.push_back(node); }

  GVNNode *getFather() const override { return fa; }
};

struct GVNContext {
  std::vector<std::unique_ptr<GVNNode>> nodes;
  // <scalar> -> <reg>
  std::map<IR::typed_scalar_t, int> scalar_reg_by_value;
  // <reg> -> <scalar>
  std::map<int, IR::typed_scalar_t> scalar_value_by_reg;
  // op <reg> -> <reg>
  std::map<std::pair<IR::UnaryCompute, int>, int> unary_values;
  // <reg> op <reg> -> <reg>
  std::map<std::tuple<IR::BinaryCompute, int, int>, int> binary_values;
  // <reg> + <reg> * <scalar> -> <reg>
  std::map<std::tuple<int, int, int>, int> array_index_values;
  // <scalar> -> <reg>
  std::map<int, int> arg_values;
  // <IR::MemObject*> -> <reg>
  std::map<IR::MemObject *, int> addr_values;
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
    scalar_reg_by_value.clear();
    scalar_value_by_reg.clear();
    unary_values.clear();
    binary_values.clear();
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
    assert(reference_reg_id > 0);
    for (auto use : uses[target_reg_id]) {
      use->i->map_use([&](auto &r) {
        if (r.id == target_reg_id)
          r.id = reference_reg_id;
      });
    }
  }

  template <typename Scalar>
  void process_scalar(GVNInstr *i, IR::LoadConst<Scalar> *load_const,
                      GVNNode *node) {
    if (scalar_reg_by_value.find(load_const->value) !=
        scalar_reg_by_value.end()) {
      i->removed = true;
      assert(scalar_reg_by_value[load_const->value] != 0);
      replace_same_value(load_const->d1.id,
                         scalar_reg_by_value[load_const->value]);
    } else {
      assert(load_const->d1.id != 0);
      scalar_reg_by_value[load_const->value] = load_const->d1.id;
      scalar_value_by_reg[load_const->d1.id] = load_const->value;
      node->new_scalars.push_back(load_const->value);
      node->new_const_regs.push_back(load_const->d1.id);
    }
  }

  void dfs(GVNNode *node) {
    auto outs = node->getOutNodes();
    for (auto &i : node->instrs) {
      if (auto load_const = dynamic_cast<IR::LoadConst<int32_t> *>(i->i)) {
        process_scalar(i.get(), load_const, node);
      } else if (auto load_const = dynamic_cast<IR::LoadConst<float> *>(i->i)) {
        process_scalar(i.get(), load_const, node);
      } else if (auto unary = dynamic_cast<IR::UnaryOpInstr *>(i->i)) {
        auto key = std::make_pair(unary->op.type, unary->s1.id);
        if (unary_values.find(key) != unary_values.end()) {
          i->removed = true;
          assert(unary_values[key] != 0);
          replace_same_value(unary->d1.id, unary_values[key]);
        } else if (scalar_value_by_reg.find(unary->s1.id) !=
                   scalar_value_by_reg.end()) {
          auto value = scalar_value_by_reg[unary->s1.id];
          auto computed = IR::compute(unary->op.type, value);
          if (scalar_reg_by_value.find(computed) != scalar_reg_by_value.end()) {
            i->removed = true;
            assert(scalar_reg_by_value[computed] != 0);
            replace_same_value(unary->d1.id, scalar_reg_by_value[computed]);
          } else {
            assert(unary->d1.id != 0);
            scalar_reg_by_value[computed] = unary->d1.id;
            scalar_value_by_reg[unary->d1.id] = computed;
            i->setLoadConst(unary->d1, computed);
            node->new_scalars.push_back(computed);
            node->new_const_regs.push_back(unary->d1.id);
          }
        } else if (IR::is_useless_compute(unary->op.type)) {
          i->removed = true;
          assert(unary->s1.id != 0);
          replace_same_value(unary->d1.id, unary->s1.id);
        } else {
          unary_values[key] = unary->d1.id;
          node->new_unaries.push_back(key);
        }

      } else if (auto binary = dynamic_cast<IR::BinaryOpInstr *>(i->i)) {
        auto key =
            std::make_tuple(binary->op.type, binary->s1.id, binary->s2.id);

        if (binary_values.find(key) != binary_values.end()) {
          i->removed = true;
          assert(binary_values[key] != 0);
          replace_same_value(binary->d1.id, binary_values[key]);
        } else if (scalar_value_by_reg.find(binary->s1.id) !=
                       scalar_value_by_reg.end() &&
                   scalar_value_by_reg.find(binary->s2.id) !=
                       scalar_value_by_reg.end()) {
          auto s1_value = scalar_value_by_reg[binary->s1.id];
          auto s2_value = scalar_value_by_reg[binary->s2.id];
          auto computed = IR::compute(binary->op.type, s1_value, s2_value);
          if (scalar_reg_by_value.find(computed) != scalar_reg_by_value.end()) {
            i->removed = true;
            assert(scalar_reg_by_value[computed] != 0);
            replace_same_value(binary->d1.id, scalar_reg_by_value[computed]);
          } else {
            assert(binary->d1.id != 0);
            scalar_reg_by_value[computed] = binary->d1.id;
            scalar_value_by_reg[binary->d1.id] = computed;
            i->setLoadConst(binary->d1, computed);
            node->new_scalars.push_back(computed);
            node->new_const_regs.push_back(binary->d1.id);
          }
        } else {
          std::optional<IR::typed_scalar_t> s1_value = std::nullopt;
          std::optional<IR::typed_scalar_t> s2_value = std::nullopt;
          if (scalar_value_by_reg.find(binary->s1.id) !=
              scalar_value_by_reg.end()) {
            s1_value = scalar_value_by_reg[binary->s1.id];
          }
          if (scalar_value_by_reg.find(binary->s2.id) !=
              scalar_value_by_reg.end()) {
            s2_value = scalar_value_by_reg[binary->s2.id];
          }
          if (IR::is_useless_compute(binary->op.type, s1_value, s2_value)) {
            i->removed = true;
            if (s1_value.has_value()) {
              assert(binary->s1.id != 0);
              replace_same_value(binary->d1.id, binary->s2.id);
            } else if (s2_value.has_value()) {
              assert(binary->s2.id != 0);
              replace_same_value(binary->d1.id, binary->s1.id);
            } else
              assert(false);
          } else {
            auto key_s1_s2 =
                std::make_tuple(binary->op.type, binary->s1.id, binary->s2.id);
            auto key_s2_s1 =
                std::make_tuple(binary->op.type, binary->s2.id, binary->s1.id);
            if (binary_values.find(key_s1_s2) != binary_values.end()) {
              i->removed = true;
              assert(binary_values[key_s1_s2] != 0);
              replace_same_value(binary->d1.id, binary_values[key_s1_s2]);
            } else {
              node->new_binaries.push_back(key_s1_s2);
              binary_values[key_s1_s2] = binary->d1.id;
              if (key_s1_s2 != key_s2_s1 &&
                  IR::is_commutable_compute(binary->op.type)) {
                node->new_binaries.push_back(key_s2_s1);
                binary_values[key_s2_s1] = binary->d1.id;
              }
            }
          }
        }
      } else if (auto ai = dynamic_cast<IR::ArrayIndex *>(i->i)) {
        std::optional<IR::typed_scalar_t> s2_value = std::nullopt;
        if (scalar_value_by_reg.find(ai->s2.id) != scalar_value_by_reg.end()) {
          s2_value = scalar_value_by_reg[ai->s2.id];
        }

        if (s2_value && std::holds_alternative<int32_t>(*s2_value) &&
            std::get<int32_t>(*s2_value) == 0) {
          i->removed = true;
          assert(ai->s1.id != 0);
          replace_same_value(ai->d1.id, ai->s1.id);
        } else {
          auto key = std::make_tuple(ai->s1.id, ai->s2.id, ai->size);
          if (array_index_values.count(key)) {
            i->removed = true;
            assert(array_index_values[key] != 0);
            replace_same_value(ai->d1.id, array_index_values[key]);
          } else {
            array_index_values[key] = ai->d1.id;
            node->new_array_indexs.push_back(key);
          }
        }
      } else if (auto la = dynamic_cast<IR::LoadArg *>(i->i)) {
        auto key = la->id;
        if (arg_values.count(key)) {
          i->removed = true;
          assert(arg_values[key] != 0);
          replace_same_value(la->d1.id, arg_values[key]);
        } else {
          arg_values[key] = la->d1.id;
          node->new_args.push_back(key);
        }
      } else if (auto la = dynamic_cast<IR::LoadAddr *>(i->i)) {
        auto key = la->offset;
        if (addr_values.count(key)) {
          i->removed = true;
          assert(addr_values[key] != 0);
          replace_same_value(la->d1.id, addr_values[key]);
        } else {
          addr_values[key] = la->d1.id;
          node->new_addrs.push_back(key);
        }
      } else if (auto phi = dynamic_cast<IR::PhiInstr *>(i->i)) {
        if (auto r0 = find_same_value(phi)) {
          i->removed = true;
          replace_same_value(phi->d1.id, r0->id);
        }
      }
    }
    for (auto out : outs) {
      dfs(out);
    }
    for (auto new_scalar : node->new_scalars) {
      assert(scalar_reg_by_value.erase(new_scalar) == 1);
    }
    for (auto new_const_reg : node->new_const_regs) {
      assert(scalar_value_by_reg.erase(new_const_reg) == 1);
    }
    for (auto new_unary : node->new_unaries) {
      assert(unary_values.erase(new_unary) == 1);
    }
    for (auto new_binary : node->new_binaries) {
      assert(binary_values.erase(new_binary) == 1);
    }
    for (auto new_array_index : node->new_array_indexs) {
      assert(array_index_values.erase(new_array_index) == 1);
    }
    for (auto new_arg : node->new_args) {
      assert(arg_values.erase(new_arg) == 1);
    }
    for (auto new_addr : node->new_addrs) {
      assert(addr_values.erase(new_addr) == 1);
    }
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
