#include "ir/opt/dag_ir.hpp"

struct CodeReorder : SimpleLoopVisitor {
  std::vector<std::unique_ptr<BB>> bbs;
  void visitBB(BB *bb) { bbs.emplace_back(bb); }
  void apply(NormalFunc *f) {
    for (auto &x : f->bbs)
      (void)x.release();
    f->bbs = std::move(bbs);
  }
};

void remove_branch(BB *bb, bool cond) {
  Case(BranchInstr, br, bb->back()) {
    auto target = cond ? br->target1 : br->target0;
    auto bb0 = cond ? br->target0 : br->target1;
    bb->pop();
    bb->push(new JumpInstr(target));
    bb0->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        remove_if_vec(phi->uses,
                      [bb](auto &w) -> bool { return w.second == bb; });
      }
    });
  }
  else assert(0);
}

struct CondProp : ForwardLoopVisitor<std::map<std::pair<Reg, BB *>, int32_t>>,
                  CounterOutput {
  using ForwardLoopVisitor::map_t;
  CondProp() : CounterOutput("CondProp") {}
  void visitBB(BB *bb) {
    auto &w = info[bb];
    w.out.clear();
    for (auto [k, v] : w.in) {
      if (k.second == nullptr || k.second == bb) {
        w.out[{k.first, nullptr}] = v;
      }
    }
    Case(BranchInstr, br, bb->back()) {
      if (w.out.count({br->cond, nullptr})) {
        auto v = w.out[{br->cond, nullptr}];
        remove_branch(bb, v);
        ++cnt;
      } else {
        w.out[{br->cond, br->target1}] = 1;
        w.out[{br->cond, br->target0}] = 0;
      }
    }
  }
};

