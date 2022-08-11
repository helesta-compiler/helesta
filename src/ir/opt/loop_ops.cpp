#include "add_expr.hpp"
#include "ir/opt/dag_ir.hpp"
#include <climits>

template <class K> using uset = std::unordered_set<K>;
template <class K, class V> using umap = std::unordered_map<K, V>;

#define BE(x) (x).begin(), (x).end()

std::ostream &operator<<(std::ostream &os, const SimpleIndVar &w) {
  os << "i=" << w.init << "; i" << BinaryOp(w.op) << "=" << w.step << "; ";
  return os;
}

std::optional<CmpExpr> CmpExpr::make(BinaryOpInstr *bop) {
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

std::ostream &operator<<(std::ostream &os, const CmpExpr &w) {
  os << w.s1;
  os << (w.less ? '<' : '>');
  if (w.eq)
    os << '=';
  os << w.s2;
  return os;
}

struct FindLoopVar : SimpleLoopVisitor, Defs {
  struct LoopVarInfo {
    RegWriteInstr *def;
    std::optional<SimpleIndVar> ind;
    std::optional<SimpleReductionVar> reduce;
  };
  struct LoopInfo {
    DAG_IR::LoopTreeNode *node;
    umap<Reg, RegWriteInstr *> defs; // def in loop
    umap<Reg, LoopVarInfo> vars;
    std::optional<CmpExpr> cond;
    uset<BB *> bbs;
    umap<Reg, int> use_count;
    size_t nested_cnt = 0, instr_cnt = 0;
  };
  umap<BB *, LoopInfo> loop_info;
  std::map<IR::Reg, int> use_count;

  std::optional<std::tuple<Reg, Reg, Reg, CmpOp>>
  get_ilr(BB *w, bool no_export_var = 0) {
    auto &wi = loop_info.at(w);
    if (!wi.node->is_loop_head)
      return std::nullopt;
    if (!wi.cond)
      return std::nullopt;
    Reg i = wi.cond->s1;
    if (!wi.vars.count(i))
      return std::nullopt;
    auto ind = wi.vars[i].ind;
    if (!ind)
      return std::nullopt;
    auto step = get_const(ind->step);
    if (!step || *step != 1)
      return std::nullopt;
    auto op = ind->op;
    if (op != BinaryCompute::ADD)
      return std::nullopt;
    bool pure = 1;
    w->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) {
        /*if (no_export_var && (wi.use_count[rw->d1] != use_count[rw->d1])) {
          dbg(*x, '\n');
          dbg(wi.use_count[rw->d1], " != ", use_count[rw->d1], '\n');
        }*/
        if (no_export_var)
          pure &= (wi.use_count[rw->d1] == use_count[rw->d1]);
      }
      else Case(StoreInstr, _, x) {
        (void)_;
        pure = 0;
      }
      else Case(CallInstr, _, x) {
        (void)_;
        pure = 0;
      }
    });
    if (!pure)
      return std::nullopt;
    Reg l = ind->init;
    Reg r = wi.cond->s2;
    // dbg("for(i=", l, ";i", wi.cond->name(), r, ";i=i", BinaryOp(op), *step,
    //  "){...}  i:", i, "\n");
    return std::make_tuple(i, l, r, wi.cond->op());
  }

  FindLoopVar(NormalFunc *_f) : Defs(_f) { use_count = build_use_count(f); }
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
    for (BB *bb : wi.bbs) {
      bb->map_use([&](Reg &r) { ++wi.use_count[r]; });
    }

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
              if (bop->s1 == phi->d1) {
                if (!wi.defs.count(bop->s2)) {
                  ri.ind = SimpleIndVar{r1, bop->s2, bop->op.type};
                  // dbg(w->name, ": ", r, " ind ", *ri.ind, '\n');
                } else {
                  ri.reduce = SimpleReductionVar{r1, bop->s2, bop->op.type,
                                                 std::nullopt};
                }
              } else if (bop->op.type == BinaryCompute::MOD) {
                if (auto mod = get_const(bop->s2)) {
                  Case(BinaryOpInstr, bop2, defs.at(bop->s1)) {
                    if (bop2->s1 == phi->d1) {
                      ri.reduce =
                          SimpleReductionVar{r1, bop2->s2, bop2->op.type, mod};
                    }
                  }
                }
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

struct ArrayReadWrite : SimpleLoopVisitor {
  struct LoopInfo {
    uset<Reg> rs, ws;
    bool call = 0;
    void operator|=(LoopInfo &x) {
      rs.insert(BE(x.rs));
      ws.insert(BE(x.ws));
      call |= x.call;
    }
  };
  struct RegInfo {
    MulAddExpr add;
    AddrExpr addr;
    std::optional<MulAddExpr> min, max;
    std::pair<int, int> get_range() {
      int l = INT_MIN;
      int r = INT_MAX;
      if (min) {
        if (auto c = min->get_c_if()) {
          l = *c;
        }
      }
      if (max) {
        if (auto c = max->get_c_if()) {
          r = *c;
        }
      }
      return {l, r};
    }
  };
  umap<Reg, RegInfo> reg_info;
  umap<BB *, LoopInfo> loop_info;
  FindLoopVar &S;
  ArrayReadWrite(FindLoopVar &_S) : S(_S) {}
  bool dependent(const EqContext &ctx, Reg r1, Reg r2) {
    auto &v1 = reg_info.at(r1).addr;
    auto &v2 = reg_info.at(r2).addr;
    if (v1.maybe_eq(v2, ctx)) {
      // dbg("### dependent: ", v1, "  ", v2, '\n');
      return 1;
    }
    return 0;
  }
  bool dependent(BB *bb) {
    auto &wi = loop_info.at(bb);
    if (wi.call)
      return 1;
    if ((wi.rs.size() + wi.ws.size()) * wi.ws.size() >= 100)
      return 1;
    auto &wi0 = S.loop_info.at(bb);
    if (wi0.vars.size() != 1)
      return 1;
    MulAddExpr zero;
    umap<Reg, MulAddExpr> mp;
    EqContext ctx{[&](Reg r) -> std::pair<EqContext::Type, MulAddExpr &> {
      if (!wi0.defs.count(r)) {
        return {EqContext::IND, zero};
      }
      if (wi0.vars.count(r)) {
        if (mp.count(r))
          return {EqContext::IND, mp.at(r)};
        auto &ind = wi0.vars.at(r).ind;
        if (ind) {
          const auto &c = S.get_const(ind->step);
          if (c) {
            auto &w = mp[r];
            w.add_eq(*c);
            return {EqContext::IND, w};
          }
        }
      }
      return {EqContext::ANY, zero};
    }};
    for (Reg w : wi.ws) {
      for (Reg r : wi.rs) {
        if (dependent(ctx, w, r))
          return 1;
      }
      for (Reg w2 : wi.ws) {
        if (w < w2 && dependent(ctx, w, w2))
          return 1;
      }
    }
    return 0;
  }
  void visitLoopTreeNode(BB *w, DAG_IR::LoopTreeNode *) { loop_info[w]; }
  void visitBB(BB *w) {
    auto &wi = loop_info.at(w);
    w->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) {
        auto &ri = reg_info[rw->d1];
        ri.add.bad = 1;
        ri.addr.bad = 1;
        Case(LoadConst<int32_t>, lc, rw) {
          ri.add.bad = 0;
          ri.add.add_eq(lc->value);
        }
        Case(PhiInstr, phi, rw) {
          ri.add.bad = 0;
          ri.add.add_eq(phi->d1, 1, 1);
        }
        else Case(LoadAddr, la, rw) {
          ri.addr.bad = 0;
          ri.addr.base = la->offset;
        }
        else Case(ArrayIndex, ai, rw) {
          ri.addr = reg_info.at(ai->s1).addr;
          ri.addr.add_eq(ai->size, reg_info.at(ai->s2).add);
        }
        else Case(BinaryOpInstr, bop, rw) {
          switch (bop->op.type) {
          case BinaryCompute::ADD:
            ri.add = reg_info.at(bop->s1).add;
            ri.add.add_eq(reg_info.at(bop->s2).add, 1);
            break;
          case BinaryCompute::SUB:
            ri.add = reg_info.at(bop->s1).add;
            ri.add.add_eq(reg_info.at(bop->s2).add, -1);
            break;
          case BinaryCompute::MUL:
            ri.add.bad = 0;
            ri.add.set_mul(reg_info.at(bop->s1).add, reg_info.at(bop->s2).add,
                           1);
            break;
          default:
            break;
          }
        }
        else Case(LoadInstr, ld, rw) {
          wi.rs.insert(ld->addr);
        }
        else Case(CallInstr, call, rw) {
          (void)call;
          wi.call = 1;
        }
      }
      else Case(StoreInstr, st, x) {
        wi.ws.insert(st->addr);
      }
    });
  }
  void end(BB *w) {
    if (!w)
      return;
    auto &wi0 = S.loop_info.at(w);
    auto &wi = loop_info.at(w);
    BB *h = wi0.node->loop_head;
    auto &hi = loop_info.at(h);
    hi |= wi;
    if (auto ilr = S.get_ilr(w, 1)) {
      auto [i, l, r, op] = *ilr;
      auto &ri = reg_info.at(i);
      ri.min = reg_info.at(l).add;
      ri.max = reg_info.at(r).add;
      if (!op.eq) {
        ri.max->add_eq(-1);
      }
    }
  }
  bool loop_parallel(BB *w, CompileUnit *ir);
  bool simplify_reduction_var(BB *w, CompileUnit *ir);
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
      v->disable_parallel = k->disable_parallel;
      v->disable_unroll = k->disable_unroll;
    }
    auto mp_regs = partial_map(regs);
    auto mp_bbs = partial_map(bbs);
    for (auto &kv : bbs) {
      auto k = kv.first;
      auto v = kv.second;
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
  BB *get_entry_prev() {
    BB *ans = nullptr;
    entry->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        for (auto [r, bb] : phi->uses) {
          if (!bbs_rev.count(bb)) {
            assert(!ans || ans == bb);
            ans = bb;
          }
        }
      }
    });
    assert(ans);
    return ans;
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
void code_reorder(NormalFunc *f);
void remove_unused_BB(NormalFunc *f);
void global_value_numbering_func(IR::NormalFunc *func);
void remove_unused_def_func(IR::NormalFunc *func);
void remove_trivial_BB(NormalFunc *f);
void pretty_print_func(NormalFunc *f);

