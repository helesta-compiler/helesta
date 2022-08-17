#pragma once

#include <functional>
#include <memory>
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
class IRCColoringAllocator : public ColoringAllocator {
private:
  struct CoalesceEdge {
    int u, v, w;
    CoalesceEdge(int _u, int _v, int _w) : u(_u), v(_v), w(_w) {}
  };
  std::vector<bool> occur;
  std::vector<std::set<int>> interfere_edge;
  std::queue<int> simplify_worklist;
  std::vector<int> spilled_nodes;
  std::vector<std::pair<int, std::vector<int>>> select_stack;
  std::vector<int> alias;
  std::vector<std::map<int, int>> move_edges;
  std::vector<CoalesceEdge> frozen_moves;
  std::set<int> remain_pesudo_nodes;
  std::vector<int> def_cnt;
  std::vector<int> use_cnt;
  std::vector<int> depth_info;

  bool is_neighbor(int x, int y) {
    assert(x >= 0 && x < (int)interfere_edge.size());
    assert(y >= 0 && y < (int)interfere_edge.size());
    return interfere_edge[x].find(y) != interfere_edge[x].end();
  }

  void add_move_edge(int u, int v, int w) {
    move_edges[u][v] += w;
    move_edges[v][u] += w;
  }

  void for_each_node(std::function<void(int)> f) {
    for (int i = 0; i < RegConvention<type>::Count; ++i) {
      if (occur[i]) {
        f(i);
      }
    }
    for (int i : remain_pesudo_nodes) {
      f(i);
    }
  }

