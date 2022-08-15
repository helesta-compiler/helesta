#pragma once

#include "arm/program.hpp"

namespace ARMv7 {

void eliminate_branch(Program *);

inline void afterplay(Program *) {}

} // namespace ARMv7
