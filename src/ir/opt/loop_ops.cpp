#include "ir/opt/dag_ir.hpp"

template <class K> using uset = std::unordered_set<K>;
template <class K, class V> using umap = std::unordered_map<K, V>;

#define BE(x) (x).begin(), (x).end()

struct CmpExpr {
  Reg s1, s2;
  bool less, eq;
  void neg() {
    less = !less;
    eq = !eq;
  }
  void swap() {
    std::swap(s1, s2);
    less = !less;
  }
  static std::optional<CmpExpr> make(BinaryOpInstr *bop) {
    CmpExpr e;
    e.s1 = bop->s1;
    e.s2 = bop->s2;
    e.less = 1;
    switch (bop->op.type) {
    case BinaryCompute::LESS:
      e.eq = 0;
      break;
    case BinaryCompute::LEQ:
      e.eq = 1;
      break;
    default:
      return std::nullopt;
    }
    return e;
  }
  const char *name() { return less ? (eq ? "<=" : "<") : (eq ? ">=" : ">"); }
  bool compute(int32_t x, int32_t y) {
    return less ? (eq ? x <= y : x < y) : (eq ? x >= y : x > y);
  }
};
std::ostream &operator<<(std::ostream &os, const CmpExpr &w) {
  os << w.s1;
  os << (w.less ? '<' : '>');
  if (w.eq)
    os << '=';
  os << w.s2;
  return os;
}

struct SimpleIndVar {
  Reg init, step;
  BinaryCompute op;
};

std::ostream &operator<<(std::ostream &os, const SimpleIndVar &w) {
  os << "i=" << w.init << "; i" << BinaryOp(w.op) << "=" << w.step << "; ";
  return os;
}

struct FindLoopVar : SimpleLoopVisitor, Defs {
  struct LoopVarInfo {
    RegWriteInstr *def;
    std::optional<SimpleIndVar> ind;
  };
  struct LoopInfo {
    DAG_IR::LoopTreeNode *node;
    umap<Reg, RegWriteInstr *> defs; // def in loop
    umap<Reg, LoopVarInfo> vars;
    std::optional<CmpExpr> cond;
    uset<BB *> bbs;
    size_t nested_cnt = 0, instr_cnt = 0;
  };
  umap<BB *, LoopInfo> loop_info;

  FindLoopVar(NormalFunc *_f) : Defs(_f) {}
  void visitLoopTreeNode(BB *w, DAG_IR::LoopTreeNode *node) {
    assert(node);
    auto &wi = loop_info[w];
    wi.node = node;
    if (!w)
      return;
    wi.instr_cnt += w->instrs.size();
    w->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) { wi.defs[rw->d1] = rw; }
    });
    if (wi.node->is_loop_head) {
      w->for_each([&](Instr *x) {
        Case(PhiInstr, phi, x) { wi.vars[phi->d1].def = phi; }
      });
    }
  }
  void end(BB *w) {
    if (!w)
      return;

    auto &wi = loop_info.at(w);
    wi.bbs.insert(w);

    BB *h = wi.node->loop_head;
    auto &hi = loop_info.at(h);
    hi.defs.insert(BE(wi.defs));
    hi.bbs.insert(BE(wi.bbs));
    hi.nested_cnt = std::max(hi.nested_cnt, wi.nested_cnt + 1);
    hi.instr_cnt += wi.instr_cnt;

    if (wi.node->is_loop_head) {
      checkVars(w);
      auto &may_exit = wi.node->may_exit;
      if (may_exit.size() == 1) {
        BB *u = may_exit[0];
        if (u == w) {
          checkLoopHeadExit(w);
        }
      }
    }
  }
  DomTreeNode *dom_node(BB *w) { return loop_info.at(w).node->dom_tree_node; }
  bool sdom(BB *w, BB *u) { return dom_node(w)->sdom(dom_node(u)); }
  void checkVars(BB *w) {
    auto &wi = loop_info.at(w);
    assert(wi.node->is_loop_head);
    /*
        dbg("Loop: ", w->name, '\n');
        for (auto x : wi.bbs)
          dbg(x->name, "; ");
        dbg("\n");
        for (auto x : wi.defs)
          dbg(*x.second, ";\n");
      */

    for (auto &[r, ri] : wi.vars) {
      Case(PhiInstr, phi, ri.def) {
        std::vector<Reg> u1, u2;
        for (auto [r, bb] : phi->uses) {
          if (sdom(bb, w)) {
            u1.emplace_back(r);
          } else {
            u2.emplace_back(r);
          }
        }
        if (u1.size() == 1 && u2.size() == 1) {
          Reg r1 = u1[0];
          Reg r2 = u2[0];
          // dbg(r, r1, r2, *phi, '\n');
          if (wi.defs.count(r2)) {
            auto def = wi.defs[r2];
            Case(BinaryOpInstr, bop, def) {
              // dbg(*bop, '\n');
              if (bop->s1 == phi->d1 && !wi.defs.count(bop->s2)) {
                ri.ind = SimpleIndVar{r1, bop->s2, bop->op.type};
                // dbg(w->name, ": ", r, " ind ", *ri.ind, '\n');
              }
            }
          }
        }
      }
    }
  }
  void checkLoopHeadExit(BB *w) {
    auto &wi = loop_info.at(w);
    assert(wi.node->is_loop_head);
    Case(BranchInstr, br, w->back()) {
      Case(BinaryOpInstr, bop, defs.at(br->cond)) {
        auto e = CmpExpr::make(bop);
        assert(wi.bbs.count(br->target1) + wi.bbs.count(br->target0) == 1);
        if (e) {
          if (!wi.bbs.count(br->target1)) {
            e->neg();
          }
          auto check = [&]() -> bool {
            return wi.defs.count(e->s1) && !wi.defs.count(e->s2);
          };
          if (!check())
            e->swap();
          if (check())
            wi.cond = e;
        }
      }
    }
    else assert(0);
    if (wi.cond) {
      Reg r = wi.cond->s1;
      if (wi.vars.count(r)) {
        // dbg(w->name, ": ", *wi.cond, '\n');
      }
    }
  }
};