void simplify_branch(NormalFunc *f) {
  DAG_IR dag(f);
  CondProp cp;
  dag.visit(cp);

  size_t cnt = 0;
  auto defs = build_defs(f);
  f->for_each([&](BB *bb) {
    Case(BranchInstr, br, bb->back()) {
      Case(LoadConst<int32_t>, lc, defs.at(br->cond)) {
        remove_branch(bb, lc->value);
        ++cnt;
      }
    }
  });
  if (cnt) {
    ::info << "simplify_branch: " << cnt << '\n';
  }
}
void code_reorder(NormalFunc *f) {
  DAG_IR dag(f);
  CodeReorder w;
  dag.visit(w);
  w.apply(f);
}
void remove_trivial_BB(NormalFunc *f) {
  auto prev = build_prev(f);
  for (auto &_bb : reverse_view(f->bbs)) {
    BB *bb = _bb.get();
    if (prev[bb].size() != 1)
      continue;
    BB *bb0 = prev[bb][0];
    Case(JumpInstr, _, bb0->back()) { (void)_; }
    else continue;
    assert(bb0 != bb);
    bb->for_each_until([&](Instr *x) -> bool {
      Case(PhiInstr, _, x) {
        (void)_;
        return 1;
      }
      Case(ControlInstr, _, x) {
        (void)_;
        return 1;
      }
      bb0->push1(x);
      bb->move();
      return 0;
    });
    // dbg("```cpp\n", *bb0, '\n', *bb, "\n```\n");
  }
  UnionFind<BB *> mp1, mp2;
  f->for_each([&](BB *bb) {
    mp1.add(bb);
    mp2.add(bb);
  });
  auto cplx = [&](BB *bb) {
    return !(prev[bb].size() == 1 && bb->instrs.size() == 1);
  };
  std::unordered_set<BB *> n1, n2;

  f->for_each([&](BB *bb) {
    Case(BranchInstr, br, bb->back()) {
      bool t1 = !cplx(br->target1);
      bool t0 = !cplx(br->target0);
      if (t1) {
        if (t0) {
          n1.insert(br->target1);
        } else {
          n2.insert(br->target0);
        }
      } else if (t0) {
        n2.insert(br->target1);
      }
    }
  });
  f->for_each([&](BB *bb) {
    if (!cplx(bb) && !n1.count(bb)) {
      Case(JumpInstr, jmp, bb->instrs.back().get()) {
        if (!n2.count(jmp->target)) {
          mp1.merge(bb, jmp->target);
          mp2.merge(bb, prev[bb][0]);
        }
      }
    }
  });
  f->for_each([&](BB *bb) {
    bb->for_each([&](Instr *x) {
      Case(PhiInstr, p, x) {
        p->map_BB([&](BB *&w) { w = mp2[w]; });
      }
      else {
        x->map_BB([&](BB *&w) { w = mp1[w]; });
      }
    });
  });
  remove_if_vec(f->bbs, [&](const std::unique_ptr<BB> &bb) {
    return mp1[bb.get()] != bb.get();
  });
  f->for_each([&](BB *bb) {
    Case(BranchInstr, br, bb->back()) { assert(br->target1 != br->target0); }
  });
}
void remove_phi(NormalFunc *f) {
  struct Movs {
    std::vector<std::pair<Reg, Reg>> a[2];
  };
  std::unordered_map<std::pair<BB *, BB *>, Movs> movs;
  auto float_regs = get_float_regs(f);
  f->for_each([&](BB *bb) {
    bb->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        for (auto &[r, bb0] : phi->uses) {
          if (r != phi->d1) {
            bool t1 = float_regs.count(r);
            bool t2 = float_regs.count(phi->d1);
            assert(t1 == t2);
            movs[{bb0, bb}].a[t1].emplace_back(r, phi->d1);
          }
        }
        bb->move();
      }
    });
  });
  for (auto &[edge, Ps] : movs) {
    auto [bb0, bb] = edge;
    BB *bb1 = f->new_BB();
    bb0->back()->map_BB(partial_map(bb, bb1));
    bb1->push(new JumpInstr(bb));
    for (auto &P : Ps.a) {
      auto emit_copy = [&](Reg a, Reg b) {
        if (b != a) {
          bb1->push1(new UnaryOpInstr(b, a, UnaryCompute::ID));
        }
      };
      std::unordered_map<Reg, Reg> pred, loc;
      std::unordered_map<Reg, int> deg;
      for (auto [a, b] : P) {
        pred[b] = a;
        ++deg[a];
      }
      std::vector<Reg> ready;
      for (auto [a, b] : P) {
        if (!deg[b]) {
          ready.push_back(b);
        }
      }
      while (ready.size()) {
        auto b = ready.back();
        ready.pop_back();
        auto a = pred.at(b);
        emit_copy(a, b);
        loc[a] = b;
        if (!--deg[a] && pred.count(a)) {
          ready.push_back(a);
        }
      }
      Reg n = f->new_Reg();
      for (auto [a, b] : P) {
        if (deg[b] && loc.count(b)) {
          for (Reg x = b;;) {
            deg[x] = 0;
            ready.push_back(x);
            x = pred[x];
            if (x == b)
              break;
          }
          ready.push_back(loc.at(b));
          std::reverse(ready.begin(), ready.end());
          while (ready.size() > 1) {
            Reg x = ready.back();
            ready.pop_back();
            emit_copy(ready.back(), x);
          }
        }
      }
      for (auto [a, b] : P) {
        if (deg[b]) {
          emit_copy(b, n);
          for (Reg x = b;;) {
            deg[x] = 0;
            ready.push_back(x);
            x = pred[x];
            if (x == b)
              break;
          }
          ready.push_back(n);
          std::reverse(ready.begin(), ready.end());
          while (ready.size() > 1) {
            Reg x = ready.back();
            ready.pop_back();
            emit_copy(ready.back(), x);
          }
        }
      }
    }
  }
}
void remove_unused_BB(NormalFunc *f) {
  DAG_IR dag(f);
  auto used = [&](BB *bb) {
    auto &w = dag.loop_tree[bb];
    return w.returnable;
  };
  bool is_float = 0;
  f->for_each([&](BB *bb) {
    bb->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        remove_if_vec(phi->uses, [&](const std::pair<Reg, BB *> &w) {
          return !used(w.second);
        });
      }
    });
    Case(ReturnInstr<ScalarType::Float>, _, bb->back()) {
      (void)_;
      is_float = 1;
    }
    Case(BranchInstr, br, bb->back()) {
      std::optional<BB *> target;
      if (!used(br->target1)) {
        target = br->target0;
      } else if (!used(br->target0)) {
        target = br->target1;
      }
      if (target) {
        bb->pop();
        bb->push(new JumpInstr(*target));
      }
    }
  });
  remove_if_vec(f->bbs,
                [&](const std::unique_ptr<BB> &bb) { return !used(bb.get()); });
  if (f->bbs.empty()) {
    Reg r = f->new_Reg();
    BB *bb = f->entry = f->new_BB();
    if (is_float) {
      bb->push(new LoadConst<float>(r, 0.0f));
      bb->push(new ReturnInstr<ScalarType::Float>(r, 1));
    } else {
      bb->push(new LoadConst<int32_t>(r, 0));
      bb->push(new ReturnInstr<ScalarType::Int>(r, 1));
    }
  }
}