void after_unroll(NormalFunc *f) {
  remove_unused_BB(f);
  checkIR(f);
  global_value_numbering_func(f);
  remove_unused_def_func(f);
  code_reorder(f);
  remove_trivial_BB(f);
}

int parseIntArg(int v, std::string s) {
  if (global_config.args.count(s)) {
    int x = v;
    if (sscanf(global_config.args[s].data(), "%d", &x) == 1) {
      v = x;
    }
  }
  return v;
}

bool ArrayReadWrite::loop_parallel(BB *w, CompileUnit *ir) {
  if (w->disable_parallel)
    return 0;
  auto &wi0 = S.loop_info.at(w);
  auto &wi = loop_info.at(w);
  bool dbg_on = (global_config.args["dbg-par"] == "1");
  if (auto ilr = S.get_ilr(w, 1)) {
    bool flag = dependent(w);
    if (dbg_on) {
      for (BB *bb : wi0.node->dfn) {
        if (bb != w && S.loop_info.at(bb).node->is_loop_head) {
          dbg('[', bb->name, ']', ' ');
        } else {
          dbg(bb->name, ' ');
        }
      }
      dbg('\n');
      dbg(">>> data ", (flag ? "" : "in"), "dependent for each i\n");
      for (Reg r : wi.rs) {
        dbg("R: ", reg_info.at(r).addr, '\n');
      }
      for (Reg r : wi.ws) {
        dbg("W: ", reg_info.at(r).addr, '\n');
      }
    }
    auto [i_, l, r, op] = *ilr;
    if (!(op.less))
      return 0;
    if (!flag) {
      dbg(">>> loop_parallel\n");
      if (dbg_on)
        print_cfg(S.f);

      std::deque<LoopCopyTool> loops;
      loops.emplace_back(wi0.bbs, w, w, S.f);
      size_t cnt = parseIntArg(3, "num-threads");
      for (size_t i = 1; i <= cnt; ++i) {
        loops.emplace_back(loops[0]);
        loops.back().copy(std::string(":") + std::to_string(i) + ":");
      }
      auto &p0 = loops[0];
      p0.entry->disable_parallel = 1;

      BB *prev = p0.get_entry_prev();
      BB *next = p0.get_exit_next();

      BB *head = S.f->new_BB();
      BB *bb1 = S.f->new_BB();
      BB *tail = S.f->new_BB();
      CodeGen cg(S.f);

      auto n = cg.reg(r) - cg.reg(l);
      int min_par_loop_cnt = 4;
      min_par_loop_cnt = wi0.nested_cnt == 1 ? (1 << 12) : (1 << 6);
      cg.branch(n < cg.lc(min_par_loop_cnt), p0.entry, bb1);
      head->push(std::move(cg.instrs));

      prev->map_BB(partial_map(p0.entry, head));

      auto fork = ir->lib_funcs.at("__create_threads").get();
      auto join = ir->lib_funcs.at("__join_threads").get();

      auto l0 = cg.reg(l);
      auto step = n / cg.lc(cnt);

      p0.entry->map_phi_use([&](Reg &, BB *&bb) {
        if (bb == prev)
          bb = head;
      });

      for (size_t i = 1; i <= cnt; ++i) {
        auto &p1 = loops[i];
        auto l0_0 = l0.r;
        BB *bb1_0 = bb1;

        if (i < cnt) {
          BB *bb2 = S.f->new_BB();
          auto r0 = l0 + step;
          cg.branch(cg.call(fork, ScalarType::Int), p1.entry, bb2);
          bb1->push(std::move(cg.instrs));
          bb1 = bb2;
          l0 = r0;
          Case(BranchInstr, br, p1.exit->back()) {
            auto tg1 = br->target1, tg0 = br->target0;
            cg.branch(cg.reg(p1.regs.at(i_)) < r0, tg1, tg0);
            p1.exit->pop();
            p1.exit->push(std::move(cg.instrs));
          }
          else assert(0);
        } else {
          cg.jump(p1.entry);
          bb1->push(std::move(cg.instrs));
        }

        p1.entry->map_phi_use([&](Reg &r, BB *&bb) {
          if (bb == prev) {
            r = l0_0;
            bb = bb1_0;
          }
        });
      }
      std::vector<BB *> joins;
      for (size_t i = 1; i <= cnt; ++i) {
        BB *bb = S.f->new_BB();
        auto &p1 = loops[i];
        p1.exit->map_BB(partial_map(next, bb));
        if (i != cnt)
          cg.call(join, ScalarType::Void,
                  {{cg.lc(0), ScalarType::Int}}); // wait
        if (i != 1)
          cg.call(join, ScalarType::Void,
                  {{cg.lc(1), ScalarType::Int}}); // exit
        cg.jump(tail);
        bb->push(std::move(cg.instrs));
      }

      for (size_t i = 0; i <= cnt; ++i) {
        loops[i].exit->map_BB(partial_map(next, tail));
      }
      next->map_BB(partial_map(p0.exit, tail));

      cg.jump(next);
      tail->push(std::move(cg.instrs));

      for (size_t i = 0; i <= cnt; ++i) {
        for (auto &kv : loops[i].bbs) {
          kv.second->disable_parallel = 1;
          kv.second->thread_id = i;
        }
      }

      if (dbg_on) {
        print_cfg(S.f);
        dbg("\n```cpp\n", *S.f, "\n```\n");
      }

      return 1;
    }
  }
  return 0;
}

