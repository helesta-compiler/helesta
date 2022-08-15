#pragma once

#include "arm/program.hpp"

namespace ARMv7 {

void merge_instr(Program *);

inline void foreplay(Program *prog) { PassEnabled("mi") merge_instr(prog); }

} // namespace ARMv7