void before_gcm_func(NormalFunc *f) {
  auto prev = build_prev(f);
  std::vector<std::pair<BB *, BB *>> edges;
  f->for_each([&](BB *bb) {
    auto out = bb->getOutNodes();
    if (out.size() > 1) {
      for (BB *next : out) {
        if (prev[next].size() > 1) {
          edges.emplace_back(bb, next);
        }
      }
    }
  });
  for (auto &[x, y] : edges) {
    BB *z = f->new_BB();
    z->push(new JumpInstr(y));
    x->back()->map_BB(partial_map(y, z));
    y->map_phi_use([](auto &) {}, partial_map(x, z));
  }
}

void before_gcm(CompileUnit *ir) { ir->for_each(before_gcm_func); }

void checkIR(NormalFunc *f) {
  auto prev = build_prev(f);
  auto defs = build_defs(f);
  f->for_each([&](BB *bb) {
    auto &ps = prev[bb];
    bb->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        bool flag = 0;
        for (auto &kv : phi->uses) {
          if (std::find(ps.begin(), ps.end(), kv.second) == ps.end()) {
            flag = 1;
          }
        }
        if (flag || phi->uses.size() != ps.size()) {
          // print_cfg(f);
          // dbg("\n```cpp\n", *f, "\n```\n");
          for (auto bb0 : ps)
            dbg(*bb0);
          dbg(*bb);
          dbg(bb->name, '\n');
          dbg(*phi, '\n');
          assert(0);
        }
      }
      Case(BranchInstr, br, x) { assert(br->target1 != br->target0); }
      Case(RegWriteInstr, rw, x) { assert(rw == defs.at(rw->d1)); }
      x->map_use([&](Reg &r) { assert(defs.count(r)); });
    });
    assert(bb->instrs.size() > 0);
    Case(ControlInstr, _, bb->back()) { (void)_; }
    else assert(0);
  });
}

void checkIR(CompileUnit *ir) {
  ir->for_each([&](NormalFunc *f) { ::checkIR(f); });
}

void simplify_BB(NormalFunc *f) {
  PassEnabled("sb") simplify_branch(f);
  ::remove_unused_BB(f);
  checkIR(f);
  PassEnabled("rtb") {
    code_reorder(f);
    remove_trivial_BB(f);
    checkIR(f);
  }
  // dbg("```cpp\n", *f, "\n```\n");
}

void DAG_IR_ALL::remove_unused_BB() {
  PassDisabled("rub") return;
  ir->for_each([&](NormalFunc *f) {
    simplify_BB(f);
    typed &= type_check(f);
  });
}

