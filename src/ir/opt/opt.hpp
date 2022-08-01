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
void call_graph(IR::CompileUnit *);

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
  gvn(ir);
  gcm(ir);
  call_graph(ir);
  dag_ir(ir);
  gvn(ir);
  call_graph(ir);
  gvn(ir);
}