  void build_graph() {
    occur.resize(func->reg_n);
    interfere_edge.resize(func->reg_n);
    move_edges.resize(func->reg_n);
    alias.resize(func->reg_n, -1);
    def_cnt.resize(func->reg_n, 0);
    use_cnt.resize(func->reg_n, 0);
    depth_info.resize(func->reg_n, 0);
    std::fill(occur.begin(), occur.end(), 0);
    std::fill(interfere_edge.begin(), interfere_edge.end(), std::set<int>{});
    func->calc_live();
    std::vector<int> temp, new_nodes;
    for (auto &block : func->blocks) {
      int cur_block_depth = block->depth;
      std::set<Reg> live = block->live_out;
      for (auto it = live.begin(); it != live.end();) {
        if (it->type != type) {
          it = live.erase(it);
        } else {
          it++;
        }
      }
      temp.clear();
      for (Reg r : live) {
        if (r.type == type &&
            (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
          temp.push_back(r.id);
        }
      }
      if (block->insts.size() > 0) {
        for (Reg r : (*block->insts.rbegin())->def_reg()) {
          if (r.type == type &&
              (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
            temp.push_back(r.id);
          }
        }
      }
      for (size_t idx1 = 0; idx1 < temp.size(); ++idx1) {
        for (size_t idx0 = 0; idx0 < idx1; ++idx0) {
          if (temp[idx0] != temp[idx1]) {
            interfere_edge[temp[idx0]].insert(temp[idx1]);
            interfere_edge[temp[idx1]].insert(temp[idx0]);
          }
        }
      }
      for (auto i = block->insts.rbegin(); i != block->insts.rend(); ++i) {
        if (auto mov_instr = dynamic_cast<MoveReg *>((*i).get())) {
          if (mov_instr->src != mov_instr->dst) {
            if (mov_instr->src.type == type && mov_instr->dst.type == type) {
              if ((mov_instr->src.is_pseudo() ||
                   RegConvention<type>::allocable(mov_instr->src.id)) &&
                  (mov_instr->dst.is_pseudo() ||
                   RegConvention<type>::allocable(mov_instr->dst.id))) {
                add_move_edge(mov_instr->src.id, mov_instr->dst.id, 1);
              }
            }
          }
        }
        new_nodes.clear();
        for (Reg r : (*i)->def_reg()) {
          if (r.type == type &&
              (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
            assert(r.id >= 0);
            ++def_cnt[r.id];
            depth_info[r.id] = std::max(depth_info[r.id], cur_block_depth);
            occur[r.id] = 1;
            live.erase(r);
          }
        }
        for (Reg r : (*i)->use_reg()) {
          if (r.type == type &&
              (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
            assert(r.id >= 0);
            ++use_cnt[r.id];
            depth_info[r.id] = std::max(depth_info[r.id], cur_block_depth);
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
        }
        if (std::next(i) != block->insts.rend()) {
          for (Reg r : (*std::next(i))->def_reg()) {
            if (r.type == type &&
                (r.is_pseudo() || RegConvention<type>::allocable(r.id))) {
              new_nodes.push_back(r.id);
              if (live.find(r) == live.end()) {
                for (Reg o : live) {
                  interfere_edge[r.id].insert(o.id);
                  interfere_edge[o.id].insert(r.id);
                }
              }
            }
          }
        }
        for (size_t idx1 = 0; idx1 < new_nodes.size(); ++idx1) {
          for (size_t idx0 = 0; idx0 < idx1; ++idx0) {
            if (new_nodes[idx0] != new_nodes[idx1]) {
              interfere_edge[new_nodes[idx0]].insert(new_nodes[idx1]);
              interfere_edge[new_nodes[idx1]].insert(new_nodes[idx0]);
            }
          }
        }
      }
    }
    for (int i = 0; i < func->reg_n; ++i) {
      if (occur[i]) {
        alias[i] = i;
        if (i >= RegConvention<type>::Count) {
          remain_pesudo_nodes.insert(i);
        }
      }
    }
  }

  void remove(int id) {
    assert(id >= RegConvention<type>::Count);
    for (int i : interfere_edge[id]) {
      interfere_edge[i].erase(id);
      if (interfere_edge[i].size() ==
              RegConvention<type>::ALLOCABLE_REGISTER_COUNT - 1 &&
          i >= RegConvention<type>::Count && move_edges[i].empty())
        simplify_worklist.push(i);
    }
    interfere_edge[id].clear();
    remain_pesudo_nodes.erase(id);
  }

  bool can_simplify(int id) {
    return move_edges[id].empty() &&
           (interfere_edge[id].size() <
            RegConvention<type>::ALLOCABLE_REGISTER_COUNT);
  }

  void prepare_for_simplify() {
    for_each_node([this](int id) {
      for (int neighbor : this->interfere_edge[id]) {
        this->move_edges[id].erase(neighbor);
      }
      if (id < RegConvention<type>::Count) {
        for (int i = 0; i < RegConvention<type>::Count; ++i) {
          if (this->occur[i] && i != id) {
            this->move_edges[id].erase(i);
          }
        }
      }
    });
    for (int node : remain_pesudo_nodes) {
      if (can_simplify(node)) {
        simplify_worklist.push(node);
      }
    }
  }

  void simplify() {
    while (!simplify_worklist.empty()) {
      int cur = simplify_worklist.front();
      simplify_worklist.pop();
      std::vector<int> neighbors;
      neighbors.assign(interfere_edge[cur].begin(), interfere_edge[cur].end());
      remove(cur);
      select_stack.emplace_back(cur, neighbors);
    }
  }

  bool conservative(const std::set<int> &shared_nodes,
                    const std::set<int> &unique_nodes) {
    int sum = 0;
    for (int node : unique_nodes) {
      if (interfere_edge[node].size() >=
          RegConvention<type>::ALLOCABLE_REGISTER_COUNT) {
        ++sum;
      }
    }
    for (int node : shared_nodes) {
      if (interfere_edge[node].size() >
          RegConvention<type>::ALLOCABLE_REGISTER_COUNT) {
        ++sum;
      }
    }
    return sum < RegConvention<type>::ALLOCABLE_REGISTER_COUNT;
  }

  void combine(int u, int v) {
    assert(u >= RegConvention<type>::Count || v >= RegConvention<type>::Count);
    if (v < RegConvention<type>::Count) {
      std::swap(u, v);
    }
    remain_pesudo_nodes.erase(v);
    alias[v] = u;
    for (int v_neighbor : interfere_edge[v]) {
      interfere_edge[v_neighbor].erase(v);
      interfere_edge[v_neighbor].insert(u);
      interfere_edge[u].insert(v_neighbor);
    }
    interfere_edge[v].clear();
    for (auto &v_move_neighbor : move_edges[v]) {
      move_edges[v_move_neighbor.first].erase(v);
      if (v_move_neighbor.first == u) {
        continue;
      }
      add_move_edge(v_move_neighbor.first, u, v_move_neighbor.second);
    }
    move_edges[v].clear();
  }

  int get_alias(int x) {
    return alias[x] == x ? x : alias[x] = get_alias(alias[x]);
  }

  int coalesce() {
    std::vector<CoalesceEdge> worklist_moves;
    for_each_node([&](int id) {
      for (auto &move_neighbor : this->move_edges[id]) {
        if (move_neighbor.first < id) {
          worklist_moves.emplace_back(id, move_neighbor.first,
                                      move_neighbor.second);
        }
      }
    });
    std::sort(
        worklist_moves.begin(), worklist_moves.end(),
        [](const CoalesceEdge &x, const CoalesceEdge &y) { return x.w > y.w; });
    int coalesced_degree = 0;
    for (auto &cur_move : worklist_moves) {
      int u = get_alias(cur_move.u);
      int v = get_alias(cur_move.v);
      if (u == v) {
        continue;
      }
      if (u < RegConvention<type>::Count && v < RegConvention<type>::Count) {
        continue;
      }
      if (!is_neighbor(u, v)) {
        std::set<int> shared_adjacent;
        std::set<int> unique_adjacent;
        for (int i : interfere_edge[u]) {
          if (interfere_edge[v].count(i)) {
            shared_adjacent.insert(i);
          } else {
            unique_adjacent.insert(i);
          }
        }
        for (int i : interfere_edge[v]) {
          if (interfere_edge[u].count(i) == 0) {
            unique_adjacent.insert(i);
          }
        }
        if (conservative(shared_adjacent, unique_adjacent)) {
          combine(u, v);
          coalesced_degree += cur_move.w;
        }
      }
    }
    return coalesced_degree;
  }

  bool freeze() {
    int selected_freeze = -1;
    for (int i : remain_pesudo_nodes) {
      if (move_edges[i].empty()) {
        continue;
      }
      if (interfere_edge[i].size() <
          RegConvention<type>::ALLOCABLE_REGISTER_COUNT) {
        if (selected_freeze < 0) {
          selected_freeze = i;
          continue;
        }
        if (interfere_edge[i].size() > interfere_edge[selected_freeze].size()) {
          selected_freeze = i;
          if (interfere_edge[i].size() ==
              RegConvention<type>::ALLOCABLE_REGISTER_COUNT - 1) {
            break;
          }
        }
      }
    }
    if (selected_freeze < 0) {
      return false;
    }
    for (auto &move_neighbor : move_edges[selected_freeze]) {
      frozen_moves.emplace_back(selected_freeze, move_neighbor.first,
                                move_neighbor.second);
      move_edges[move_neighbor.first].erase(selected_freeze);
    }
    move_edges[selected_freeze].clear();
    return true;
  }

  int get_exp(int m, int n) { // m^n
    if (n == 0) {
      return 1;
    }
    if (n == 1) {
      return m;
    }
    if (n % 2 == 0) {
      return get_exp(m * m, n / 2);
    } else {
      return m * get_exp(m * m, n / 2);
    }
  }

  int select_spill() {
    int selected_spill = -1;
    // TODO: get a better policy to select the node
    int optimal_depth = 0;
    double optimal_weight = 0;
    auto less_than_optimal = [&](int x, double y) {
      // cmp y*4^x  choose minimal
      if (x == optimal_depth) {
        return y < optimal_weight;
      } else if (x > optimal_depth) {
        if (x - optimal_depth > 12) {
          return false;
        }
        return get_exp(4, x - optimal_depth) < (optimal_weight / y);
      } else {
        if (optimal_depth - x > 12) {
          return true;
        }
        return get_exp(4, optimal_depth - x) > (y / optimal_weight);
      }
    };
    for (int i : remain_pesudo_nodes) {
      if (func->spilling_reg.find(Reg(i, type)) == func->spilling_reg.end()) {
        int cur_depth = depth_info[i];
        double cur_weight;
        if (func->constant_reg.find(Reg(i, type)) != func->constant_reg.end() ||
            func->symbol_reg.find(Reg(i, type)) != func->symbol_reg.end()) {
          cur_weight = use_cnt[i] * 100.0 / interfere_edge[i].size();
          if (selected_spill == -1 ||
              less_than_optimal(cur_depth, cur_weight)) {
            selected_spill = i;
            optimal_depth = cur_depth;
            optimal_weight = cur_weight;
          }
        }
      }
    }
    if (selected_spill == -1) {
      for (int i : remain_pesudo_nodes) {
        if (func->spilling_reg.find(Reg(i, type)) == func->spilling_reg.end()) {
          int cur_depth = depth_info[i];
          double cur_weight =
              (use_cnt[i] + def_cnt[i]) * 100.0 / interfere_edge[i].size();
          if (selected_spill == -1 ||
              less_than_optimal(cur_depth, cur_weight)) {
            selected_spill = i;
            optimal_depth = cur_depth;
            optimal_weight = cur_weight;
          }
        }
      }
    }
    assert(selected_spill != -1);
    remain_pesudo_nodes.erase(selected_spill);
    for (int neighbor : interfere_edge[selected_spill]) {
      interfere_edge[neighbor].erase(selected_spill);
    }
    interfere_edge[selected_spill].clear();
    for (auto &move_neighbor : move_edges[selected_spill]) {
      move_edges[move_neighbor.first].erase(selected_spill);
    }
    move_edges[selected_spill].clear();
    return selected_spill;
  }

  void rewrite_function() {
    std::vector<StackObject *> spill_obj;
    std::set<int> constant_spilled;
    for (size_t i = 0; i < spilled_nodes.size(); ++i) {
      if (func->constant_reg.find(Reg(spilled_nodes[i], type)) !=
              func->constant_reg.end() ||
          func->symbol_reg.find(Reg(spilled_nodes[i], type)) !=
              func->symbol_reg.end()) {
        constant_spilled.insert(spilled_nodes[i]);
        spill_obj.push_back(nullptr);
      } else {
        StackObject *t = new StackObject();
        t->size = INT_SIZE;
        t->position = -1;
        func->stack_objects.emplace_back(t);
        spill_obj.push_back(t);
      }
    }
    for (auto &block : func->blocks) {
      for (auto it = block->insts.begin(); it != block->insts.end();) {
        bool need_continue = false;
        for (Reg r : (*it)->def_reg())
          if (constant_spilled.find(r.id) != constant_spilled.end()) {
            it = block->insts.erase(it);
            need_continue = true;
            break;
          }
        if (need_continue) {
          continue;
        }
        for (size_t j = 0; j < spilled_nodes.size(); ++j) {
          int id = spilled_nodes[j];
          bool cur_def = (*it)->def(Reg(id, type));
          bool cur_use = (*it)->use(Reg(id, type));
          if (func->constant_reg.find(Reg(id, type)) !=
              func->constant_reg.end()) {
            assert(type == ScalarType::Int);
            assert(!cur_def);
            if (cur_use) {
              Reg tmp{func->reg_n++, type};
              func->spilling_reg.insert(tmp);
              func->constant_reg[tmp] = func->constant_reg[Reg(id, type)];
              insert(block->insts, it,
                     load_imm(tmp, func->constant_reg[Reg(id, type)]));
              (*it)->replace_reg(Reg(id, type), tmp);
            }
          } else if (func->symbol_reg.find(Reg(id, type)) !=
                     func->symbol_reg.end()) {
            assert(!cur_def);
            if (cur_use) {
              Reg tmp{func->reg_n++, type};
              func->spilling_reg.insert(tmp);
              func->symbol_reg[tmp] = func->symbol_reg[Reg(id, type)];
              insert(block->insts, it,
                     load_symbol_addr(tmp, func->symbol_reg[Reg(id, type)]));
              (*it)->replace_reg(Reg(id, type), tmp);
            }
          } else {
            if (cur_def || cur_use) {
              StackObject *cur_obj = spill_obj[j];
              Reg tmp{func->reg_n++, type};
              if (type == ScalarType::Float)
                func->float_regs.insert(tmp);
              func->spilling_reg.insert(tmp);
              if (cur_use)
                block->insts.insert(
                    it, std::make_unique<LoadStack>(tmp, 0, cur_obj));
              if (cur_def)
                block->insts.insert(std::next(it), std::make_unique<StoreStack>(
                                                       tmp, 0, cur_obj));
              (*it)->replace_reg(Reg(id, type), tmp);
            }
          }
        }
        ++it;
      }
    }
    spilled_nodes.clear();
  }

  std::vector<int> assign_colors(RegAllocStat *stat) {
    std::vector<int> ans(func->reg_n, -1);
    std::vector<std::map<int, int>> frozen(func->reg_n, std::map<int, int>{});
    bool used[RegConvention<type>::Count] = {};
    for (int i = 0; i < RegConvention<type>::Count; ++i) {
      if (occur[i]) {
        ans[i] = i;
        used[i] = true;
      }
    }
    for (auto &i : frozen_moves) {
      int u = get_alias(i.u);
      int v = get_alias(i.v);
      if (u != v) {
        frozen[u][v] += i.w;
        frozen[v][u] += i.w;
      }
    }
    for (int i = ((int)select_stack.size()) - 1; i >= 0; --i) {
      auto &cur_node = select_stack[i];
      assert(ans[cur_node.first] == -1);
      assert(alias[cur_node.first] == cur_node.first);
      bool flag[RegConvention<type>::Count] = {};
      for (int neighbor : cur_node.second) {
        flag[ans[get_alias(neighbor)]] = true;
      }
      int score[RegConvention<type>::Count] = {};
      for (int j = 0; j < RegConvention<type>::Count; ++j) {
        if (RegConvention<type>::REGISTER_USAGE[j] ==
                RegisterUsage::caller_save ||
            (RegConvention<type>::REGISTER_USAGE[j] ==
                 RegisterUsage::callee_save &&
             used[j])) {
          score[j] = 1;
        }
      }
      for (auto &move_neighbor : frozen[cur_node.first]) {
        int v = get_alias(move_neighbor.first);
        if (ans[v] != -1) {
          score[ans[v]] += move_neighbor.second;
        }
      }
      int cur_ans = -1;
      for (int j = 0; j < RegConvention<type>::Count; ++j) {
        if (RegConvention<type>::allocable(j) && !flag[j]) {
          if (cur_ans == -1 || score[j] > score[cur_ans]) {
            cur_ans = j;
          }
        }
      }
      assert(cur_ans >= 0);
      if (score[cur_ans] > 0) {
        stat->move_eliminated += score[cur_ans] - 1;
      }
      used[cur_ans] = true;
      ans[cur_node.first] = cur_ans;
    }
    for (int i = RegConvention<type>::Count; i < func->reg_n; ++i) {
      if (occur[i]) {
        ans[i] = ans[get_alias(i)];
      }
    }
    stat->callee_save_used = 0;
    for (int i = 0; i < RegConvention<type>::Count; ++i) {
      if (used[i] &&
          RegConvention<type>::REGISTER_USAGE[i] == RegisterUsage::callee_save)
        ++stat->callee_save_used;
    }
    return ans;
  }

public:
  IRCColoringAllocator(Func *_func) : ColoringAllocator(_func) {}
  virtual void clear() override {
    occur.clear();
    interfere_edge.clear();
    simplify_worklist = std::queue<int>{};
    spilled_nodes.clear();
    select_stack.clear();
    alias.clear();
    move_edges.clear();
    frozen_moves.clear();
    remain_pesudo_nodes.clear();
    def_cnt.clear();
    use_cnt.clear();
    depth_info.clear();
  }
  virtual std::vector<int> run(RegAllocStat *stat) override {
    build_graph();
    stat->succeed = true;
    stat->move_eliminated = 0;
    spilled_nodes.clear();
    while (!remain_pesudo_nodes.empty()) {
      prepare_for_simplify();
      simplify();
      if (remain_pesudo_nodes.empty()) {
        break;
      }
      int coalesced_degree = coalesce();
      if (coalesced_degree > 0) {
        stat->move_eliminated += coalesced_degree;
        continue;
      }
      if (freeze()) {
        continue;
      }
      stat->succeed = false;
      ++stat->spill_cnt;
      spilled_nodes.push_back(select_spill());
    }
    if (!stat->succeed) {
      rewrite_function();
      return {};
    }
    assert(spilled_nodes.empty());
    return assign_colors(stat);
  }
};

} // namespace ARMv7