struct LoopCopyTool {
  BB *entry, *exit;
  NormalFunc *f;
  umap<BB *, BB *> bbs, bbs_rev;
  umap<Reg, Reg> regs, regs_rev;
  LoopCopyTool(const uset<BB *> &_bbs, BB *_entry, BB *_exit, NormalFunc *_f)
      : entry(_entry), exit(_exit), f(_f) {
    for (BB *bb : _bbs) {
      bbs[bb] = bb;
      bb->for_each([&](Instr *x) {
        Case(RegWriteInstr, rw, x) {
          Reg r = rw->d1;
          regs[r] = r;
        }
      });
    }
    bbs_rev = bbs;
    regs_rev = regs;
  }
  void copy(std::string name_suffix) {
    regs_rev.clear();
    for (auto &[k, v] : regs) {
      v = f->new_Reg(f->get_name(k) + name_suffix);
      regs_rev[v] = k;
    }
    bbs_rev.clear();
    for (auto &[k, v] : bbs) {
      v = f->new_BB(k->name + name_suffix);
      bbs_rev[v] = k;
    }
    auto mp_regs = partial_map(regs);
    auto mp_bbs = partial_map(bbs);
    for (auto &[k, v] : bbs) {
      k->for_each(
          [&](Instr *x) { v->push(x->map(mp_regs, mp_bbs, [](auto &) {})); });
    }
    entry = bbs.at(entry);
    exit = bbs.at(exit);
  }
  void entry_del_edge(bool back) {
    entry->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        remove_if_vec(phi->uses, [&](const std::pair<Reg, BB *> kv) {
          return bbs_rev.count(kv.second) == back;
        });
      }
    });
  }
  void no_exit() { exit_to(get_exit_next(1)); }
  void exit_to(BB *w) {
    exit->pop();
    exit->push(new JumpInstr(w));
  }
  BB *get_exit_next(bool tp = 0) {
    Case(BranchInstr, br, exit->back()) {
      int n = 0;
      BB *ans = nullptr;
      for (BB *bb : {br->target1, br->target0}) {
        if (tp == bbs_rev.count(bb)) {
          ans = bb;
          ++n;
        }
      }
      assert(n == 1);
      return ans;
    }
    else assert(0);
    return nullptr;
  }
  void change_to(LoopCopyTool &w, BB *bb) {
    bb->map_phi_use(sequential(partial_map(regs_rev), partial_map(w.regs)),
                    sequential(partial_map(bbs_rev), partial_map(w.bbs)));
  }
  void back_edge_to(BB *bb) {
    auto f = partial_map(entry, bb);
    for (auto &w : bbs_rev) {
      w.first->back()->map_BB(f);
    }
  }
};

