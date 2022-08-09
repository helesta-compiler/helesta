#include "ir/opt/dag_ir.hpp"

template <class T> struct Repeat : Printable {
  const T &x;
  int n;
  Repeat(const T &_x, int _n) : x(_x), n(_n) {}
  void print(std::ostream &os) const override {
    for (int i = 0; i < n; ++i)
      os << x;
  }
};

struct RunOnDestruct {
  std::function<void()> f;
  ~RunOnDestruct() { f(); }
};

struct PrettyPrint : Defs {
  DAG_IR dag;
  std::map<IR::Reg, int> use_count;
  bool pp_phi;
  PrettyPrint(NormalFunc *f) : Defs(f), dag(f) {
    use_count = build_use_count(f);
    pp_phi = (global_config.args["pp-phi"] == "1");
    auto reg_names_0 = f->reg_names;
    SetPrintContext _(f);
    print_ctx.disable_reg_id = 1;
    f->for_each([&](Instr *x) {
      x->map_use([&](Reg &r) { f->reg_names[r.id] = get_desc(r); });
    });
    (void)_;
    dfs(nullptr);
    f->reg_names = reg_names_0;
    print_ctx.disable_reg_id = 0;
  }
  std::string tab;

  std::unordered_map<Reg, std::string> desc;

  bool print_as_assign(RegWriteInstr *rw) {
    Case(PhiInstr, _, rw) {
      (void)_;
      return 1;
    }
    Case(CallInstr, _, rw) {
      (void)_;
      return 1;
    }
    Case(LoadInstr, _, rw) {
      (void)_;
      return 1;
    }
    if (use_count[rw->d1] == 1)
      return 0;
    Case(LoadConst<int32_t>, _, rw) {
      (void)_;
      return 0;
    }
    Case(LoadConst<float>, _, rw) {
      (void)_;
      return 0;
    }
    Case(LoadArg<ScalarType::Int>, _, rw) {
      (void)_;
      return 0;
    }
    Case(LoadArg<ScalarType::Float>, _, rw) {
      (void)_;
      return 0;
    }
    Case(LoadAddr, _, rw) {
      (void)_;
      return 0;
    }
    Case(UnaryOpInstr, uop, rw) {
      if (uop->op.type == UnaryCompute::ID) {
        return 0;
      }
    }
    return 1;
  }

  std::string get_desc(Reg r) {
    if (desc.count(r)) {
      return desc[r];
    }
    auto rw = defs.at(r);
    if (print_as_assign(rw)) {
      return desc[r] = f->get_name(r) + "(" + std::to_string(r.id) + ")";
    }
    Case(LoadConst<int32_t>, lc, rw) {
      return desc[r] = std::to_string(lc->value);
    }
    Case(LoadConst<float>, lc, rw) {
      return desc[r] = std::to_string(lc->value) + "f";
    }
    Case(LoadArg<ScalarType::Int>, la, rw) {
      return desc[r] = "arg" + std::to_string(la->id);
    }
    Case(LoadArg<ScalarType::Float>, la, rw) {
      return desc[r] = "farg" + std::to_string(la->id);
    }
    Case(LoadAddr, la, rw) {
      return get_simple_name(desc[r] = la->offset->name);
    }
    Case(UnaryOpInstr, uop, rw) {
      if (uop->op.type == UnaryCompute::ID) {
        return desc[r] = get_desc(uop->s1);
      }
      return desc[r] = uop->op.get_name() + get_desc(uop->s1);
    }
    Case(BinaryOpInstr, bop, rw) {
      return desc[r] = "(" + get_desc(bop->s1) + bop->op.get_name() +
                       get_desc(bop->s2) + ")";
    }
    Case(ArrayIndex, ai, rw) {
      return desc[r] = get_desc(ai->s1) + "+" + get_desc(ai->s2) + "*" +
                       std::to_string(ai->size);
    }
    dbg(*rw);
    assert(0);
    return "";
  }

  std::string get_simple_name(BB *bb) {
    if (!bb)
      return "(null)";
    return get_simple_name(bb->name);
  }
  std::string get_simple_name(std::string name) {
    auto prefix = f->name + "::";
    if (!name.compare(0, prefix.size(), prefix)) {
      name = name.substr(prefix.size());
    }
    return name;
  }

