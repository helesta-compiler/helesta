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

struct PrettyPrint : Defs {
  DAG_IR dag;
  std::map<IR::Reg, int> use_count;
  PrettyPrint(NormalFunc *f) : Defs(f), dag(f) {
    use_count = build_use_count(f);
    auto reg_names_0 = f->reg_names;
    SetPrintContext _(f);
    print_ctx.disable_reg_id = 1;
    (void)_;
    dfs(nullptr);
    f->reg_names = reg_names_0;
    print_ctx.disable_reg_id = 0;
  }
  std::string tab;

  std::unordered_map<Reg, std::string> desc;

  bool print_as_assign(RegWriteInstr *rw) {
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

  std::string get_simple_name(std::string name) {
    auto prefix = f->name + "::";
    if (!name.compare(0, prefix.size(), prefix)) {
      name = name.substr(prefix.size());
    }
    return name;
  }

  void print(Instr *x) {
    Case(RegWriteInstr, rw, x) {
      if (!print_as_assign(rw)) {
        return;
      }
    }
    x->map_use([&](Reg &r) { f->reg_names[r.id] = get_desc(r); });
    dbg(tab, *x, '\n');
  }
  void print(BB *w) {
    dbg(tab, get_simple_name(w->name), ":\n");
    w->for_each([&](Instr *x) { print(x); });
  }
  void dfs(BB *w) {
    auto wi = dag.loop_tree.at(w);
    if (wi.is_loop_head) {
      tab += ">>> ";
    }
    auto &dfn = wi.dfn;
    if (w) {
      print(w);
    }
    for (BB *u : dfn) {
      if (u != w) {
        dfs(u);
      }
    }
    if (wi.is_loop_head) {
      tab = tab.substr(0, tab.size() - 4);
    }
  }
};

void pretty_print_func(NormalFunc *f) {
  dbg("```cpp\n");
  dbg(f->name, ": \n");
  dbg(f->scope);
  PrettyPrint w(f);
  dbg("\n```\n");
}

void pretty_print(CompileUnit *ir) {
  dbg(ir->scope);
  ir->for_each(pretty_print_func);
}
