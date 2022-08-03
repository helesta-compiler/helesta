#include "ir/opt/dag_ir.hpp"

struct CodeReorder : SimpleLoopVisitor {
  std::vector<std::unique_ptr<BB>> bbs;
  void visitBB(BB *bb) { bbs.emplace_back(bb); }
  void apply(NormalFunc *f) {
    for (auto &x : f->bbs)
      x.release();
    f->bbs = std::move(bbs);
  }
};

struct CondProp : ForwardLoopVisitor<std::map<std::pair<Reg, BB *>, int32_t>> {
  using ForwardLoopVisitor::map_t;
  CondProp() {}
  ~CondProp() {
    if (cnt) {
      ::info << "CondProp: " << cnt << '\n';
    }
  }
  size_t cnt = 0;
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
        auto target = (v ? br->target1 : br->target0);
        bb->pop();
        bb->push(new JumpInstr(target));
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
        auto target = lc->value ? br->target1 : br->target0;
        bb->pop();
        bb->push(new JumpInstr(target));
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
  f->for_each([&](BB *bb) {
    if (prev[bb].size() != 1)
      return;
    BB *bb0 = prev[bb][0];
    Case(JumpInstr, _, bb0->back()) { (void)_; }
    else return;
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
  });
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
  std::unordered_map<std::pair<BB *, BB *>, std::vector<std::pair<Reg, Reg>>>
      movs;
  f->for_each([&](BB *bb) {
    bb->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        for (auto &[r, bb0] : phi->uses) {
          if (r != phi->d1)
            movs[{bb0, bb}].emplace_back(r, phi->d1);
        }
        bb->move();
      }
    });
  });
  for (auto &[edge, P] : movs) {
    auto [bb0, bb] = edge;
    BB *bb1 = f->new_BB();
    bb0->back()->map_BB(partial_map(bb, bb1));
    bb1->push(new JumpInstr(bb));
    auto emit_copy = [&](Reg a, Reg b) {
      bb1->push1(new UnaryOpInstr(b, a, UnaryCompute::ID));
    };
    Reg n = f->new_Reg();
    std::vector<std::pair<Reg, Reg>> res;
    std::map<Reg, std::optional<Reg>> loc, pred;
    std::vector<Reg> to_do, ready;
    for (auto [a, b] : P) {
      loc[a] = a;
      pred[b] = a;
      to_do.push_back(b);
    }
    for (auto [a, b] : P) {
      if (!loc[b])
        ready.push_back(b);
    }
    while (!to_do.empty()) {
      while (!ready.empty()) {
        Reg b = ready.back();
        ready.pop_back();
        Reg a = pred[b].value();
        Reg c = loc[a].value();
        emit_copy(c, b);
        loc[a] = b;
        if (a == c && pred[a])
          ready.push_back(a);
      }
      Reg b = to_do.back();
      to_do.pop_back();
      if (b == loc[pred[b].value()].value()) {
        emit_copy(b, n);
        loc[b] = n;
        ready.push_back(b);
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
  f->for_each([&](BB *bb) {
    bb->for_each([&](Instr *x) {
      Case(PhiInstr, phi, x) {
        remove_if_vec(phi->uses, [&](const std::pair<Reg, BB *> &w) {
          return !used(w.second);
        });
      }
    });
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
    bb->push(new LoadConst<int32_t>(r, 0));
    bb->push(new ReturnInstr(r, 1));
  }
}
void DAG_IR_ALL::remove_unused_BB() {
  PassDisabled("rub") return;
  ir->for_each([&](NormalFunc *f) {
    PassEnabled("sb") simplify_branch(f);
    ::remove_unused_BB(f);
    PassEnabled("rtb") {
      code_reorder(f);
      remove_trivial_BB(f);
    }
    typed &= type_check(f);
  });
}