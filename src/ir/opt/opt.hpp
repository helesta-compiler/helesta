#pragma once

#include "ir/ir.hpp"

void mem2reg(IR::CompileUnit *);
void ssa_construction(IR::NormalFunc *, const std::unordered_set<IR::Reg> &);
void remove_unused_def(IR::CompileUnit *);
void func_inline(IR::CompileUnit *);

inline void optimize_ir(IR::CompileUnit *ir) {
  // ir->print(std::cout);
  func_inline(ir);
  // ir->print(std::cout);
  // mem2reg(ir);
  // remove_unused_def(ir);
}