  void print(Instr *x, BB *next, BB *break_, BB *continue_) {
    Case(PhiInstr, _, x) {
      (void)_;
      if (!pp_phi) {
        return;
      }
    }
    Case(RegWriteInstr, rw, x) {
      if (!print_as_assign(rw)) {
        return;
      }
    }
    Case(JumpInstr, jmp, x) {
      if (jmp->target == next) {
        return;
      }
    }
    dbg(tab);
    RunOnDestruct _{[&]() { dbg('\n'); }};
    auto goto_ = [&](BB *bb) {
      if (bb == break_) {
        dbg("break");
        return;
      }
      if (bb == continue_) {
        dbg("continue");
        return;
      }
      dbg("goto ", get_simple_name(bb->name));
    };
    Case(JumpInstr, jmp, x) {
      goto_(jmp->target);
      return;
    }
    Case(BranchInstr, br, x) {
      if (br->target1 == next) {
        dbg("if not(", br->cond, ") ");
        goto_(br->target0);
        return;
      }
      if (br->target0 == next) {
        dbg("if (", br->cond, ") ");
        goto_(br->target1);
        return;
      }
    }
    dbg(*x);
  }
  void print(BB *w, BB *next, BB *break_, BB *continue_) {
    auto wi = dag.loop_tree.at(w);
    dbg(tab);
    if (wi.is_loop_head) {
      dbg("[loop] ");
    }
    dbg(get_simple_name(w));
    // dbg("{ ", get_simple_name(next), " }");
    // dbg("{ ", get_simple_name(break_), " }");
    // dbg("{ ", get_simple_name(continue_), " }");
    dbg(":\n");
    w->for_each([&](Instr *x) {
      Case(ControlInstr, _, x) {
        (void)_;
        if (!pp_phi) {
          auto out = w->getOutNodes();
          for (BB *u : out) {
            u->for_each([&](Instr *x0) {
              Case(PhiInstr, phi, x0) {
                int cnt = 0;
                for (auto &[r, bb0] : phi->uses) {
                  if (bb0 == w) {
                    dbg(tab, "[phi] ", phi->d1, " = ", r, '\n');
                    ++cnt;
                  }
                }
                assert(cnt == 1);
                assert(out.size() == 1);
              }
            });
          }
        }
      }
      print(x, next, break_, continue_);
    });
  }
  void dfs(BB *w, BB *next = nullptr, BB *break_ = nullptr,
           BB *continue_ = nullptr) {
    auto wi = dag.loop_tree.at(w);
    if (wi.is_loop_head) {
      // dbg(tab, "LOOP: ", get_simple_name(w->name), '\n');
      tab += ">>> ";
      break_ = next;
      continue_ = w;
    }
    auto &dfn = wi.dfn;
    std::map<BB *, size_t> idfn;
    for (auto [bb, id] : enumerate(dfn)) {
      idfn[bb] = id;
    }
    size_t l = 0, r = dfn.size();
    auto reachable_set = [&](BB *bb) {
      std::set<BB *> s;
      s.insert(bb);
      for (size_t i = l; i < r; ++i) {
        bb = dfn.at(i);
        auto &bi = dag.loop_tree.at(bb);
        if (s.count(bb)) {
          auto &out = (bb == w ? bi.out : bi.loop_exit);
          for (BB *bb0 : out) {
            if (idfn.count(bb0) && i < idfn.at(bb0)) {
              s.insert(bb0);
            }
          }
        }
      }
      return s;
    };
    std::vector<std::vector<int>> nested_if;
    nested_if.resize(dfn.size());
    auto chech_if_else = [&](BB *bb, BB *next) {
      if (!next)
        return;
      auto &bi = dag.loop_tree.at(bb);
      auto &out = (bb == w ? bi.out : bi.loop_exit);
      if (out.size() != 2)
        return;
      if (!idfn.count(out[0]) || !idfn.count(out[1]))
        return;
      int v0 = 1, v1 = 2;
      if (out[1] == next)
        std::swap(out[0], out[1]);
      if (out[0] != next)
        return;
      auto s0 = reachable_set(out[0]);
      auto s1 = reachable_set(out[1]);
      size_t l0 = idfn.at(out[0]);
      assert(l0 == l + 1);
      size_t l1 = idfn.at(out[1]);
      assert(l0 < l1);
      size_t r0 = l0;
      while (s0.count(dfn.at(r0)) && !s1.count(dfn.at(r0)))
        ++r0;
      size_t r1 = l1;
      while (!s0.count(dfn.at(r1)) && s1.count(dfn.at(r1)))
        ++r1;
      // [l0,r0) [l1,r1)
      remove_if(s0, [&](BB *bb0) {
        auto t = idfn.at(bb0);
        return l0 <= t && t < r0;
      });
      remove_if(s1, [&](BB *bb1) {
        auto t = idfn.at(bb1);
        return l1 <= t && t < r1;
      });
      if (s0 != s1)
        return;
      for (size_t i = l0; i < r0; ++i)
        nested_if.at(i).push_back(v0);
      for (size_t i = l1; i < r1; ++i)
        nested_if.at(i).push_back(v1);
    };
    if (w) {
      print(w, l + 1 < r ? dfn.at(l + 1) : next, break_, continue_);
      chech_if_else(w, l + 1 < r ? dfn.at(l + 1) : nullptr);
    }
    while (l < r) {
      BB *u = dfn.at(l);
      auto tab0 = tab;
      for (int v : nested_if.at(l)) {
        if (v == 1)
          tab += ">0> ";
        else if (v == 2)
          tab += ">1> ";
        else
          assert(0);
      }
      if (u != w) {
        dfs(u, l + 1 < r ? dfn.at(l + 1) : next, break_, continue_);
        chech_if_else(u, l + 1 < r ? dfn.at(l + 1) : nullptr);
      }
      tab = tab0;
      ++l;
    }
    if (wi.is_loop_head) {
      tab = tab.substr(0, tab.size() - 4);
    }
  }
};

void code_reorder(NormalFunc *f);
void before_gcm_func(NormalFunc *f);

void pretty_print_func(NormalFunc *f) {
  before_gcm_func(f);
  code_reorder(f);
  dbg("```cpp\n");
  dbg(f->name, ": \n");
  dbg(f->scope);
  PrettyPrint w(f);
  dbg("\n```\n");
}

void pretty_print(CompileUnit *ir) {
  dbg("```cpp\n");
  dbg(ir->scope);
  dbg("\n```\n");
  ir->for_each(pretty_print_func);
}
