#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"
using namespace IR;

void dec_use_count(std::map<Reg, int> &f, Instr *x) {
  x->map_use([&](Reg r) { f[r] -= 1; });
}
void inc_use_count(std::map<Reg, int> &f, Instr *x) {
  x->map_use([&](Reg r) { f[r] += 1; });
}
std::map<Reg, int> build_use_count(NormalFunc *func) {
  std::map<Reg, int> f;
  func->for_each([&](Instr *x) { inc_use_count(f, x); });
  return f;
}

struct CodeGen {
  std::list<std::unique_ptr<Instr>> instrs;
  NormalFunc *f;
  CodeGen(NormalFunc *_f) : f(_f) {}
  struct RegRef {
    Reg r;
    CodeGen *cg;
    friend RegRef operator-(RegRef a) {
      Reg r = a.cg->f->new_Reg();
      a.cg->instrs.emplace_back(new UnaryOpInstr(r, a.r, UnaryCompute::NEG));
      return a.cg->reg(r);
    }
    void assign(RegRef a) {
      cg->instrs.emplace_back(new UnaryOpInstr(r, a.r, UnaryCompute::ID));
    }
    void set_last_def(RegRef a) {
      if (cg->instrs.size()) {
        Case(RegWriteInstr, rw, cg->instrs.back().get()) {
          if (rw->d1 == a.r) {
            rw->d1 = r;
          }
          return;
        }
      }
      assign(a);
    }
#define bop(op, name)                                                          \
  friend RegRef operator op(RegRef a, RegRef b) {                              \
    Reg r = a.cg->f->new_Reg();                                                \
    a.cg->instrs.emplace_back(                                                 \
        new BinaryOpInstr(r, a.r, b.r, BinaryCompute::name));                  \
    return a.cg->reg(r);                                                       \
  }
    bop(+, ADD) bop(-, SUB) bop(*, MUL) bop(/, DIV) bop(%, MOD)
#undef bop
  };
  RegRef reg(Reg r) { return RegRef{r, this}; }
  RegRef lc(int32_t x) {
    Reg r = f->new_Reg();
    instrs.emplace_back(new LoadConst<int32_t>(r, x));
    return reg(r);
  }
};

struct AddExpr : Printable {
  int32_t c = 0;
  bool bad = 0;
  std::map<Reg, int32_t> cs;
  void add_eq(Reg x, int32_t a) {
    if (bad)
      return;
    int32_t &v = cs[x];
    v += a;
    if (!v)
      cs.erase(x);
    if (cs.size() > 20) {
      cs.clear();
      bad = 1;
    }
  }
  void print(std::ostream &os) const override {
    if (bad) {
      os << "[bad]";
      return;
    }
    os << c;
    for (auto [x, a] : cs) {
      if (a >= 0)
        os << '+';
      if (a == -1)
        os << '-';
      else if (a != 1)
        os << a << '*';
      os << x;
    }
  }
  void add_eq(const AddExpr &w, int32_t s) {
    if (bad)
      return;
    if (w.bad) {
      bad = 1;
      return;
    }
    c += w.c * s;
    for (auto [x, a] : w.cs) {
      add_eq(x, a * s);
    }
  }
  void set_mul(const AddExpr &w1, const AddExpr &w2) {
    if (w1.bad | w2.bad)
      bad = 1;
    else if (w1.cs.empty())
      add_eq(w2, w1.c);
    else if (w2.cs.empty())
      add_eq(w1, w2.c);
    else
      bad = 1;
    // std::cerr << w1 << '*' << w2 << '=' << *this << '\n';
  }
  std::list<std::unique_ptr<Instr>> genIR(Reg result, NormalFunc *f) {
    std::map<int32_t, std::vector<std::pair<Reg, int32_t>>> mp;
    CodeGen cg(f);
    for (auto [x, a] : cs) {
      mp[a > 0 ? a : -a].emplace_back(x, a > 0 ? 1 : -1);
    }
    std::optional<CodeGen::RegRef> sum;
    if (c) {
      sum = cg.lc(c);
    }
    for (auto &[a, xs] : mp) {
      for (auto &x : xs)
        if (x.second == 1) {
          std::swap(x, xs[0]);
          break;
        }
      std::optional<CodeGen::RegRef> reg_xs;
      for (auto [x, s] : xs) {
        auto reg_x = cg.reg(x);
        // std::cerr << "reg_x: " << reg_x.r << '\n';
        if (!reg_xs) {
          if (s == 1) {
            reg_xs = reg_x;
          } else {
            reg_xs = -reg_x;
          }
        } else {
          if (s == 1) {
            reg_xs = *reg_xs + reg_x;
          } else {
            reg_xs = *reg_xs - reg_x;
          }
        }
        // std::cerr << "reg_xs: " << reg_xs->r << '\n';
      }
      if (a != 1) {
        reg_xs = *reg_xs * cg.lc(a);
      }
      if (!sum) {
        sum = *reg_xs;
      } else {
        sum = *sum + *reg_xs;
      }
    }
    if (!sum) {
      sum = cg.lc(0);
    }
    cg.reg(result).set_last_def(*sum);
    return std::move(cg.instrs);
  }
};

