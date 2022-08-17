#pragma once

#include <bitset>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <utility>

#include "arm/func.hpp"
#include "arm/inst.hpp"
#include "arm/irc_coloring_alloc.hpp"
#include "arm/regalloc.hpp"
#include "arm/simple_coloring_alloc.hpp"
#include "ir/ir.hpp"

namespace ARMv7 {

struct Program {
  std::vector<std::unique_ptr<Func>> funcs;
  std::vector<std::unique_ptr<GlobalObject>> global_objects;
  int block_n;

  Program(IR::CompileUnit *ir);
  void gen_global_var_asm(std::ostream &out);
  void gen_asm(std::ostream &out);
  void allocate_register();
};

} // namespace ARMv7
