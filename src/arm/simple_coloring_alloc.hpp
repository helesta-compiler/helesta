#pragma once

#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "arm/inst.hpp"

namespace ARMv7 {

struct RegAllocStat;
struct Func;

// no coalescing
class SimpleColoringAllocator {
  Func *func;
  std::vector<unsigned char> occur; // in fact it's boolean array
  std::vector<std::set<Reg>> interfere_edge;
  std::queue<Reg> simplify_nodes;
  std::vector<std::pair<Reg, std::vector<Reg>>> simplify_history;
  std::set<Reg> remain_pesudo_nodes;

  void build_graph(); // build occur, interfere_edge
  void spill(const std::vector<Reg> &spill_nodes);
  void remove(Reg r);
  void simplify();
  void clear();
  Reg choose_spill();

public:
  SimpleColoringAllocator(Func *_func);
  std::vector<Reg> run(RegAllocStat *stat);
};

} // namespace ARMv7