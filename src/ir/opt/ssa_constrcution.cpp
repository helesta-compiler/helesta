#include <unordered_set>

#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

void phi_insertion(DomTreeContext *ctx, IR::Reg checking_reg) {
  // 1. construct Defs
  std::vector<IR::BB *> defs;
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
        defs.push_back(bb);
      }
    });
  }
}

void ssa_construction_func(IR::NormalFunc *func,
                           const std::unordered_set<IR::Reg> &checking_regs) {
  for (auto reg : checking_regs) {
    auto dom_ctx = DomTreeBuilderContext(func).construct_dom_tree();
    // phase 1: phi insertion
    phi_insertion(dom_ctx.get(), reg);
    // phase 2: varaible renaming
  }
}

void ssa_construction(IR::CompileUnit *ir,
                      const std::unordered_set<IR::Reg> &checking_regs) {
  ir->for_each([&](IR::NormalFunc *func) {
    ssa_construction_func(func, checking_regs);
  });
}
