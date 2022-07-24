#pragma once

#include "ir/ir.hpp"

void mem2reg(IR::CompileUnit *);

inline void optimize_ir(IR::CompileUnit *ir) { mem2reg(ir); }
