#include "ir/opt/add_expr.hpp"
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

void AddExpr::add_eq(Reg x, int32_t a) {
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
void AddExpr::print(std::ostream &os) const {
  if (bad) {
    os << "[bad]";
    return;
  }

  if (c) {
    os << c;
  } else if (cs.empty()) {
    os << '0';
  }
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
void AddExpr::add_eq(const AddExpr &w, int32_t s) {
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
void AddExpr::set_mul(const AddExpr &w1, const AddExpr &w2) {
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
std::list<std::unique_ptr<Instr>> AddExpr::genIR(Reg result, NormalFunc *f) {
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

// this += a
void MulAddExpr::add_eq(int32_t a) {
  if (bad)
    return;
  MulExpr t;
  add_eq(t, a);
}

// this += (x^a)*b
void MulAddExpr::add_eq(Reg x, int32_t a, int32_t b) {
  if (bad)
    return;
  MulExpr t{{x, a}};
  add_eq(t, b);
}

// this += (w1*w2)^a
void MulAddExpr::set_mul(const MulAddExpr &w1, const MulAddExpr &w2,
                         int32_t a) {
  if (w1.bad || w2.bad) {
    bad = 1;
    return;
  }
  for (auto &[k1, v1] : w1.cs) {
    for (auto &[k2, v2] : w2.cs) {
      add_eq(mul(k1, k2), v1 * v2 * a);
    }
  }
}

// this += w*a
void MulAddExpr::add_eq(const MulExpr &w, int32_t a) {
  if (bad)
    return;
  if (!a)
    return;
  if (!(cs[w] += a)) {
    cs.erase(w);
  }
  if (w.size() >= 4 || cs.size() >= 5) {
    bad = 1;
    cs.clear();
  }
}

// this += w*a
void MulAddExpr::add_eq(const MulAddExpr &w, int32_t a) {
  if (bad || w.bad) {
    bad = 1;
    return;
  }
  for (auto &[k, v] : w.cs) {
    add_eq(k, v * a);
  }
}

// gcd(w1,w2)
int32_t MulAddExpr::gcd(int32_t w1, int32_t w2) {
  return w2 ? gcd(w2, w1 % w2) : w1;
}

// gcd(w1,w2)
MulAddExpr::MulExpr MulAddExpr::gcd(const MulExpr &w1, const MulExpr &w2) {
  MulExpr w0;
  for (auto &[k, v1] : w1) {
    if (w2.count(k)) {
      auto &v2 = w2.at(k);
      w0[k] = std::min(v1, v2);
    }
  }
  return w0;
}

// this = gcd(this,w2)
void MulAddExpr::gcd_eq(const MulAddExpr &w) {
  if (bad || w.bad) {
    bad = 1;
    return;
  }
  std::optional<MulExpr> k0;
  int32_t v0 = 0;
  for (auto &[k, v] : cs) {
    if (k0) {
      k0 = gcd(*k0, k);
    } else {
      k0 = k;
    }
    v0 = gcd(v0, v);
  }
  for (auto &[k, v] : w.cs) {
    if (k0) {
      k0 = gcd(*k0, k);
    } else {
      k0 = k;
    }
    v0 = gcd(v0, v);
  }
  cs.clear();
  if (k0) {
    cs[*k0] = v0;
  }
}

void MulAddExpr::print(std::ostream &os) const {
  if (bad) {
    os << "[bad]";
    return;
  }
  for (auto &[k, v] : cs) {
    os << '+' << v;
    for (auto &[k0, v0] : k) {
      for (int32_t i = 0; i < v0; ++i) {
        os << '*' << k0;
      }
    }
  }
}

int32_t MulAddExpr::get_c() const {
  MulExpr one;
  if (cs.count(one)) {
    return cs.at(one);
  }
  return 0;
}

std::optional<int32_t> MulAddExpr::get_c_if() const {
  int32_t c = get_c();
  if ((c != 0) == cs.size())
    return c;
  return std::nullopt;
}

bool MulAddExpr::maybe_eq(const MulAddExpr &w, const EqContext &ctx) const {
  if (bad || w.bad)
    return 1;
  int32_t c1 = get_c(), c2 = w.get_c();
  if (cs.size() - (c1 != 0) != w.cs.size() - (c2 != 0))
    return 1;
  MulAddExpr dc;
  MulAddExpr step_gcd;
  dc.add_eq(std::abs(c1 - c2));
  for (auto &[k, v1] : cs) {
    if (k.empty())
      continue;
    if (!w.cs.count(k))
      return 1;
    if (v1 != w.cs.at(k))
      return 1;
    for (auto &[k0, v0] : k) {
      //*(k0:Reg^v0:int)
      MulAddExpr coef;
      coef.add_eq(1);
      auto [type, step] = ctx.at(k0);
      std::optional<MulAddExpr> ind_step, range;
      // ind_step*coef*range
      switch (type) {
      case EqContext::IND:
        if (!step.cs.empty()) {
          if (v0 != 1 || ind_step)
            return 1;
          ind_step = step;
        } else {
          coef.mul_eq(k0, v0);
        }
        break;
      case EqContext::RANGE:
        if (v0 != 1 || range)
          return 1;
        range = step;
        break;
      case EqContext::ANY:
        return 1;
      }
      if (ind_step) {
        if (range)
          return 1;
        coef.mul_eq(*ind_step, 1);
        step_gcd.gcd_eq(coef);
      } else if (range) {
        coef.mul_eq(*range, 1);
        dc.add_eq(coef, 1);
      }
    }
  }
  return dc.may_gt(step_gcd);
}

// w1*w2
MulAddExpr::MulExpr MulAddExpr::mul(const MulExpr &w1, const MulExpr &w2) {
  MulExpr w0 = w1;
  for (auto &[k, v] : w2) {
    w0[k] += v;
  }
  return w0;
}

// this *= x^a
void MulAddExpr::mul_eq(Reg x, int32_t a) {
  if (bad)
    return;
  MulExpr t{{x, a}};
  mul_eq(t, 1);
}

// this *= x^a
void MulAddExpr::mul_eq(const MulExpr &w, int32_t a) {
  if (bad)
    return;
  MulAddExpr t;
  t.add_eq(w, 1);
  mul_eq(t, a);
}

// this *= w^a
void MulAddExpr::mul_eq(const MulAddExpr &w1, int32_t a) {
  if (bad)
    return;
  MulAddExpr t;
  t.set_mul(*this, w1, a);
  *this = std::move(t);
}

// maybe this >= w ?
// if w-this>0 is proved, return 0
bool MulAddExpr::may_gt(const MulAddExpr &w) {
  if (bad || w.bad)
    return 1;
  MulAddExpr d = w;
  d.add_eq(*this, -1);
  int32_t c = d.get_c();
  if (d.cs.size() == (c != 0) && c > 0) {
    return 0;
  }
  return 1;
}

void AddrExpr::add_eq(int key, const MulAddExpr &w) {
  if (bad)
    return;
  indexs[key].add_eq(w, 1);
  if (indexs.size() > 3) {
    bad = 1;
  }
}
void AddrExpr::print(std::ostream &os) const {
  if (bad)
    os << "[bad]";
  else {
    os << base->name;
    for (auto &[k, v] : indexs) {
      os << '+' << k << "*(" << v << ')';
    }
  }
}

bool AddrExpr::maybe_eq(const AddrExpr &w, const EqContext &ctx) const {
  if (bad || w.bad)
    return 1;
  if (base != w.base)
    return 0;
  if (indexs.size() != w.indexs.size())
    return 1;
  // dbg(*this, " =? ", w, '\n');
  for (auto &[k, v1] : indexs) {
    if (!w.indexs.count(k))
      return 1;
    auto &v2 = w.indexs.at(k);
    if (!v1.maybe_eq(v2, ctx))
      return 0;
  }
  return 1;
}

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
