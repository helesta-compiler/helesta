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
#include "arm/simple_coloring_alloc.hpp"
#include "ir/ir.hpp"

namespace ARMv7 {

inline int dynamic_allocable(const Reg &r) {
  if (r.type == ScalarType::Int)
    return RegConvention<ScalarType::Int>::allocable(r.id);
  else
    return RegConvention<ScalarType::Float>::allocable(r.id);
}

struct AsmContext {
  int32_t temp_sp_offset;
  std::function<bool(std::ostream &)> epilogue;
};

struct Program {
  std::vector<std::unique_ptr<Func>> funcs;
  std::vector<std::unique_ptr<GlobalObject>> global_objects;
  int block_n;

  Program(IR::CompileUnit *ir);
  void gen_global_var_asm(std::ostream &out);
  void gen_asm(std::ostream &out);
};

} // namespace ARMv7
