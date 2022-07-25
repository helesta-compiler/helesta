#include <unordered_set>

#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

std::multiset<IR::PhiInstr *> phi_insertion(DomTreeContext *ctx,
                                            IR::Reg checking_reg) {
  // 1. construct Defs
  std::multiset<DomTreeNode *> defs;
  for (auto &node : ctx->nodes) {
    auto bb = node->bb;
    bb->for_each([&](IR::Instr *i) {
      bool bb_in = false;
      if (auto reg_writer_instr = dynamic_cast<IR::RegWriteInstr *>(i)) {
        if (reg_writer_instr->d1 == checking_reg) {
          bb_in = true;
        }
      }
      if (bb_in) {
        defs.insert(node.get());
      }
    });
  }
  // 2. perform phi insertion
  std::multiset<IR::PhiInstr *> phis;
  std::deque<DomTreeNode *> W{defs.begin(),
                              defs.end()}; // the exact `W` in the SSA 3.1
  std::unordered_set<DomTreeNode *> F; // set of basic blocks where phi is added
  while (!W.empty()) {
    auto node = W.front();
    W.pop_front();
    for (auto df : node->dom_frontiers) {
      if (F.find(df) == F.end()) {
        F.insert(df);
        auto instr = std::make_unique<IR::PhiInstr>(checking_reg);
        phis.insert(instr.get());
        df->bb->instrs.push_front(std::move(instr));
        if (defs.find(df) != defs.end()) {
          W.push_back(df);
        }
      }
    }
  }
  return phis;
}

inline void update_reaching_def(std::vector<int> &reaching_def,
                                std::vector<DomTreeNode *> &def_node,
                                int reg_id, DomTreeNode *cur) {
  auto x = reg_id;
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
                       IR::Reg checking_reg,
                       const std::multiset<IR::PhiInstr *> phis) {
  std::vector<int> reaching_def(func->max_reg_id, 0);
  std::vector<DomTreeNode *> def_node(func->max_reg_id, nullptr);
  int version_count = 0;
  for (auto node : ctx->dfn) {
    node->bb->for_each([&](IR::Instr *i) {
      if (dynamic_cast<IR::PhiInstr *>(i)) {
        // skip phi instrs
      } else {
        i->map_use([&](IR::Reg &cur) {
          if (cur.id != checking_reg.id)
            return;
          update_reaching_def(reaching_def, def_node, cur.id, node);
          cur.id = reaching_def[cur.id];
        });
      }
      if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i)) {
        if (reg_write_instr->d1.id == checking_reg.id) {
          update_reaching_def(reaching_def, def_node, checking_reg.id, node);
          auto new_version = func->new_Reg(func->get_name(checking_reg) + "_" +
                                           std::to_string(version_count++));
          reaching_def[new_version.id] = reaching_def[checking_reg.id];
          def_node[new_version.id] = node;
          reaching_def[checking_reg.id] = new_version.id;
        }
      }
    });
    for (auto succ_bb : node->bb->getOutNodes()) {
      succ_bb->for_each([&](IR::Instr *i) {
        if (auto phi_instr = dynamic_cast<IR::PhiInstr *>(i)) {
          if (phis.find(phi_instr) != phis.end()) {
            update_reaching_def(reaching_def, def_node, checking_reg.id, node);
            phi_instr->add_use(IR::Reg(reaching_def[checking_reg.id]),
                               node->bb);
          } else {
            for (auto &kv : phi_instr->uses) {
              if (kv.second == node->bb && kv.first.id == checking_reg.id) {
                update_reaching_def(reaching_def, def_node, checking_reg.id,
                                    node);
                kv.first.id = reaching_def[checking_reg.id];
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
  for (auto reg : checking_regs) {
    auto dom_ctx = DomTreeBuilderContext(func).construct_dom_tree();
    // phase 1: phi insertion
    auto phis = phi_insertion(dom_ctx.get(), reg);
    // phase 2: varaible renaming
    varaible_renaming(func, dom_ctx.get(), reg, phis);
  }
}
