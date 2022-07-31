#pragma once

#include "ir/ir.hpp"

std::map<IR::Reg, IR::RegWriteInstr *> build_defs(IR::NormalFunc *func);
std::map<IR::BB *, std::vector<IR::BB *>> build_prev(IR::NormalFunc *func);
void dag_ir(IR::CompileUnit *ir);

void mem2reg(IR::CompileUnit *);
void ssa_construction(IR::NormalFunc *, const std::unordered_set<IR::Reg> &);
void remove_unused_def(IR::CompileUnit *);
void global_code_motion(IR::CompileUnit *);
void global_value_numbering(IR::CompileUnit *);
void simplify_expr(IR::CompileUnit *ir);
void inference_pure_call(IR::CompileUnit *);
// void func_inline(IR::CompileUnit &);

inline void gvn(IR::CompileUnit *ir) {
  if (!global_config.disabled_passes.count("gvn")) {
    global_value_numbering(ir);
    remove_unused_def(ir);
    simplify_expr(ir);
  }
}

inline void gcm(IR::CompileUnit *ir) {
  if (!global_config.disabled_passes.count("gvm")) {
    global_code_motion(ir);
  }
}

inline void optimize_ir(IR::CompileUnit *ir) {
  mem2reg(ir);
  remove_unused_def(ir);
  inference_pure_call(ir);
  // func_inline(*ir);
  gvn(ir);
  gcm(ir);
  // simplify_load_store(ir);
  dag_ir(ir);
  gvn(ir);
  gvn(ir);
}
