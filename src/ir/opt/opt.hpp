#pragma once

#include "ir/ir.hpp"

void mem2reg(IR::CompileUnit *);
void ssa_construction(IR::NormalFunc *, const std::unordered_set<IR::Reg> &);
void remove_unused_def(IR::CompileUnit *);
void func_inline(IR::CompileUnit *);
void global_code_motion(IR::CompileUnit *);
void global_value_numbering(IR::CompileUnit *);
void simplify_load_store(IR::CompileUnit *);

inline void optimize_ir(IR::CompileUnit *ir) {
  if (global_config.disabled_passes.find("func-inline") == gloabl_config.disabled_passes.end()){
  func_inline(ir);
  }
  mem2reg(ir);
  // std::cerr << "end func inline" << std::endl;
  remove_unused_def(ir);
  if (global_config.disabled_passes.find("gvn") ==
      global_config.disabled_passes.end()) {
    global_value_numbering(ir);
    remove_unused_def(ir);
  }
  if (global_config.disabled_passes.find("gvm") ==
      global_config.disabled_passes.end()) {
    global_code_motion(ir);
  }
  simplify_load_store(ir);
  if (global_config.disabled_passes.find("gvn") ==
      global_config.disabled_passes.end()) {
    global_value_numbering(ir);
    remove_unused_def(ir);
  }
}