void split_live_range(NormalFunc *f) {
  PassDisabled("slr") return;
  auto defs = build_defs(f);
  struct RegInfo {
    RegWriteInstr *def;
    size_t def_pos, dead_pos;
    std::multiset<std::pair<size_t, Reg *>> use_pos;
  };
  struct BBInfo {
    size_t min_pos, max_pos;
  };
  std::unordered_map<BB *, BBInfo> bb_info;
  std::unordered_map<Reg, RegInfo> info;
  std::unordered_set<Reg> live;
  std::unordered_map<size_t, std::vector<Reg>> kill;
  size_t cur_pos = 0;
  f->for_each([&](BB *bb) {
    auto &bi = bb_info[bb];
    bi.min_pos = cur_pos;
    bb->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) {
        auto &ri = info[rw->d1];
        ri.def = rw;
        ri.def_pos = cur_pos;
      }
      Case(PhiInstr, _, x) { (void)_; }
      else {
        x->map_use([&](Reg &r) { info.at(r).use_pos.insert({cur_pos, &r}); });
      }
      cur_pos += 1;
    });
    for (BB *u : bb->getOutNodes()) {
      u->for_each([&](Instr *x) {
        Case(PhiInstr, phi, x) {
          for (auto &[r, bb0] : phi->uses) {
            if (bb0 == bb) {
              info.at(r).use_pos.insert({cur_pos, &r});
            }
          }
          cur_pos += 1;
        }
      });
    }
    bi.max_pos = cur_pos;
  });
  auto update_kill = [&](Reg k) {
    auto &v = info.at(k);
    if (v.use_pos.empty()) {
      v.dead_pos = v.def_pos;
    } else {
      v.dead_pos = (--v.use_pos.end())->first;
    }
    kill[v.dead_pos].push_back(k);
  };
  for (auto &[k, v] : info) {
    update_kill(k);
  }
  bool dbg_live = global_config.args["dbg-live"] == "1";
  size_t spill_cnt = 0;
  size_t used_spill = 0;
  size_t extra_op = 0;
  f->for_each([&](BB *bb) {
    auto &bi = bb_info[bb];
    auto check_spill = [&]() {
      if (live.size() >= 12) {
        size_t max_pos = 0;
        Reg spill;
        for (Reg r : live) {
          auto pos = info.at(r).dead_pos;
          if (max_pos <= pos) {
            pos = max_pos;
            spill = r;
          }
        }
        ++spill_cnt;
        if (dbg_live)
          dbg("spill: ", spill, '\n');
        live.erase(spill);
        /*
for (auto &[x, y] : info.at(spill).use_pos) {
  dbg("use: ", x, "  ", *y, '\n');
}*/
      }
    };
    bool on_phi = 0;
    auto on_use = [&](Reg &r) {
      if (live.count(r))
        return;
      auto &ri = info.at(r);
      auto recompute = [&](RegWriteInstr *instr0) {
        Reg r_ = r;
        Reg r0 = instr0->d1;
        auto &ri0 = info[r0];
        ri0.def = instr0;
        ri0.def_pos = cur_pos;
        if (on_phi)
          bb->push1(ri0.def);
        else
          bb->ins(ri0.def);
        size_t cnt = 0;
        for (auto it = ri.use_pos.lower_bound({cur_pos, nullptr});
             it != ri.use_pos.end(); ++it) {
          auto [pos, reg_use] = *it;
          if (pos >= bi.max_pos)
            break;
          ri0.use_pos.insert({pos, reg_use});
          assert(*reg_use == r_);
          *reg_use = r0;
          ++cnt;
          if (std::next(it) == ri.use_pos.end()) {
            --spill_cnt;
          }
          if (cnt >= 4)
            break;
        }
        if (dbg_live) {
          // dbg("cur_pos: ", cur_pos, '\n');
          dbg("recompute: ", r_, " => ", *instr0, "  cnt: ", cnt, '\n');
        }
        extra_op += 1;
        check_spill();
        live.insert(r0);
        update_kill(r0);
        assert(cnt >= 1);
        assert(r == r0);
      };
      PassEnabled("slr-rc") {
        Case(LoadConst<int32_t>, lc, ri.def) {
          Reg r0 = f->new_Reg();
          return recompute(new LoadConst<int32_t>(r0, lc->value));
        }
        Case(LoadConst<float>, lc, ri.def) {
          Reg r0 = f->new_Reg();
          return recompute(new LoadConst<float>(r0, lc->value));
        }
        Case(LoadAddr, la, ri.def) {
          Reg r0 = f->new_Reg();
          return recompute(new LoadAddr(r0, la->offset));
        }
      }
      ++used_spill;
      if (dbg_live)
        dbg("use spilled reg: ", *defs.at(r), "\n");
    };
    cur_pos = bi.min_pos;
    bb->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) {
        check_spill();
        live.insert(rw->d1);
      }
      Case(PhiInstr, _, x) { (void)_; }
      else {
        x->map_use(on_use);
      }
      for (auto x : kill[cur_pos])
        live.erase(x);
      if (dbg_live)
        dbg(live.size(), "\t", *x, '\n');
      cur_pos += 1;
    });
    on_phi = 1;
    for (BB *u : bb->getOutNodes()) {
      u->for_each([&](Instr *x) {
        Case(PhiInstr, phi, x) {
          for (auto &[r, bb0] : phi->uses) {
            if (bb0 == bb) {
              on_use(r);
            }
          }
          cur_pos += 1;
        }
      });
    }
    assert(cur_pos == bi.max_pos);
  });
  dbg("IR estimated spill: ", spill_cnt, '\n');
  dbg("IR estimated used spill: ", used_spill, '\n');
  dbg("IR estimated extra op: ", extra_op, '\n');
}
