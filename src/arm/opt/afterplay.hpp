#pragma once

#include "arm/program.hpp"

namespace ARMv7 {

void replace_complex_inst(Program *);
void remove_trivial_inst(Program *);
bool eliminate_branch(Program *);
void remove_empty_blocks(Program *);

inline void afterplay(Program *prog) {
  replace_complex_inst(prog);
  remove_trivial_inst(prog);
  PassEnabled("eliminate-branch") {
    while (eliminate_branch(prog))
      remove_empty_blocks(prog);
  }
}
} // namespace ARMv7
