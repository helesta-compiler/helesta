#pragma once

#include "ir/ir.hpp"

std::map<IR::Reg, int> build_use_count(IR::NormalFunc *func);
std::map<IR::Reg, IR::RegWriteInstr *> build_defs(IR::NormalFunc *func);
std::map<IR::BB *, std::vector<IR::BB *>> build_prev(IR::NormalFunc *func);
void dag_ir(IR::CompileUnit *ir);

void mem2reg(IR::CompileUnit *);
void ssa_construction(IR::NormalFunc *, const std::unordered_set<IR::Reg> &);
void remove_unused_def(IR::CompileUnit *);
void func_inline(IR::CompileUnit *);
void global_code_motion(IR::CompileUnit *);
void global_value_numbering(IR::CompileUnit *);
void global_to_local(IR::CompileUnit *);
void simplify_expr(IR::CompileUnit *ir);
void before_backend(IR::CompileUnit *ir);

inline void gvn(IR::CompileUnit *ir) {
  PassEnabled("gvn") global_value_numbering(ir);
  remove_unused_def(ir);
  PassEnabled("se") simplify_expr(ir);
}

inline void gcm(IR::CompileUnit *ir) {
  PassEnabled("gcm") { global_code_motion(ir); }
}

inline void optimize_ir(IR::CompileUnit *ir) {
  PassEnabled("opt") {
    PassEnabled("mem2reg") mem2reg(ir);
    remove_unused_def(ir);
    gvn(ir);
    gcm(ir);
    gvn(ir);
  }
  PassEnabled("del-phi") before_backend(ir);
}
