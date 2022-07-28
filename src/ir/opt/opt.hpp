#pragma once

#include "ir/ir.hpp"

void mem2reg(IR::CompileUnit *);
void ssa_construction(IR::NormalFunc *, const std::unordered_set<IR::Reg> &);
void remove_unused_def(IR::CompileUnit *);
void global_code_motion(IR::CompileUnit *);

inline void optimize_ir(IR::CompileUnit *ir) {
  mem2reg(ir);
  remove_unused_def(ir);
  if (global_config.disabled_passes.find("gvm") ==
      global_config.disabled_passes.end())
    global_code_motion(ir);
}
