#pragma once
#include "ir/ir.hpp"
using namespace IR;

struct EqContext{
  std::map<Reg,int> mp;
  int32_t step_size(Reg r){
	if(mp.count(r))return mp[r];
	return 0;
  }
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