bool ArrayReadWrite::simplify_reduction_var(BB *w, CompileUnit *ir) {
  PassDisabled("sr") return 0;
  auto &wi0 = S.loop_info.at(w);
  bool dbg_on = (global_config.args["dbg-sr"] == "1");
  if (auto ilr = S.get_ilr(w)) {
    auto [i, i1, i2, op] = *ilr;
    auto i0 = S.get_const(i1);
    CodeGen cg(S.f);
    using RegRef = CodeGen::RegRef;
    auto umulmod = [&](RegRef a, RegRef b, RegRef c) {
      return cg.call(
          ir->lib_funcs.at("__umulmod").get(), ScalarType::Int,
          {{a, ScalarType::Int}, {b, ScalarType::Int}, {c, ScalarType::Int}});
    };
    auto u_c_np1_2_mod = [&](RegRef a, RegRef b) {
      return cg.call(ir->lib_funcs.at("__u_c_np1_2_mod").get(), ScalarType::Int,
                     {{a, ScalarType::Int}, {b, ScalarType::Int}});
    };
    auto s_c_np1_2 = [&](RegRef a) {
      return cg.call(ir->lib_funcs.at("__s_c_np1_2").get(), ScalarType::Int,
                     {{a, ScalarType::Int}});
    };
    auto umod = [&](RegRef a, RegRef b) {
      return cg.call(ir->lib_funcs.at("__umod").get(), ScalarType::Int,
                     {{a, ScalarType::Int}, {b, ScalarType::Int}});
    };
    auto fixmod = [&](RegRef a, RegRef b) {
      return cg.call(ir->lib_funcs.at("__fixmod").get(), ScalarType::Int,
                     {{a, ScalarType::Int}, {b, ScalarType::Int}});
    };

    umap<Reg, Reg> mp;

    for (auto &[r, var] : wi0.vars) {
      if (!var.reduce)
        continue;
      if (wi0.use_count[r] != 1)
        continue;
      auto &reduce = *var.reduce;
      if (reduce.op == BinaryCompute::ADD) {
        auto &step = reg_info.at(reduce.step).add;
        if (step.bad)
          continue;
        bool poly_i = 1, positive = (i0 && *i0 >= 0);
        int max_pow = 0;
        for (auto &[k, v] : step.cs) {
          for (auto &[k0, v0] : k) {
            if (k0 == i)
              max_pow = std::max(max_pow, 1);
            else if (wi0.defs.count(k0))
              poly_i = 0;
          }
          if (v < 0)
            positive = 0;
        }
        if (!poly_i)
          continue;
        if (max_pow > 1)
          continue;
        if (reduce.mod) {
          if (*reduce.mod <= 1)
            continue;
          if (!positive)
            continue;
          if (auto c0 = S.get_const(reduce.init)) {
            if (*c0 < 0)
              continue;
            // optimizeable
          } else
            continue;
        } else {
          // optimizeable
        }
        auto lv = cg.reg(i1) - cg.lc(1), rv = cg.reg(i2);
        if (!op.eq) {
          rv = rv - cg.lc(1);
        }
        RegRef mod;
        if (reduce.mod) {
          mod = cg.lc(*reduce.mod);
        }
        auto add = [&](RegRef a, RegRef b) {
          if (reduce.mod) {
            return umod(a + b, mod);
          }
          return a + b;
        };
        auto sub = [&](RegRef a, RegRef b) {
          if (reduce.mod) {
            return umod(a - b + mod, mod);
          }
          return a - b;
        };
        auto mul = [&](RegRef a, RegRef b) {
          if (reduce.mod) {
            return umulmod(a, b, mod);
          }
          return a * b;
        };
        auto cnp12 = [&](RegRef a) {
          if (reduce.mod) {
            return u_c_np1_2_mod(a, mod);
          }
          return s_c_np1_2(a);
        };
        auto fix = [&](RegRef a) {
          if (reduce.mod) {
            return fixmod(a, mod);
          }
          return a;
        };
        auto calc = [&](RegRef x) {
          x = fix(x);
          auto s = cg.lc(0);
          for (auto &[k, v] : step.cs) {
            auto p = fix(cg.lc(v));
            int i_pow = 0;
            for (auto &[k0, v0] : k) {
              if (k0 != i) {
                for (int t = 0; t < v0; ++t) {
                  p = mul(p, cg.reg(k0));
                }
              } else {
                assert(v0 == 1);
                i_pow = 1;
                p = mul(p, cnp12(x));
              }
            }
            if (!i_pow)
              p = mul(p, x);
            s = add(s, p);
          }
          return s;
        };
        auto s = cg.reg(reduce.init);
        s = sub(add(fix(s), calc(rv)), calc(lv));
        mp[r] = s.r;
        dbg("reduce: s=", r, "  init=", reduce.init, "  step=", step,
            "  mod=", (reduce.mod ? *reduce.mod : -1), "\n");
      }
    }

    if (mp.size()) {
      LoopCopyTool p0(wi0.bbs, w, w, S.f);
      BB *next = p0.get_exit_next();
      BB *bb1 = S.f->new_BB();
      BB *bb2 = S.f->new_BB();
      BB *bb3 = S.f->new_BB();
      cg.jump(bb3);
      bb2->push(std::move(cg.instrs));
      if (dbg_on)
        dbg(*bb2);
      if (op.eq) {
        cg.branch(cg.reg(i1) <= cg.reg(i2), bb2, bb3);
      } else {
        cg.branch(cg.reg(i1) < cg.reg(i2), bb2, bb3);
      }
      bb1->push(std::move(cg.instrs));
      for (auto &[k, v] : mp) {
        Reg t = S.f->new_Reg();
        auto phi = new PhiInstr(t);
        phi->add_use(wi0.vars.at(k).reduce.value().init, bb1);
        phi->add_use(v, bb2);
        dbg(k, " => ", v, " => ", t, '\n');
        v = t;
        bb3->push(phi);
      }
      p0.exit->map_BB(partial_map(next, bb1));
      next->map_BB(partial_map(p0.exit, bb3));
      S.f->for_each([&](BB *bb) {
        if (!wi0.bbs.count(bb)) {
          bb->map_use(partial_map(mp));
        }
      });
      cg.jump(next);
      bb3->push(std::move(cg.instrs));
      // dbg(*bb1, *bb2, *bb3);
      checkIR(S.f);
      global_value_numbering_func(S.f);
      remove_unused_def_func(S.f);
      return 1;
    }
  }
  return 0;
}

