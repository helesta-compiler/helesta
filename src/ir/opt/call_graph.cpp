#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"
using namespace IR;
std::map<Reg, RegWriteInstr *> build_defs(NormalFunc *func) {
  std::map<Reg, RegWriteInstr *> f;
  func->for_each([&](Instr *x) {
    Case(RegWriteInstr, w, x) { f[w->d1] = w; }
  });
  return f;
}

std::map<BB *, std::vector<BB *>> build_prev(NormalFunc *func) {
  std::map<BB *, std::vector<BB *>> f;
  func->for_each([&](BB *bb) {
    for (BB *w : bb->getOutNodes())
      f[w].push_back(bb);
  });
  return f;
}

struct CallGraph {
  struct Info {
    bool visited = 0, no_store = 1, no_load = 1, ret_used = 0;
    std::vector<CallInstr *> calls;
    std::vector<std::pair<NormalFunc *, CallInstr *>> called;
    std::map<Reg, RegWriteInstr *> defs;
  };
  std::map<NormalFunc *, Info> info;
  std::pair<bool, bool> isPure(NormalFunc *f) {
    auto &fi = info[f];
    if (fi.visited)
      return {fi.no_store, fi.no_load};
    fi.visited = 1;
    for (auto call : fi.calls) {
      Case(NormalFunc, f0, call->f) {
        if (f0 != f) {
          auto [p1, p2] = isPure(f0);
          fi.no_store &= call->no_store = p1;
          fi.no_load &= call->no_load = p2;
        }
      }
      else {
        fi.no_store = 0;
        fi.no_load = 0;
      }
    }
    for (auto call : fi.calls) {
      Case(NormalFunc, f0, call->f) {
        if (f0 == f) {
          call->no_store = fi.no_store;
          call->no_load = fi.no_load;
        }
      }
    }
    // std::cerr << f->name << "  pure?  " << fi.no_store << fi.no_load << '\n';
    return {fi.no_store, fi.no_load};
  }
  CompileUnit *ir;
  CallGraph(CompileUnit *_ir) : ir(_ir) {
    ir->for_each([&](NormalFunc *f) {
      auto &fi = info[f];
      f->for_each([&](Instr *x) {
        Case(LoadInstr, ld, x) {
          // std::cerr << f->name << "  load:  " << *ld << '\n';
          fi.no_load = 0;
          (void)ld;
        }
        else Case(StoreInstr, st, x) {
          fi.no_store = 0;
          (void)st;
        }
        else Case(CallInstr, call, x) {
          fi.calls.push_back(call);
        }
      });
    });
    ir->for_each([&](NormalFunc *f) {
      auto &fi = info[f];
      fi.defs = build_defs(f);
      for (auto call : fi.calls) {
        Case(NormalFunc, f0, call->f) { info[f0].called.emplace_back(f, call); }
      }
    });
  }
  void build_pure() {
    ir->for_each([&](NormalFunc *f) { isPure(f); });
  }
  void const_prop() {
    size_t cnt = 0;
    ir->for_each([&](NormalFunc *f) {
      auto &fi = info[f];
      if (fi.called.size() == 1) {
        auto [f0, call] = fi.called[0];
        auto &defs = info[f0].defs;
        std::vector<std::pair<int32_t, int>> args(call->args.size(), {0, 0});
        std::map<Reg, int> same_arg;
        size_t used_arg = 0;
        bool flag = 0;
        for (auto [r, id] : enumerate(call->args)) {
          auto rw = defs.at(r);
          Case(LoadConst<int32_t>, lc, rw) {
            flag = 1;
            args[id] = {lc->value, 1};
          }
          else if (same_arg.count(r)) {
            flag = 1;
            args[id] = {same_arg[r], 2};
          }
          else {
            same_arg[r] = id;
            args[id] = {id, 0};
            used_arg = id + 1;
          }
        }
        if (!flag)
          return;
        call->args.resize(used_arg);
        f->for_each([&](BB *bb) {
          bb->for_each([&](Instr *x) {
            Case(LoadArg, la, x) {
              auto [v, type] = args.at(la->id);
              if (type == 2) {
                la->id = v;
                ++cnt;
              } else if (type == 1) {
                Reg d1 = la->d1;
                auto lc = new LoadConst<int32_t>(d1, v);
                bb->replace(lc);
                info[f].defs.at(d1) = lc;
                ++cnt;
              }
            }
          });
        });
      }
    });
    if (cnt) {
      ::info << "CallGraph.constProp: " << cnt << '\n';
    }
  }
  void remove_unused_ret() {
    size_t cnt = 0;
    info[ir->main()].ret_used = 1;
    for (auto f : reverse_view(ir->_funcs)) {
      auto use = build_use_count(f);
      auto &fi = info[f];
      for (auto call : fi.calls) {
        Case(NormalFunc, f0, call->f) {
          if (use[call->d1])
            info[f0].ret_used = 1;
        }
      }
      if (!fi.ret_used) {
        bool flag = 0;
        f->for_each([&](BB *bb) {
          Case(ReturnInstr, ret, bb->back()) {
            Case(LoadConst<int32_t>, _, fi.defs.at(ret->s1)) {
              (void)_;
              return;
            }
            Reg r = ret->s1 = f->new_Reg();
            auto lc = new LoadConst(r, 0);
            bb->push1(lc);
            fi.defs[r] = lc;
            flag = 1;
          }
        });
        if (flag) {
          remove_unused_def_func(f);
          ++cnt;
        }
      }
    }
    if (cnt) {
      ::info << "CallGraph.remove_unused_ret: " << cnt << '\n';
    }
  }
  void tail_rec_to_loop(NormalFunc *f) {
    size_t cnt = 0;
    BB *bb = nullptr;
    f->for_each([&](BB *bb0) {
      Case(ReturnInstr, _, bb0->back()) {
        (void)_;
        assert(!bb);
        bb = bb0;
      }
    });
    assert(bb);
    if (bb->instrs.size() != 2)
      return;
    Case(PhiInstr, phi, bb->back1()) {
      auto prev = build_prev(f);
      if (phi->uses.size() != prev[bb].size())
        return;
      std::set<BB *> tail_rec;
      for (auto &[r, bb0] : phi->uses) {
        if (bb0->instrs.size() < 2)
          continue;
        Case(JumpInstr, jmp, bb0->back()) {
          assert(jmp->target == bb);
          Case(CallInstr, call, bb0->back1()) {
            if (call->f == f && call->d1 == r) {
              tail_rec.insert(bb0);
            }
          }
        }
      }
      if (tail_rec.size()) {
        ++cnt;
        std::unordered_map<Reg, Reg> mp;
        std::map<int, std::pair<Reg, Reg>> args;
        f->for_each([&](Instr *x) {
          Case(LoadArg, la, x) {
            if (!args.count(la->id)) {
              Reg r1 = f->new_Reg();
              Reg r2 = f->new_Reg();
              args[la->id] = {r1, r2};
            }
            mp[la->d1] = args[la->id].second;
          }
        });
        auto new_entry = f->new_BB();
        auto old_entry = f->entry;
        f->entry = new_entry;
        for (auto &[id, rs] : args) {
          new_entry->push(new LoadArg(rs.first, id));
        }
        new_entry->push(new JumpInstr(old_entry));
        for (auto &[id, rs] : args) {
          auto phi = new PhiInstr(rs.second);
          old_entry->push_front(phi);
          phi->add_use(rs.first, new_entry);
          for (BB *bb0 : tail_rec) {
            Case(CallInstr, call, bb0->back1()) {
              phi->add_use(call->args.at(id), bb0);
            }
            else assert(0);
          }
        }
        for (BB *bb0 : tail_rec) {
          bb0->pop();
          bb0->pop();
          bb0->push(new JumpInstr(old_entry));
        }
        f->for_each([&](Instr *x) { x->map_use(partial_map(mp)); });
        remove_if_vec(phi->uses, [&](const std::pair<Reg, BB *> &w) {
          return tail_rec.count(w.second);
        });
        map_use(f, mp);
      }
    }
    if (cnt) {
      ::info << "CallGraph.tail_rec_to_loop: " << cnt << '\n';
    }
  }
  void tail_rec_to_loop() {
    ir->for_each([&](NormalFunc *f) { tail_rec_to_loop(f); });
  }
};

void call_graph(CompileUnit *ir) {
  PassDisabled("cg") return;
  CallGraph cg(ir);
  cg.const_prop();
  cg.build_pure();
  cg.remove_unused_ret();
  cg.tail_rec_to_loop();
}
