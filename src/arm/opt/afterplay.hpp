#pragma once

#include "arm/program.hpp"

namespace ARMv7 {

void eliminate_branch(Program *);
void replace_complex_inst(Program *);
void remove_trivial_inst(Program *);

inline void afterplay(Program *prog) {
  replace_complex_inst(prog);
  remove_trivial_inst(prog);
}

} // namespace ARMv7