struct UnrollLoop {
  FindLoopVar &S;
  ArrayReadWrite &arw;
  bool last;
  CompileUnit *ir;
  UnrollLoop(FindLoopVar &_S, ArrayReadWrite &_arw, bool _last,
             CompileUnit *_ir)
      : S(_S), arw(_arw), last(_last), ir(_ir) {}
  bool parallel_only;
  bool apply(bool _parallel_only) {
    parallel_only = _parallel_only;
    return dfs(nullptr);
  }
  bool dfs(BB *w) {
    auto &wi = S.loop_info.at(w);
    if (w && remove_unused_loop(w))
      return 1;
    if (w && parallel_only && loop_parallel(w))
      return 1;
    for (BB *u : wi.node->dfn) {
      if (u == w)
        continue;
      if (dfs(u))
        return 1;
    }
    if (w && !parallel_only) {
      if (arw.simplify_reduction_var(w, ir))
        return 1;
      if (unroll_fixed(w))
        return 1;
      if (unroll_simple_for_loop(w))
        return 1;
    }
    return 0;
  }
  bool remove_unused_loop(BB *w) {
    auto &wi0 = S.loop_info.at(w);
    auto &wi = arw.loop_info.at(w);
    if (wi.call || wi.ws.size())
      return 0;
    if (!S.get_ilr(w))
      return 0;
    bool flag = 1;
    w->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) {
        flag &= (wi0.use_count[rw->d1] == S.use_count[rw->d1]);
      }
    });
    if (flag) {
      LoopCopyTool p0(wi0.bbs, w, w, S.f);
      BB *prev = p0.get_entry_prev();
      BB *next = p0.get_exit_next();
      prev->map_BB(partial_map(p0.entry, next));
      next->map_BB(partial_map(p0.exit, prev));
      dbg(">>> remove unused loop\n");
      after_unroll(S.f);
      return 1;
    }
    return 0;
  }
  bool loop_parallel(BB *w) {
    PassDisabled("par") return 0;
    if (!last)
      return 0;
    return arw.loop_parallel(w, ir);
  }
  void _unroll_simple_for_loop(BB *w, size_t cnt, Reg i, Reg l, Reg r,
                               CmpOp op) {
    assert(cnt >= 2);
    assert(op.less);
    dbg(">>> unroll: ", i, ' ', l, ' ', r, ' ', op.name(), '\n');
    auto &wi = S.loop_info.at(w);
    dbg("instr_cnt: ", wi.instr_cnt, "\n");
    dbg("BB_cnt: ", wi.bbs.size(), "\n");
    std::deque<LoopCopyTool> loops;
    loops.emplace_back(wi.bbs, w, w, S.f);
    for (size_t i = 1; i <= cnt; ++i) {
      loops.emplace_back(loops[0]);
      loops.back().copy(std::string(":") + std::to_string(i) + ":");
    }
    auto &p0 = loops[0];
    auto &p3 = loops[cnt - 1];
    auto &p4 = loops[cnt];
    BB *next = p0.get_exit_next();
    for (size_t i = 1; i < cnt; ++i) {
      auto &p2 = loops[i];
      p2.no_exit();
      p2.entry_del_edge(0);
    }
    for (size_t i = 1; i < cnt; ++i) {
      auto &p1 = loops[i - 1];
      auto &p2 = loops[i];
      p1.back_edge_to(p2.entry);
      p2.change_to(p1, p2.entry);
    }
    p3.back_edge_to(p0.entry);
    p0.change_to(p3, p0.entry);
    p0.exit->back()->map_BB(partial_map(next, p4.entry));
    p4.entry->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        size_t n = 0;
        for (auto &[r, bb] : phi->uses) {
          if (!p4.bbs_rev.count(bb)) {
            r = p4.regs_rev.at(phi->d1);
            bb = p0.exit;
            ++n;
          }
        }
        assert(n == 1);
      }
    });

    p0.change_to(p4, next);

    umap<BB *, BB *> bbs_rev;
    for (auto &p : loops)
      bbs_rev.insert(BE(p.bbs_rev));
    S.f->for_each([&](BB *bb) {
      if (!bbs_rev.count(bb)) {
        bb->map_use(partial_map(p4.regs));
      }
    });
    p0.entry->disable_unroll = 1;
    p4.entry->disable_unroll = 1;
    p4.entry->disable_parallel = 1;

    size_t n = 0;
    w->for_each([&](Instr *x) {
      Case(BranchInstr, br, x) {
        CodeGen cg(S.f);
        if (op.eq) {
          br->cond = (cg.reg(i) + cg.lc(cnt) <= cg.reg(r)).r;
        } else {
          br->cond = (cg.reg(i) + cg.lc(cnt) < cg.reg(r)).r;
        }
        w->ins(std::move(cg.instrs));
        ++n;
      }
    });
    assert(n == 1);
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
    auto &p3 = loops[cnt];
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
    if (w->disable_unroll)
      return 0;
    auto &wi = S.loop_info.at(w);
    if (wi.nested_cnt != 1)
      return 0;
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
    PassDisabled("unroll-fixed") return 0;
    auto op = ind->op;
    // for(i=l;i<r;i{op}=step);
    int32_t i0 = *l, i1 = *r, i2 = *step;
    dbg("for(i=", i0, ";i", wi.cond->name(), i1, ";i=i", BinaryOp(op), i2,
        "){...}\n");
    size_t cnt = 0;
    size_t MAX_UNROLL = parseIntArg(32, "max-unroll"),
           MAX_UNROLL_INSTR = parseIntArg(1024, "max-unroll-instr");
    while (cnt <= MAX_UNROLL && wi.cond->compute(i0, i1)) {
      i0 = std::get<int32_t>(typed_compute(op, i0, i2));
      cnt += 1;
    }
    dbg("cnt=", cnt, "  instr_cnt=", wi.instr_cnt, '\n');
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
      after_unroll(S.f);
      return 1;
    }
    return 0;
  }
  bool unroll_simple_for_loop(BB *w) {
    constexpr size_t MAX_UNROLL_SIMPLE_FOR_INSTR = 256;
    constexpr size_t UNROLL_SIMPLE_FOR_CNT = 3;
    if (w->disable_unroll || !last)
      return 0;
    auto &wi = S.loop_info.at(w);
    if (wi.nested_cnt != 1)
      return 0;
    if (wi.instr_cnt > MAX_UNROLL_SIMPLE_FOR_INSTR)
      return 0;
    auto ilr = S.get_ilr(w);
    if (!ilr)
      return 0;
    auto [i, l, r, op] = *ilr;
    if (!(op.less))
      return 0;
    // for(i=l;i op r;++i);
    PassDisabled("unroll-for") return 0;
    bool dbg_on(global_config.args["dbg-unroll"] == "1");
    if (dbg_on)
      print_cfg(S.f);
    _unroll_simple_for_loop(w, UNROLL_SIMPLE_FOR_CNT, i, l, r, op);
    if (dbg_on) {
      print_cfg(S.f);
      dbg("\n```cpp\n", *S.f, "\n```\n");
    }
    after_unroll(S.f);
    if (dbg_on) {
      print_cfg(S.f);
      dbg("\n```cpp\n", *S.f, "\n```\n");
    }
    return 1;
  }
};

