#include <unordered_set>

#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

std::vector<std::pair<IR::Reg, std::vector<DomTreeNode *>>>
construct_defs(DomTreeContext *ctx,
               const std::unordered_set<IR::Reg> &checking_regs) {

  std::unordered_map<IR::Reg, std::vector<DomTreeNode *>> defs;
  for (auto &node : ctx->nodes) {
    auto bb = node->bb;
    bb->for_each([&](IR::Instr *i) {
      if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i)) {
        if (checking_regs.find(reg_write_instr->d1) != checking_regs.end()) {
          defs[reg_write_instr->d1].push_back(node.get());
        }
      }
    });
  }
  return std::vector<std::pair<IR::Reg, std::vector<DomTreeNode *>>>{
      defs.begin(), defs.end()};
}

void phi_insertion(IR::Reg checking_reg, const std::vector<DomTreeNode *> &defs,
                   std::unordered_set<IR::PhiInstr *> &phis) {
  std::deque<DomTreeNode *> W{defs.begin(),
                              defs.end()}; // the exact `W` in the SSA 3.1
  std::unordered_set<DomTreeNode *> F; // set of basic blocks where phi is added
  std::unordered_set<DomTreeNode *> def_set(defs.begin(), defs.end());
  while (!W.empty()) {
    auto node = W.front();
    W.pop_front();
    for (auto df : node->dom_frontiers) {
      if (F.find(df) == F.end()) {
        F.insert(df);
        auto instr = std::make_unique<IR::PhiInstr>(checking_reg);
        phis.insert(instr.get());
        df->bb->instrs.push_front(std::move(instr));
        if (def_set.find(df) == def_set.end()) {
          W.push_back(df);
        }
      }
    }
  }
}

inline void update_reaching_def(std::vector<int> &reaching_def,
                                std::vector<DomTreeNode *> &def_node,
                                int reg_id, DomTreeNode *cur) {
  assert(cur != nullptr);
  assert(reaching_def.size() == def_node.size());
  auto x = reaching_def[reg_id];
  while (x > 0) {
    auto node = def_node[x];
    if (node->sdom(cur) || node == cur) {
      break;
    }
    x = reaching_def[x];
  }
  reaching_def[reg_id] = x;
}

void varaible_renaming(IR::NormalFunc *func, DomTreeContext *ctx,
                       const std::unordered_set<IR::Reg> &checking_regs,
                       const std::unordered_set<IR::PhiInstr *> &phis) {
  std::vector<int> reaching_def;
  std::vector<int> version_count;
  std::vector<int> origin_name;
  std::vector<DomTreeNode *> def_node;
  reaching_def.resize(func->max_reg_id + 1, 0);
  version_count.resize(func->max_reg_id + 1, 0);
  origin_name.resize(func->max_reg_id + 1, 0);
  for (auto reg : checking_regs)
    origin_name[reg.id] = reg.id;
  def_node.resize(func->max_reg_id + 1, nullptr);
  for (auto node : ctx->dfn) {
    node->bb->for_each([&](IR::Instr *i) {
      if (dynamic_cast<IR::PhiInstr *>(i)) {
        // skip phi instrs
      } else {
        i->map_use([&](IR::Reg &cur) {
          if (checking_regs.find(cur) == checking_regs.end())
            return;
          update_reaching_def(reaching_def, def_node, cur.id, node);
          assert(reaching_def[cur.id] != 0);
          cur.id = reaching_def[cur.id];
        });
      }
      if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i)) {
        if (checking_regs.find(reg_write_instr->d1) != checking_regs.end()) {
          update_reaching_def(reaching_def, def_node, reg_write_instr->d1.id,
                              node);
          auto new_version = func->new_Reg(
              func->get_name(reg_write_instr->d1) + "_" +
              std::to_string(version_count[reg_write_instr->d1.id]++));
          reaching_def.resize(func->max_reg_id + 1, 0);
          version_count.resize(func->max_reg_id + 1, 0);
          origin_name.resize(func->max_reg_id + 1, 0);
          def_node.resize(func->max_reg_id + 1, nullptr);
          reaching_def[new_version.id] = reaching_def[reg_write_instr->d1.id];
          def_node[new_version.id] = node;
          origin_name[new_version.id] = origin_name[reg_write_instr->d1.id];
          reaching_def[reg_write_instr->d1.id] = new_version.id;
          reg_write_instr->d1.id = new_version.id;
        }
      }
    });
    for (auto succ_bb : node->bb->getOutNodes()) {
      succ_bb->for_each([&](IR::Instr *i) {
        if (auto phi_instr = dynamic_cast<IR::PhiInstr *>(i)) {
          if (phis.find(phi_instr) != phis.end()) {
            auto reg_id = origin_name[phi_instr->d1.id];
            update_reaching_def(reaching_def, def_node, reg_id, node);
            assert(reaching_def[phi_instr->d1.id] > 0);
            phi_instr->add_use(IR::Reg(reaching_def[reg_id]), node->bb);
          } else {
            for (auto &kv : phi_instr->uses) {
              if (kv.second == node->bb &&
                  checking_regs.find(kv.first) != checking_regs.end()) {
                update_reaching_def(reaching_def, def_node, kv.first.id, node);
                kv.first.id = reaching_def[kv.first.id];
              }
            }
          }
        }
      });
    }
  }
}

void ssa_construction(IR::NormalFunc *func,
                      const std::unordered_set<IR::Reg> &checking_regs) {
  auto dom_ctx = dominator_tree(func);
  std::unordered_set<IR::PhiInstr *> phis;
  // phase 0: construct defs
  auto reg_with_defs = construct_defs(dom_ctx.get(), checking_regs);
  for (auto reg_with_def : reg_with_defs) {
    // phase 1: phi insertion
    phi_insertion(reg_with_def.first, reg_with_def.second, phis);
  }
  // phase 2: varaible renaming
  varaible_renaming(func, dom_ctx.get(), checking_regs, phis);
}
