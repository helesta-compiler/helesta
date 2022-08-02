#pragma once

#include "ir/ir.hpp"

std::map<IR::Reg, int> build_use_count(IR::NormalFunc *func);
std::map<IR::Reg, IR::RegWriteInstr *> build_defs(IR::NormalFunc *func);
std::map<IR::BB *, std::vector<IR::BB *>> build_prev(IR::NormalFunc *func);
void dag_ir(IR::CompileUnit *ir);

void mem2reg(IR::CompileUnit *);
void ssa_construction(IR::NormalFunc *, const std::unordered_set<IR::Reg> &);
void remove_unused_def(IR::CompileUnit *);
void remove_unused_def_func(IR::NormalFunc *);
void func_inline(IR::CompileUnit *);
void global_code_motion(IR::CompileUnit *);
void global_value_numbering(IR::CompileUnit *);
void simplify_expr(IR::CompileUnit *ir);
void call_graph(IR::CompileUnit *);
void remove_unused_BB(IR::CompileUnit *ir);
void before_backend(IR::CompileUnit *ir);

#define PassEnabled(name) if (!global_config.disabled_passes.count(name))
#define PassDisabled(name) if (global_config.disabled_passes.count(name))

inline void gvn(IR::CompileUnit *ir) {
  PassEnabled("gvn") {
    global_value_numbering(ir);
    remove_unused_def(ir);
    PassEnabled("se") simplify_expr(ir);
  }
}

inline void gcm(IR::CompileUnit *ir) {
  PassEnabled("gcm") { global_code_motion(ir); }
}

inline void optimize_ir(IR::CompileUnit *ir) {
  PassDisabled("opt") return;
  PassEnabled("mem2reg") mem2reg(ir);
  remove_unused_def(ir);
  remove_unused_BB(ir);
  gvn(ir);
  gcm(ir);
  call_graph(ir);
  dag_ir(ir);
  gvn(ir);
  call_graph(ir);
  gvn(ir);
  if(0)
  PassEnabled("func-inline") {
    func_inline(ir);
    dag_ir(ir);
    gvn(ir);
  }
  gcm(ir);
  before_backend(ir);
}
