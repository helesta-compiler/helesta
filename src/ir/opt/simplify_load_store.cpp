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
  CallGraph(CompileUnit *ir) {
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
    ir->for_each([&](NormalFunc *f) { isPure(f); });
  }
};

void inference_pure_call(CompileUnit *ir) { CallGraph cg(ir); }
/*
struct SimplifyLoadStore {
  NormalFunc *func;
  std::map<Reg, RegWriteInstr *> defs;
  std::map<BB *, std::vector<BB *>> prev;
  size_t cnt = 0, dse_cnt = 0;

  struct LoadStoreCache {
    bool visited = 0;
    std::map<Reg, Reg> lives;
  };
  std::map<BB *, LoadStoreCache> bb_cache;

  LoadStoreCache &getLoadStoreCache(BB *bb) {
    auto &w = bb_cache[bb];
    if (w.visited)
      return w;
    w.visited = 1;
    if (prev[bb].size() == 1) {
      BB *fa = prev[bb][0];
      auto &w0 = getLoadStoreCache(fa);
      w.lives = w0.lives;
    }
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      Case(LoadInstr, ld, x) {
        if (w.lives.count(ld->addr)) {
          *it = std::make_unique<UnaryOpInstr>(ld->d1, w.lives[ld->addr],
                                               UnaryCompute::ID);
          ++cnt;
        } else {
          w.lives[ld->addr] = ld->d1;
        }
      }
      else Case(StoreInstr, st, x) {
        w.lives.clear();
        w.lives[st->addr] = st->s1;
      }
      else Case(CallInstr, _, x) {
        (void)_;
        w.lives.clear();
      }
    }

    return w;
  }

  void dse(BB *bb) {
    std::set<Reg> stores;
    for (auto it = bb->instrs.end(); it != bb->instrs.begin();) {
      auto it0 = it;
      --it;
      Instr *x = it->get();
      Case(LoadInstr, _, x) {
        stores.clear();
        (void)_;
      }
      else Case(StoreInstr, st, x) {
        if (!stores.insert(st->addr).second) {
          ++dse_cnt;
          bb->instrs.erase(it);
          it = it0;
        }
      }
      else Case(CallInstr, _, x) {
        stores.clear();
        (void)_;
      }
    }
  }

  SimplifyLoadStore(NormalFunc *_func) : func(_func) {
    if (global_config.disabled_passes.count("sls"))
      return;
    defs = build_defs(func);
    prev = build_prev(func);
    func->for_each([&](BB *bb) { getLoadStoreCache(bb); });
    func->for_each([&](BB *bb) { dse(bb); });
    if (cnt) {
      info << "SimplifyLoadStore: " << cnt << " in " << func->name << '\n';
    }
    if (dse_cnt) {
      info << "dse: " << dse_cnt << " in " << func->name << '\n';
    }
  }
};

void simplify_load_store_func(NormalFunc *func) { SimplifyLoadStore _(func); }

void simplify_load_store(CompileUnit *ir) {
  ir->for_each(simplify_load_store_func);
}
*/
