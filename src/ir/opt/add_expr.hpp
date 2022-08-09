#pragma once
#include "ir/ir.hpp"
using namespace IR;
struct MulAddExpr;
struct EqContext {
  enum Type {
    IND,   // a-b == k*step, k:int, k>0
    RANGE, // |a-b| <= step
    ANY    // a-b == ?
  };
  std::function<std::pair<Type, const MulAddExpr &>(Reg)> at;
};

struct AddExpr : Printable {
  int32_t c = 0;
  bool bad = 0;
  std::map<Reg, int32_t> cs;
  void add_eq(Reg x, int32_t a);
  void print(std::ostream &os) const override;
  void add_eq(const AddExpr &w, int32_t s);
  void set_mul(const AddExpr &w1, const AddExpr &w2);
  std::list<std::unique_ptr<Instr>> genIR(Reg result, NormalFunc *f);
};

struct MulAddExpr : Printable {
  typedef std::map<Reg, int32_t> MulExpr;
  std::map<MulExpr, int32_t> cs;
  bool bad = 0;
  bool operator<(const MulAddExpr &w) const { return cs < w.cs; }
  int32_t get_c() const;
  void add_eq(int32_t a);
  void add_eq(Reg x, int32_t a, int32_t b);
  void add_eq(const MulExpr &w, int32_t a);
  void add_eq(const MulAddExpr &w, int32_t a);
  void set_mul(const MulAddExpr &w1, const MulAddExpr &w2, int32_t a);
  void mul_eq(Reg x, int32_t a);
  void mul_eq(const MulExpr &w, int32_t a);
  void mul_eq(const MulAddExpr &w, int32_t a);
  static int32_t gcd(int32_t w1, int32_t w2);
  static MulExpr gcd(const MulExpr &w1, const MulExpr &w2);
  void gcd_eq(const MulAddExpr &w);
  void print(std::ostream &os) const override;
  bool maybe_eq(const MulAddExpr &w, const EqContext &ctx) const;
  static MulExpr mul(const MulExpr &w1, const MulExpr &w2);
  bool may_gt(const MulAddExpr &w);
};

struct AddrExpr : Printable {
  MemObject *base;
  bool bad = 0;
  std::map<int, MulAddExpr> indexs;
  bool operator<(const AddrExpr &w) const {
    if (base != w.base)
      return base < w.base;
    return indexs < w.indexs;
  }
  void add_eq(int key, const MulAddExpr &w);
  void print(std::ostream &os) const override;
  bool maybe_eq(const AddrExpr &w, const EqContext &ctx) const;
};

struct SimpleIndVar {
  Reg init, step;
  BinaryCompute op;
};

struct SimpleReductionVar {
  Reg init, step;
  BinaryCompute op;
  std::optional<int32_t> mod;
};

std::ostream &operator<<(std::ostream &os, const SimpleIndVar &w);

struct CmpOp {
  bool less, eq;
  void neg() {
    less = !less;
    eq = !eq;
  }
  const char *name() { return less ? (eq ? "<=" : "<") : (eq ? ">=" : ">"); }
  bool compute(int32_t x, int32_t y) {
    return less ? (eq ? x <= y : x < y) : (eq ? x >= y : x > y);
  }
};

struct CmpExpr : CmpOp {
  Reg s1, s2;
  void swap() {
    std::swap(s1, s2);
    less = !less;
  }
  static std::optional<CmpExpr> make(BinaryOpInstr *bop);
  CmpOp op() { return *this; }
};
std::ostream &operator<<(std::ostream &os, const CmpExpr &w);
