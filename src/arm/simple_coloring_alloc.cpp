#include "arm/simple_coloring_alloc.hpp"

#include <memory>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "arm/archinfo.hpp"
#include "arm/inst.hpp"
#include "arm/program.hpp"
#include "common/common.hpp"

using std::make_unique;
using std::set;
using std::vector;

namespace ARMv7 {

SimpleColoringAllocator::SimpleColoringAllocator(Func *_func) : func(_func) {}

void SimpleColoringAllocator::spill(const vector<Reg> &spill_nodes) {
  vector<StackObject *> spill_obj;
  set<Reg> constant_spilled;
  for (size_t i = 0; i < spill_nodes.size(); ++i)
    if (func->constant_reg.find(spill_nodes[i].id) !=
            func->constant_reg.end() ||
        func->symbol_reg.find(spill_nodes[i].id) != func->symbol_reg.end()) {
      constant_spilled.insert(spill_nodes[i]);
      spill_obj.push_back(nullptr);
    } else {
      StackObject *t = new StackObject();
      t->size = INT_SIZE;
      t->position = -1;
      func->stack_objects.emplace_back(t);
      spill_obj.push_back(t);
    }
  for (auto &block : func->blocks)
    for (auto i = block->insts.begin(); i != block->insts.end();) {
      bool need_continue = false;
      for (Reg r : (*i)->def_reg())
        if (constant_spilled.find(r) != constant_spilled.end()) {
          auto nxt = std::next(i);
          block->insts.erase(i);
          i = nxt;
          need_continue = true;
          break;
        }
      if (need_continue)
        continue;
      for (size_t j = 0; j < spill_nodes.size(); ++j) {
        Reg r = spill_nodes[j];
        bool cur_def = (*i)->def(r), cur_use = (*i)->use(r);
        if (func->constant_reg.find(r.id) != func->constant_reg.end()) {
          assert(!cur_def);
          if (cur_use) {
            Reg tmp{func->reg_n++, (bool)func->float_regs.count(r.id)};
            func->spilling_reg.insert(tmp);
            func->constant_reg[tmp] = func->constant_reg[r];
            insert(block->insts, i, load_imm(tmp, func->constant_reg[r]));
            (*i)->replace_reg(r, tmp);
          }
        } else if (func->symbol_reg.find(r) != func->symbol_reg.end()) {
          assert(!cur_def);
          if (cur_use) {
            Reg tmp{func->reg_n++, (bool)func->float_regs.count(r.id)};
            func->spilling_reg.insert(tmp);
            func->symbol_reg[tmp] = func->symbol_reg[r];
            insert(block->insts, i, load_symbol_addr(tmp, func->symbol_reg[r]));
            (*i)->replace_reg(r, tmp);
          }
        } else {
          if (cur_def || cur_use) {
            StackObject *cur_obj = spill_obj[j];
            Reg tmp{func->reg_n++, (bool)func->float_regs.count(r)};
            func->spilling_reg.insert(tmp);
            if (cur_use)
              block->insts.insert(i, make_unique<LoadStack>(tmp, 0, cur_obj));
            if (cur_def)
              block->insts.insert(std::next(i),
                                  make_unique<StoreStack>(tmp, 0, cur_obj));
            (*i)->replace_reg(r, tmp);
          }
        }
      }
      ++i;
    }
}

void SimpleColoringAllocator::build_graph() {
  occur.resize(func->reg_n);
  interfere_edge.resize(func->reg_n);
  std::fill(occur.begin(), occur.end(), 0);
  std::fill(interfere_edge.begin(), interfere_edge.end(), set<Reg>{});
  func->calc_live();
  vector<Reg> temp, new_nodes;
  for (auto &block : func->blocks) {
    set<Reg> live = block->live_out;
    temp.clear();
    for (Reg r : live)
      if (r.is_pseudo() || allocable(r.id, r.is_float))
        temp.push_back(r);
    if (block->insts.size() > 0)
      for (Reg r : (*block->insts.rbegin())->def_reg())
        if (r.is_pseudo() || allocable(r.id, r.is_float))
          temp.push_back(r);
    for (size_t idx1 = 0; idx1 < temp.size(); ++idx1)
      for (size_t idx0 = 0; idx0 < idx1; ++idx0)
        if (temp[idx0] != temp[idx1]) {
          interfere_edge[temp[idx0].get_index()].insert(temp[idx1]);
          interfere_edge[temp[idx1].get_index()].insert(temp[idx0]);
        }
    for (auto i = block->insts.rbegin(); i != block->insts.rend(); ++i) {
      new_nodes.clear();
      for (Reg r : (*i)->def_reg())
        if (r.is_pseudo() || allocable(r.id, r.is_float)) {
          occur[r.get_index()] = 1;
          live.erase(r);
        }
      for (Reg r : (*i)->use_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          occur[r.get_index()] = 1;
          if (live.find(r) == live.end()) {
            for (Reg o : live) {
              interfere_edge[r.get_index()].insert(o);
              interfere_edge[o.get_index()].insert(r);
            }
            live.insert(r);
          }
          new_nodes.push_back(r);
        }
      if (std::next(i) != block->insts.rend())
        for (Reg r : (*std::next(i))->def_reg())
          if (r.is_pseudo() || allocable(r.id, r.is_float)) {
            new_nodes.push_back(r);
            if (live.find(r) == live.end())
              for (Reg o : live) {
                interfere_edge[r.get_index()].insert(o);
                interfere_edge[o.get_index()].insert(r);
              }
          }
      for (size_t idx1 = 0; idx1 < new_nodes.size(); ++idx1)
        for (size_t idx0 = 0; idx0 < idx1; ++idx0)
          if (new_nodes[idx0] != new_nodes[idx1]) {
            interfere_edge[new_nodes[idx0].get_index()].insert(new_nodes[idx1]);
            interfere_edge[new_nodes[idx1].get_index()].insert(new_nodes[idx0]);
          }
    }
  }
}

void SimpleColoringAllocator::remove(Reg r) {
  if (r.is_float) {
    assert(r.id >= FloatRegCount);
  } else {
    assert(r.id >= RegCount);
  }
  for (Reg t : interfere_edge[r.get_index()]) {
    interfere_edge[t.get_index()].erase(r);
    if (t.is_float) {
      if (interfere_edge[t.get_index()].size() ==
              ALLOCABLE_FLOAT_REGISTER_COUNT - 1 &&
          t.id >= FloatRegCount)
        simplify_nodes.push(t);
    } else {
      if (interfere_edge[t.get_index()].size() ==
              ALLOCABLE_REGISTER_COUNT - 1 &&
          t.id >= RegCount)
        simplify_nodes.push(t);
    }
  }
  interfere_edge[r.get_index()].clear();
  remain_pesudo_nodes.erase(r);
}

void SimpleColoringAllocator::simplify() {
  while (!simplify_nodes.empty()) {
    Reg cur = simplify_nodes.front();
    simplify_nodes.pop();
    vector<Reg> neighbors;
    neighbors.reserve(interfere_edge[cur.get_index()].size());
    for (Reg i : interfere_edge[cur.get_index()]) {
      neighbors.push_back(i);
    }
    remove(cur);
    simplify_history.emplace_back(cur, neighbors);
  }
}

void SimpleColoringAllocator::clear() {
  occur.clear();
  interfere_edge.clear();
  simplify_nodes = std::queue<Reg>{};
  simplify_history.clear();
  remain_pesudo_nodes.clear();
}

Reg SimpleColoringAllocator::choose_spill() {
  Reg invalid_reg = Reg(-1, 0);
  Reg spill_node = invalid_reg;
  for (Reg i : remain_pesudo_nodes)
    if (func->spilling_reg.find(i) == func->spilling_reg.end())
      if (func->constant_reg.find(i) != func->constant_reg.end() ||
          func->symbol_reg.find(i) != func->symbol_reg.end())
        if (spill_node == invalid_reg ||
            interfere_edge[i.get_index()].size() >
                interfere_edge[spill_node.get_index()].size())
          spill_node = i;
  if (spill_node == invalid_reg) {
    for (Reg i : remain_pesudo_nodes)
      if (func->spilling_reg.find(i) == func->spilling_reg.end())
        if (spill_node == invalid_reg ||
            interfere_edge[i.get_index()].size() >
                interfere_edge[spill_node.get_index()].size())
          spill_node = i;
  }
  assert(spill_node != invalid_reg);
  return spill_node;
}

vector<Reg> SimpleColoringAllocator::run(RegAllocStat *stat) {
  build_graph();
  for (int i = RegCount; i < func->reg_n; ++i) // TODO:
    if (occur[i]) {
      remain_pesudo_nodes.insert(i);
      if (interfere_edge[i].size() < ALLOCABLE_REGISTER_COUNT)
        simplify_nodes.push(i);
    }
  simplify();
  if (remain_pesudo_nodes.size()) {
    vector<Reg> spill_nodes;
    while (remain_pesudo_nodes.size()) {
      Reg cur = choose_spill();
      spill_nodes.push_back(cur);
      remove(cur);
      simplify();
    }
    stat->spill_cnt += static_cast<int>(spill_nodes.size());
    spill(spill_nodes);
    stat->succeed = false;
    return {};
  }
  stat->succeed = true;
  stat->move_eliminated = 0;
  stat->callee_save_used = 0;
  // TODO: rebuild these code
  vector<Reg> ans;
  bool used[RegCount] = {};
  ans.resize(func->reg_n);
  std::fill(ans.begin(), ans.end(), Reg{-1, 0});
  for (int i = 0; i < RegCount; ++i)
    if (occur[i]) {
      ans[i] = index_to_reg(i);
      used[i] = true;
    }
  for (size_t i = simplify_history.size() - 1; i < simplify_history.size();
       --i) {
    assert(ans[simplify_history[i].first.id].id == -1);
    bool flag[RegCount] = {};
    for (Reg neighbor : simplify_history[i].second)
      flag[ans[neighbor.id].get_index()] = true;
    for (int j = 0; j < RegCount; ++j)
      if ((REGISTER_USAGE[j] == caller_save ||
           (REGISTER_USAGE[j] == callee_save && used[j])) &&
          !flag[j]) {
        ans[simplify_history[i].first.get_index()] = j;
        break;
      }
    if (ans[simplify_history[i].first.id].id == -1)
      for (int j = 0; j < RegCount; ++j)
        if (allocable(j) && !flag[j]) {
          ans[simplify_history[i].first.id].id = j;
          break;
        }
    used[ans[simplify_history[i].first.id].id] = true;
  }
  for (int i = 0; i < RegCount; ++i)
    if (used[i] && REGISTER_USAGE[i] == callee_save)
      ++stat->callee_save_used;
  return ans;
}

} // namespace ARMv7