void simplify_BB(NormalFunc *f);

struct UnrollLoop {
  FindLoopVar &S;
  UnrollLoop(FindLoopVar &_S) : S(_S) {}
  bool apply() { return dfs(nullptr); }
  bool dfs(BB *w) {
    auto &wi = S.loop_info.at(w);
    for (BB *u : wi.node->dfn) {
      if (u == w)
        continue;
      if (dfs(u))
        return 1;
    }
    if (unroll_fixed(w))
      return 1;
    return 0;
  }
  void _unroll_fixed(BB *w, size_t cnt) {
    auto &wi = S.loop_info.at(w);
    std::deque<LoopCopyTool> loops;
    loops.emplace_back(wi.bbs, w, w, S.f);
    for (size_t i = 1; i <= cnt; ++i) {
      loops.emplace_back(loops[0]);
      loops.back().copy(std::string(":") + std::to_string(i) + ":");
    }
    auto &p0 = loops[0];
    auto &p3 = loops.back();
    BB *next = p0.get_exit_next();
    p0.entry_del_edge(1);
    for (size_t i = 1; i <= cnt; ++i) {
      auto &p1 = loops[i - 1];
      auto &p2 = loops[i];
      p1.no_exit();
      p2.entry_del_edge(0);
    }
    for (size_t i = 1; i <= cnt; ++i) {
      auto &p1 = loops[i - 1];
      auto &p2 = loops[i];
      p1.back_edge_to(p2.entry);
      p2.change_to(p1, p2.entry);
    }
    p3.exit_to(next);
    p0.change_to(p3, next);
    umap<BB *, BB *> bbs_rev;
    for (auto &p : loops)
      bbs_rev.insert(BE(p.bbs_rev));
    S.f->for_each([&](BB *bb) {
      if (!bbs_rev.count(bb)) {
        bb->map_use(partial_map(p3.regs));
      }
    });
  }
  bool unroll_fixed(BB *w) {
    auto &wi = S.loop_info.at(w);
    if (wi.nested_cnt != 1)
      return 0;
    // if(wi.instr_cnt>20)return 0;
    if (!wi.cond)
      return 0;
    auto r = S.get_const(wi.cond->s2);
    if (!r)
      return 0;
    Reg i = wi.cond->s1;
    if (!wi.vars.count(i))
      return 0;
    auto ind = wi.vars[i].ind;
    if (!ind)
      return 0;
    auto l = S.get_const(ind->init);
    if (!l)
      return 0;
    auto step = S.get_const(ind->step);
    if (!step)
      return 0;
    auto op = ind->op;
    // for(i=l;i<r;i{op}=step);
    int32_t i0 = *l, i1 = *r, i2 = *step;
    dbg("for(i=", i0, ";i", wi.cond->name(), i1, ";i=i", BinaryOp(op), i2,
        "){...}\n");
    size_t cnt = 0;
    constexpr size_t MAX_UNROLL = 32, MAX_UNROLL_INSTR = 640;
    while (cnt <= MAX_UNROLL && wi.cond->compute(i0, i1)) {
      i0 = std::get<int32_t>(typed_compute(op, i0, i2));
      cnt += 1;
    }
    if (cnt <= MAX_UNROLL) {
      if (cnt * wi.instr_cnt > MAX_UNROLL_INSTR)
        return 0;
      dbg(">>> unroll: cnt=", cnt, " instr=", wi.instr_cnt, '\n');
      bool dbg_on(global_config.args["dbg-unroll"] == "1");
      if (dbg_on)
        print_cfg(S.f);
      _unroll_fixed(w, cnt);
      if (dbg_on) {
        print_cfg(S.f);
        dbg("\n```cpp\n", *S.f, "\n```\n");
      }
      simplify_BB(S.f);
      return 1;
    }
    return 0;
  }
};

bool unroll_loop(FindLoopVar &S) {
  UnrollLoop w(S);
  return w.apply();
}

void loop_ops(NormalFunc *f) {
  for (int T = 0; T < 10; ++T) {
    DAG_IR dag(f);
    FindLoopVar w(f);
    dag.visit(w);
    if (!unroll_loop(w))
      break;
  }
}