struct LoadToRegV2 : ForwardLoopVisitor<std::map<AddrExpr, Reg>>,
                     CounterOutput {
  using ForwardLoopVisitor::map_t;
  ArrayReadWrite &arw;
  LoadToRegV2(ArrayReadWrite &_arw) : CounterOutput("LoadToRegV2"), arw(_arw) {}
  void update(map_t &m, AddrExpr &mw) {
    if (m.size() >= 40 || mw.bad_index()) {
      m.clear();
    } else {
      m.erase(mw);
    }
  }
  void visitBB(BB *bb) {
    // std::cerr << bb->name << " visited" << '\n';
    auto &w = info[bb];
    w.out = w.in;
    if (w.is_loop_head) {
      w.out.clear();
    }
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      replace_reg(x);
      Case(LoadInstr, ld, x) {
        auto &key = arw.reg_info.at(ld->addr).addr;
        // dbg(*ld, " :::: ", key, '\n');
        if (key.bad_index()) {
        } else if (w.out.count(key)) {
          replace_reg(it, ld->d1, w.out[key]);
          ++cnt;
          // dbg(">>> L2R2\n");
        } else {
          w.out[key] = ld->d1;
        }
      }
      else Case(StoreInstr, st, x) {
        auto &key = arw.reg_info.at(st->addr).addr;
        // dbg(*st, " :::: ", key, '\n');
        update(w.out, key);
        w.out[key] = st->s1;
      }
      else Case(CallInstr, call, x) {
        if (!call->no_store) {
          w.out.clear();
        }
      }
    }
  }
};

