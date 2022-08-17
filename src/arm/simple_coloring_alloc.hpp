#pragma once

#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "arm/archinfo.hpp"
#include "arm/func.hpp"
#include "arm/inst.hpp"
#include "arm/program.hpp"
#include "arm/regalloc.hpp"
#include "common/common.hpp"

namespace ARMv7 {

template <ScalarType type>
class SimpleColoringAllocator : public ColoringAllocator {
  std::vector<bool> occur;
  std::vector<std::set<int>> interfere_edge;
  std::queue<int> simplify_nodes;
  std::vector<std::pair<int, std::vector<int>>> simplify_history;
  std::set<int> remain_pesudo_nodes;

  void build_graph() {
    occur.resize(func->reg_n);
    interfere_edge.resize(func->reg_n);
    std::fill(occur.begin(), occur.end(), 0);
    std::fill(interfere_edge.begin(), interfere_edge.end(), std::set<int>{});
    func->calc_live();
    std::vector<int> temp, new_nodes;
    for (auto &block : func->blocks) {
      std::set<Reg> live = block->live_out;
      for (auto it = live.begin(); it != live.end();) {
        if (it->type != type)
          it = live.erase(it);
        else
          it++;
      }
      temp.clear();
      for (Reg r : live)
        if (r.type == type &&
            (r.is_pseudo() || RegConvention<type>::allocable(r.id)))
          temp.push_back(r.id);
      if (block->insts.size() > 0)
        for (Reg r : (*block->insts.rbegin())->def_reg())
          if (r.type == type &&
              (r.is_pseudo() || RegConvention<type>::allocable(r.id)))
            temp.push_back(r.id);
      for (size_t idx1 = 0; idx1 < temp.size(); ++idx1)
        for (size_t idx0 = 0; idx0 < idx1; ++idx0)
          if (temp[idx0] != temp[idx1]) {
            interfere_edge[temp[idx0]].insert(temp[idx1]);
            interfere_edge[temp[idx1]].insert(temp[idx0]);
          }
      for (auto i = block->insts.rbegin(); i != block->insts.rend(); ++i) {
        new_nodes.clear();
        for (Reg r : (*i)->def_reg())
          if (r.type == type &&
              (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
            assert(r.id >= 0);
            occur[r.id] = 1;
            live.erase(r);
          }
        for (Reg r : (*i)->use_reg())
          if (r.type == type &&
              (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
            assert(r.id >= 0);
            occur[r.id] = 1;
            if (live.find(r) == live.end()) {
              for (Reg o : live) {
                interfere_edge[r.id].insert(o.id);
                interfere_edge[o.id].insert(r.id);
              }
              live.insert(r);
            }
            new_nodes.push_back(r.id);
          }
        if (std::next(i) != block->insts.rend())
          for (Reg r : (*std::next(i))->def_reg())
            if (r.type == type &&
                (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
              new_nodes.push_back(r.id);
              if (live.find(r) == live.end())
                for (Reg o : live) {
                  interfere_edge[r.id].insert(o.id);
                  interfere_edge[o.id].insert(r.id);
                }
            }
        for (size_t idx1 = 0; idx1 < new_nodes.size(); ++idx1)
          for (size_t idx0 = 0; idx0 < idx1; ++idx0)
            if (new_nodes[idx0] != new_nodes[idx1]) {
              interfere_edge[new_nodes[idx0]].insert(new_nodes[idx1]);
              interfere_edge[new_nodes[idx1]].insert(new_nodes[idx0]);
            }
      }
    }
  }
  void spill(const std::vector<int> &spill_nodes) {
    std::vector<StackObject *> spill_obj;
    std::set<int> constant_spilled;
    for (size_t i = 0; i < spill_nodes.size(); ++i)
      if (func->constant_reg.find(Reg(spill_nodes[i], type)) !=
              func->constant_reg.end() ||
          func->symbol_reg.find(Reg(spill_nodes[i], type)) !=
              func->symbol_reg.end()) {
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
          if (constant_spilled.find(r.id) != constant_spilled.end()) {
            auto nxt = std::next(i);
            block->insts.erase(i);
            i = nxt;
            need_continue = true;
            break;
          }
        if (need_continue)
          continue;
        for (size_t j = 0; j < spill_nodes.size(); ++j) {
          int id = spill_nodes[j];
          bool cur_def = (*i)->def(Reg(id, type)),
               cur_use = (*i)->use(Reg(id, type));
          if (func->constant_reg.find(Reg(id, type)) !=
              func->constant_reg.end()) {
            assert(type == ScalarType::Int);
            assert(!cur_def);
            if (cur_use) {
              Reg tmp{func->reg_n++, type};
              assert(tmp.type == type);
              func->spilling_reg.insert(tmp);
              func->constant_reg[tmp] = func->constant_reg[Reg(id, type)];
              insert(block->insts, i,
                     load_imm(tmp, func->constant_reg[Reg(id, type)]));
              (*i)->replace_reg(Reg(id, type), tmp);
            }
          } else if (func->symbol_reg.find(Reg(id, type)) !=
                     func->symbol_reg.end()) {
            assert(!cur_def);
            if (cur_use) {
              Reg tmp{func->reg_n++, type};
              assert(tmp.type == type);
              func->spilling_reg.insert(tmp);
              func->symbol_reg[tmp] = func->symbol_reg[Reg(id, type)];
              insert(block->insts, i,
                     load_symbol_addr(tmp, func->symbol_reg[Reg(id, type)]));
              (*i)->replace_reg(Reg(id, type), tmp);
            }
          } else {
            if (cur_def || cur_use) {
              StackObject *cur_obj = spill_obj[j];
              Reg tmp{func->reg_n++, type};
              if (type == ScalarType::Float)
                func->float_regs.insert(tmp);
              assert(tmp.type == type);
              func->spilling_reg.insert(tmp);
              if (cur_use)
                block->insts.insert(
                    i, std::make_unique<LoadStack>(tmp, 0, cur_obj));
              if (cur_def)
                block->insts.insert(std::next(i), std::make_unique<StoreStack>(
                                                      tmp, 0, cur_obj));
              (*i)->replace_reg(Reg(id, type), tmp);
            }
          }
        }
        ++i;
      }
  }
  void remove(int id) {
    assert(id >= RegConvention<type>::Count);
    for (int i : interfere_edge[id]) {
      interfere_edge[i].erase(id);
      if (interfere_edge[i].size() ==
              RegConvention<type>::ALLOCABLE_REGISTER_COUNT - 1 &&
          i >= RegConvention<type>::Count)
        simplify_nodes.push(i);
    }
    interfere_edge[id].clear();
    remain_pesudo_nodes.erase(id);
  }
  void simplify() {
    while (!simplify_nodes.empty()) {
      int cur = simplify_nodes.front();
      simplify_nodes.pop();
      std::vector<int> neighbors;
      neighbors.reserve(interfere_edge[cur].size());
      for (int i : interfere_edge[cur]) {
        neighbors.push_back(i);
      }
      remove(cur);
      simplify_history.emplace_back(cur, neighbors);
    }
  }

  int choose_spill() {
    int spill_node = -1;
    for (int i : remain_pesudo_nodes)
      if (func->spilling_reg.find(Reg(i, type)) == func->spilling_reg.end())
        if (func->constant_reg.find(Reg(i, type)) != func->constant_reg.end() ||
            func->symbol_reg.find(Reg(i, type)) != func->symbol_reg.end())
          if (spill_node == -1 ||
              interfere_edge[i].size() > interfere_edge[spill_node].size())
            spill_node = i;
    if (spill_node == -1) {
      for (int i : remain_pesudo_nodes)
        if (func->spilling_reg.find(Reg(i, type)) == func->spilling_reg.end())
          if (spill_node == -1 ||
              interfere_edge[i].size() > interfere_edge[spill_node].size())
            spill_node = i;
    }
    assert(spill_node != -1);
    return spill_node;
  }

public:
  SimpleColoringAllocator(Func *_func) : ColoringAllocator(_func) {}
  virtual void clear() override {
    occur.clear();
    interfere_edge.clear();
    simplify_nodes = std::queue<int>{};
    simplify_history.clear();
    remain_pesudo_nodes.clear();
  }
  virtual std::vector<int> run(RegAllocStat *stat) override {
    build_graph();
    for (int i = RegConvention<type>::Count; i < func->reg_n; ++i)
      if (occur[i]) {
        remain_pesudo_nodes.insert(i);
        if (interfere_edge[i].size() <
            RegConvention<type>::ALLOCABLE_REGISTER_COUNT)
          simplify_nodes.push(i);
      }
    simplify();
    if (remain_pesudo_nodes.size()) {
      std::vector<int> spill_nodes;
      while (remain_pesudo_nodes.size()) {
        int cur = choose_spill();
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
    std::vector<int> ans;
    bool used[RegConvention<type>::Count] = {};
    ans.resize(func->reg_n);
    std::fill(ans.begin(), ans.end(), -1);
    for (int i = 0; i < RegConvention<type>::Count; ++i)
      if (occur[i]) {
        ans[i] = i;
        used[i] = true;
      }
    for (size_t i = simplify_history.size() - 1; i < simplify_history.size();
         --i) {
      assert(ans[simplify_history[i].first] == -1);
      bool flag[RegConvention<type>::Count] = {};
      for (int neighbor : simplify_history[i].second)
        flag[ans[neighbor]] = true;
      for (int j = 0; j < RegConvention<type>::Count; ++j)
        if ((RegConvention<type>::REGISTER_USAGE[j] ==
                 RegisterUsage::caller_save ||
             (RegConvention<type>::REGISTER_USAGE[j] ==
                  RegisterUsage::callee_save &&
              used[j])) &&
            !flag[j]) {
          ans[simplify_history[i].first] = j;
          break;
        }
      if (ans[simplify_history[i].first] == -1)
        for (int j = 0; j < RegConvention<type>::Count; ++j)
          if (RegConvention<type>::allocable(j) && !flag[j]) {
            ans[simplify_history[i].first] = j;
            break;
          }
      used[ans[simplify_history[i].first]] = true;
    }
    for (int i = 0; i < RegConvention<type>::Count; ++i)
      if (used[i] &&
          RegConvention<type>::REGISTER_USAGE[i] == RegisterUsage::callee_save)
        ++stat->callee_save_used;
    return ans;
  }
};

} // namespace ARMv7
