#pragma once

#include <vector>

#include "arm/archinfo.hpp"
#include "arm/func.hpp"
#include "arm/inst.hpp"

struct RegAllocStat {
  int spill_cnt, move_eliminated, callee_save_used;
  bool succeed;
};

namespace ARMv7 {

class ColoringAllocator {
protected:
  Func *func;

public:
  ColoringAllocator(Func *_func) : func(_func) {}
  virtual std::vector<int> run(RegAllocStat *stat) = 0;
  virtual void clear() = 0;
};

} // namespace ARMv7
