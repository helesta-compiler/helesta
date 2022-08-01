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
    bool visited = 0, no_store = 1, no_load = 1;
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
  void buildPure() {
    ir->for_each([&](NormalFunc *f) { isPure(f); });
  }
  void constProp() {
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
          for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
            auto x = it->get();
            Case(LoadArg, la, x) {
              auto [v, type] = args.at(la->id);
              if (type == 2) {
                la->id = v;
                ++cnt;
              } else if (type == 1) {
                Reg d1 = la->d1;
                auto lc = new LoadConst<int32_t>(d1, v);
                *it = std::unique_ptr<Instr>(lc);
                info[f].defs.at(d1) = lc;
                ++cnt;
              }
            }
          }
        });
      }
    });
    if (cnt) {
      ::info << "CallGraph.constProp: " << cnt << '\n';
    }
  }
};

void call_graph(CompileUnit *ir) {
  CallGraph cg(ir);
  cg.constProp();
  cg.buildPure();
}
