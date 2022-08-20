#pragma once

#include "arm/program.hpp"

namespace ARMv7 {

void merge_instr(Program *);
void replace_pseduo_inst(Program *);
void dce(Program *);

inline void foreplay(Program *prog) {
  PassEnabled("mi") merge_instr(prog);
  replace_pseduo_inst(prog);
  PassEnabled("dce") dce(prog);
}

} // namespace ARMv7