struct SimplifyExpr {
  NormalFunc *func;
  std::map<Reg, RegWriteInstr *> defs;
  std::map<Reg, int> use_count;

  struct Expr {
    size_t tree_size = 1;
    bool visited = 0;
    AddExpr add;
  };
  std::map<Reg, Expr> exprs;
  Expr &getExpr(Reg r) {
    if (!defs.count(r)) {
      std::cerr << r << std::endl;
    }
    assert(defs.count(r));
    return getExpr(defs[r]);
  }
  Expr &getExpr(RegWriteInstr *rw, bool is_cur = 0) {
    Reg r = rw->d1;
    auto &w = exprs[r];
    if (w.visited)
      return w;
    w.visited = 1;
    Case(LoadConst<int32_t>, lc, rw) {
      w.add.c = lc->value;
      return w;
    }
    auto set_const = [&]() {
      w.add.add_eq(r, 1);
      w.tree_size = 0;
    };
    if (!is_cur) {
      set_const();
      return w;
    }

    // std::cerr << ">>> " << *rw << std::endl;

    if (use_count[r] == 1) {
      rw->map_use([&](Reg r0) {
        if (use_count[r0] == 1)
          w.tree_size += getExpr(r0).tree_size;
      });
    }

    Case(UnaryOpInstr, uop, rw) {
      switch (uop->op.type) {
      case UnaryCompute::ID:
        w.add = getExpr(uop->s1).add;
        break;
      case UnaryCompute::NEG:
        w.add.add_eq(getExpr(uop->s1).add, -1);
        break;
      default:
        set_const();
        break;
      }
    }
    else Case(BinaryOpInstr, bop, rw) {
      switch (bop->op.type) {
      case BinaryCompute::ADD:
        w.add = getExpr(bop->s1).add;
        w.add.add_eq(getExpr(bop->s2).add, 1);
        break;
      case BinaryCompute::SUB:
        w.add = getExpr(bop->s1).add;
        w.add.add_eq(getExpr(bop->s2).add, -1);
        break;
      case BinaryCompute::MUL: {
        auto &w1 = getExpr(bop->s1).add;
        auto &w2 = getExpr(bop->s2).add;
        w.add.set_mul(w1, w2);
        if (!w1.bad && !w2.bad && w.add.bad) {
          set_const();
        }
        break;
      }
      default:
        set_const();
        break;
      }
    }
    else {
      w.add.add_eq(r, 1);
      w.tree_size = 0;
    }
    // w.add.print(std::cerr);
    // std::cerr << '\n';
    return w;
  }
  void print_tree(RegWriteInstr *rw) {
    rw->map_use([&](Reg r) {
      if (use_count[r] == 1) {
        print_tree(defs[r]);
      }
    });
    std::cerr << *rw << "  " << getExpr(rw).tree_size << "  " << getExpr(rw).add
              << '\n';
  }
  SimplifyExpr(NormalFunc *_func) : func(_func) {
    defs = build_defs(func);
    use_count = build_use_count(func);
    size_t del_cnt = 0, ins_cnt = 0;
    func->for_each([&](BB *bb) {
      // std::cerr << *bb;
      exprs.clear();
      bb->for_each([&](Instr *x) {
        Case(RegWriteInstr, rw, x) { getExpr(rw, 1); }
      });
      std::set<Reg> del;
      for (auto it = bb->instrs.end(); it != bb->instrs.begin();) {
        auto it0 = it;
        --it;
        Instr *x = it->get();
        Case(RegWriteInstr, rw, x) {
          auto &w = getExpr(rw);
          if (w.tree_size != 0 && !use_count[rw->d1]) {
            // std::cerr << "del: " << *rw << std::endl;
            dec_use_count(use_count, rw);
            bb->instrs.erase(it);
            it = it0;
            ++del_cnt;
            continue;
          }
          if (w.add.bad || w.add.cs.count(rw->d1))
            continue;
          auto ir = w.add.genIR(rw->d1, func);
          if (ir.size() < w.tree_size) {
            for (auto &x : ir) {
              Case(RegWriteInstr, rw, x.get()) { defs[rw->d1] = rw; }
              inc_use_count(use_count, x.get());
              bb->instrs.insert(it, std::move(x));
              ++ins_cnt;
            }
            dec_use_count(use_count, rw);
            bb->instrs.erase(it);
            it = std::prev(it0, ir.size());
            ++del_cnt;
          }
        }
      }
    });
    if (del_cnt) {
      ::info << "SimplifyExpr: " << ins_cnt << "/" << del_cnt << '\n';
    }
    /*
func->for_each([&](Instr *x) {
  Case(RegWriteInstr, rw, x) {
    if (use_count[rw->d1] < 1) {
      std::cerr << "to_del: " << *rw << std::endl;
    }
  }
});*/
  }
};

void simplify_expr_func(NormalFunc *func) { SimplifyExpr _(func); }

void simplify_expr(CompileUnit *ir) { ir->for_each(simplify_expr_func); }