struct LoopOps {
  NormalFunc *f;
  bool last;
  CompileUnit *ir;
  LoopOps(NormalFunc *_f, bool _last, CompileUnit *_ir)
      : f(_f), last(_last), ir(_ir) {
    parallel_only = last;
  }
  bool parallel_only;
  bool run() {
    DAG_IR dag(f);
    FindLoopVar S(f);
    dag.visit(S);
    ArrayReadWrite a(S);
    dag.visit(a);
    UnrollLoop w(S, a, last, ir);
    if (w.apply(parallel_only))
      return 1;
    if (parallel_only) {
      parallel_only = 0;
      return 1;
    }
    return 0;
  }
  void run_last() {
    DAG_IR dag(f);
    FindLoopVar S(f);
    dag.visit(S);
    ArrayReadWrite a(S);
    dag.visit(a);
    {
      LoadToRegV2 w(a);
      dag.visit(w);
    }
  }
};

void change_array_access_pattern(CompileUnit *ir, NormalFunc *f) {
  if (ir->funcs.size() == 1) {
    DAG_IR dag(f);
    FindLoopVar S(f);
    dag.visit(S);
    ArrayReadWrite a(S);
    dag.visit(a);
    umap<MemObject *, std::vector<std::pair<Reg, AddrExpr>>> addrs;
    auto &wi = a.loop_info.at(nullptr);
    for (auto &rws : {wi.rs, wi.ws}) {
      for (Reg r : rws) {
        auto &addr = a.reg_info.at(r).addr;
        if (addr.bad_index())
          return;
        addrs[addr.base].emplace_back(r, addr);
      }
    }
    bool upd = 0;
    for (auto &[k, v] : addrs) {
      bool flag = 1;
      std::optional<int32_t> dim;
      for (auto &[r, addr] : v) {
        if (addr.indexs.size() != 1) {
          flag = 0;
          break;
        }
        auto &[k1, v1] = *addr.indexs.begin();
        if (k1 != 4) {
          flag = 0;
          break;
        }
        for (auto &[k2, v2] : v1.cs) {
          if (k2.size() > 1) {
            flag = 0;
          }
          if (k2.size() == 1) {
            auto &[k3, v3] = *k2.begin();
            if (v3 != 1)
              flag = 0;
            if (v2 != 1) {
              if (!dim)
                dim = v2;
              else if (*dim != v2)
                flag = 0;
            }
          }
        }
      }
      if (!flag || !dim)
        continue;
      int32_t dim_ = *dim;
      int32_t dim_0 = k->size / (4 * dim_);
      dbg(k->name, " dim: ", dim_, '\n');
      umap<Reg, std::pair<AddExpr, AddExpr>> rewrite;
      for (auto &[r, addr] : v) {
        auto &[k1, v1] = *addr.indexs.begin();
        AddExpr a1, a2;
        for (auto &[k2, v2] : v1.cs) {
          if (k2.size() == 1) {
            auto &[k3, v3] = *k2.begin();
            if (v2 == dim_) {
              a1.add_eq(k3, 1);
            } else {
              a2.add_eq(k3, v2);
            }
          } else {
            a2.add_eq(v2);
          }
        }
        auto get_range = [&](AddExpr &e) -> std::pair<int32_t, int32_t> {
          int l0 = e.c, r0 = e.c;
          for (auto [reg, k] : e.cs) {
            auto &ri = a.reg_info.at(reg);
            auto [l, r] = ri.get_range();
            if (INT_MIN < l && l <= r && r < INT_MAX) {
              l0 += l * k;
              r0 += r * k;
            } else {
              flag = 0;
            }
          }
          return {l0, r0};
        };
        auto [a1l, a1r] = get_range(a1);
        auto [a2l, a2r] = get_range(a2);
        int c = a2l / dim_;
        if (a2l - c * dim_ < 0) {
          c -= 1;
        }
        a1.add_eq(c);
        a1l += c;
        a1r += c;
        a2l -= c * dim_;
        a2r -= c * dim_;
        a2.add_eq(-c * dim_);
        if (!(0 <= a1l && a1l <= a1r && a1r < dim_0))
          flag = 0;
        if (!(0 <= a2l && a2l <= a2r && a2r < dim_))
          flag = 0;
        if (!flag) {
          // dbg("bad\n");
          break;
        }
        rewrite[r] = {a1, a2};
        dbg(addr, " => ", a1, " ", a2, "\n");
      }
      if (!flag)
        continue;
      dbg(k->name, ": ", dim_0, "*", dim, '\n');
      upd = 1;
      f->for_each([&](BB *bb) {
        bb->for_each([&](Instr *x) {
          Case(ArrayIndex, ai, x) {
            if (rewrite.count(ai->d1)) {
              auto [a1, a2] = rewrite[ai->d1];
              Reg r1 = ai->s1;
              Reg r2 = f->new_Reg();
              bb->ins(a1.genIR(r2, f));
              Reg r3 = f->new_Reg();
              bb->ins(new ArrayIndex(r3, r1, r2, dim_ * 4, -1));
              Reg r4 = f->new_Reg();
              bb->ins(a2.genIR(r4, f));
              bb->replace(new ArrayIndex(ai->d1, r3, r4, 4, -1));
            }
          }
        });
      });
    }
    if (upd) {
      after_unroll(f);
    }
  }
}

