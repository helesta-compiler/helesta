#pragma once
#include "ir/ir.hpp"
using namespace IR;

struct EqContext {
  enum Type {
    IND, // a-b == k*step, k:int, k>0
    ANY  // a-b == ?
  };
  std::function<std::pair<Type, int32_t>(Reg)> at;
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
  bool maybe_eq(const AddExpr &w, const EqContext &ctx) const;
};

struct AddrExpr : Printable {
  MemObject *base;
  bool bad = 0;
  std::map<int, AddExpr> indexs;
  void add_eq(int key, const AddExpr &w);
  void print(std::ostream &os) const override;
  bool maybe_eq(const AddrExpr &w, const EqContext &ctx) const;
};

struct SimpleIndVar {
  Reg init, step;
  BinaryCompute op;
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
