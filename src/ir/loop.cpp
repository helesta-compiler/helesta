#include "common/common.hpp"
#include "ir/pass.hpp"

void for_each_phi(BB *w, std::function<void(PhiInstr *)> F) {
  w->for_each([&](Instr *i) { Case(PhiInstr, phi, i) F(phi); });
}

namespace ExprUtil {

struct MulExpr : Printable {
  std::multiset<Reg> terms;
  MulExpr(Reg r) { terms.insert(r); }
  // t1*t2*...*tn*c
  bool operator<(const MulExpr &w) const { return terms < w.terms; }
  MulExpr operator*(const MulExpr &w) const {
    MulExpr res = *this;
    res.terms.insert(w.terms.begin(), w.terms.end());
    return res;
  }
  size_t size() const { return terms.size(); }
  void print(std::ostream &os) const override {
    bool flag = 0;
    for (Reg r : terms) {
      if (flag) os << "*";
      os << r;
      flag = 1;
    }
  }
};

struct AddExpr : Printable {
  std::map<MulExpr, int64_t> terms;
  int64_t c = 0;
  AddExpr() {}
  AddExpr(Reg r) { terms[MulExpr(r)] = 1; }
  AddExpr(int64_t value) { c = value; }
  bool is_zero() const { return !c && terms.empty(); }
  bool is_const() const { return terms.empty(); }
  bool is_const(std::function<bool(Reg)> f) const {
    for (auto &kv : terms) {
      for (Reg r : kv.first.terms)
        if (!f(r)) return 0;
    }
    return 1;
  }
  // t1+t2+...+tn+c
  AddExpr operator+(const AddExpr &w) const { return add(w, 1); }
  AddExpr operator-(const AddExpr &w) const { return add(w, -1); }
  AddExpr operator-() const { return add(AddExpr(), -1); }
  AddExpr operator*(int64_t x) const { return *this * AddExpr(x); }
  AddExpr operator*(const AddExpr &w) const {
    AddExpr res;
    for (auto &kv1 : terms) {
      res.terms[kv1.first] += kv1.second * w.c;
      for (auto &kv2 : w.terms) {
        res.terms[kv1.first * kv2.first] += kv1.second * kv2.second;
      }
    }
    for (auto &kv2 : w.terms) {
      res.terms[kv2.first] += c * kv2.second;
    }
    for (auto it = res.terms.begin(); it != res.terms.end();) {
      if (it->second)
        ++it;
      else {
        auto del = it++;
        res.terms.erase(del);
      }
    }
    res.c += c * w.c;
    return res;
  }
  std::optional<std::pair<AddExpr, AddExpr>> as_linear(Reg r) const {
    std::pair<AddExpr, AddExpr> res;
    for (auto &kv : terms) {
      int x = kv.first.terms.count(r);
      if (x == 0)
        res.first.terms[kv.first] = kv.second;
      else if (x == 1) {
        auto key = kv.first;
        key.terms.erase(r);
        if (key.terms.empty())
          res.second.c = kv.second;
        else
          res.second.terms[key] = kv.second;
      } else
        return std::nullopt;
    }
    res.first.c = c;
    return res;
  }
  AddExpr add(const AddExpr &w, int64_t k) const {
    AddExpr res = *this;
    for (auto &kv : w.terms) {
      if (!(res.terms[kv.first] += kv.second * k)) {
        res.terms.erase(res.terms.find(kv.first));
      }
    }
    res.c += w.c * k;
    return res;
  }
  size_t size() const {
    size_t s = 0;
    for (auto &kv : terms) s += kv.first.size();
    return s;
  }
  void print(std::ostream &os) const override {
    for (auto &kv1 : terms) {
      os << kv1.first;
      if (kv1.second != 1) os << "*" << kv1.second;
      os << " + ";
    }
    if (c || terms.empty()) os << c;
  }
};

struct ArrayIndexExpr : Printable {
  std::vector<std::optional<AddExpr>> terms;
  ArrayIndexExpr operator-(const ArrayIndexExpr &w) const {
    size_t n = terms.size();
    assert(n == w.terms.size());
    ArrayIndexExpr res;
    for (size_t i = 0; i < n; ++i) {
      if (terms[i] && w.terms[i])
        res.terms.emplace_back(*terms[i] - *w.terms[i]);
      else
        res.terms.emplace_back();
    }
    return res;
  }
  // mem[t1][t2]...[tn]
  void print(std::ostream &os) const override {
    for (const auto &x : terms) {
      os << "[";
      if (x)
        os << *x;
      else
        os << "(null)";
      os << "]";
    }
  }
};

}  // namespace ExprUtil

std::unordered_map<BB *, double> estimate_BB_prob(NormalFunc *f) {
  auto S = build_dom_tree(f);
  std::unordered_map<BB *, double> f1, f2;
  f->for_each([&](BB *w) { f1[w] = 1; });
  for (int t = 0; t < 10; ++t) {
    f2.clear();
    const double decay = 0.5, break_prob = 0.15, if_prob = 0.7;
    f->for_each([&](BB *w) {
      BB *cur_loop = S[w].get_loop_rt();
      double p = f1[w];
      f2[w] += p * decay;
      p *= (1 - decay);
      Instr *x = w->back();
      Case(JumpInstr, y, x) { f2[y->target] += p; }
      else Case(BranchInstr, y, x) {
        double p1 = p * 0.5, p0 = p * 0.5;
        bool b1 = in_loop(S, y->target1, cur_loop);
        bool b0 = in_loop(S, y->target0, cur_loop);
        if (b1 != b0) {
          if (b1)
            p1 = p * (1 - break_prob), p0 = p * break_prob;
          else
            p1 = p * break_prob, p0 = p * (1 - break_prob);
        } else if (0) {
          b1 = (y->target1->instrs.empty());
          b0 = (y->target1->instrs.empty());
          if (b1 != b0) {
            if (b1)
              p1 = p * (1 - if_prob), p0 = p * if_prob;
            else
              p1 = p * if_prob, p0 = p * (1 - if_prob);
          }
        }
        f2[y->target1] += p1;
        f2[y->target0] += p0;
      }
      else Case(ReturnInstr, y, x) {
        f2[f->entry] += p;
      }
      else assert(0);
    });
    f1 = std::move(f2);
  }
  return f1;
}

void seperate_loops(NormalFunc *f) {
  auto S = build_dom_tree(f);
  f->for_each([&](BB *w) {
    Case(BranchInstr, x0, w->back()) {
      if (S[x0->target1].loop_depth > S[w].loop_depth) assert(0);
      if (S[x0->target0].loop_depth > S[w].loop_depth) assert(0);
    }
  });
}

void loop_tree_dfs(DomTree &S, NormalFunc *f, std::function<void(BB *)> F) {
  std::vector<BB *> bbs;
  f->for_each([&](BB *w) {
    auto &sw = S[w];
    if (!sw.loop_rt) return;
    if (sw.loop_fa) return;
    bbs.push_back(w);
  });
  std::function<void(BB *)> dfs;
  dfs = [&](BB *bb) {
    F(bb);
    for (BB *ch : S[bb].loop_ch)
      if (S[ch].loop_rt) dfs(ch);
  };
  for (BB *bb : bbs) dfs(bb);
}