void remove_branch(BB *bb, bool cond);
void remove_branch_by_range(NormalFunc *f) {
  DAG_IR dag(f);
  FindLoopVar S(f);
  dag.visit(S);
  ArrayReadWrite a(S);
  dag.visit(a);
  size_t cnt = 0;
  f->for_each([&](BB *bb) {
    auto &wi = S.loop_info.at(bb);
    if (wi.node->is_loop_head)
      return;
    Case(BranchInstr, br, bb->back()) {
      Case(BinaryOpInstr, bop, S.defs.at(br->cond)) {
        if (auto cmp = CmpExpr::make(bop)) {
          if (!S.get_const(cmp->s2))
            cmp->swap();
          if (auto c = S.get_const(cmp->s2)) {
            auto &ri = a.reg_info.at(cmp->s1);
            auto [l, r] = ri.get_range();
            bool t1 = cmp->compute(l, *c);
            bool t2 = cmp->compute(r, *c);
            if (t1 == t2) {
              remove_branch(bb, t1);
              ++cnt;
            }
          }
        }
      }
    }
  });
  if (cnt) {
    after_unroll(f);
    dbg("RemoveBranchByRange: ", cnt, '\n');
  }
}

void loop_ops(CompileUnit *ir, NormalFunc *f, bool last) {
  PassDisabled("loop-ops") return;
  LoopOps ops(f, last, ir);
  int LOOP_OPS_ITER = parseIntArg(16, "loop-ops-iter");
  if (last) {
    remove_branch_by_range(f);
    change_array_access_pattern(ir, f);
    LOOP_OPS_ITER *= 10;
  }
  for (int T = 0; T < LOOP_OPS_ITER; ++T) {
    if (!ops.run()) {
      dbg("T = ", T, '\n');
      break;
    }
  }
  if (last)
    ops.run_last();
}
