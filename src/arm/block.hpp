#pragma once

#include <list>
#include <string>

#include "arm/archinfo.hpp"
#include "ir/ir.hpp"

namespace ARMv7 {

struct Reg {
  int id;
  ScalarType type;

  Reg(int _id, ScalarType _type) : id(_id), type(_type) {}
  inline bool is_machine() const {
    if (type == ScalarType::Int) {
      return id < RegConvention<ScalarType::Int>::Count;
    } else {
      return id < RegConvention<ScalarType::Float>::Count;
    }
  }
  inline bool is_pseudo() const {
    if (type == ScalarType::Int) {
      return id >= RegConvention<ScalarType::Int>::Count;
    } else {
      return id >= RegConvention<ScalarType::Float>::Count;
    }
  }
  inline bool is_allocable() const {
    if (type == ScalarType::Int) {
      return RegConvention<ScalarType::Int>::allocable(id);
    } else {
      return RegConvention<ScalarType::Float>::allocable(id);
    }
  }
  bool operator<(const Reg &rhs) const {
    if (type != rhs.type)
      return type < rhs.type;
    return id < rhs.id;
  }
  bool operator==(const Reg &rhs) const {
    if (type != rhs.type)
      return false;
    return id == rhs.id;
  }
  bool operator>(const Reg &rhs) const {
    if (type != rhs.type)
      return type > rhs.type;
    return id > rhs.id;
  }
  bool operator!=(const Reg &rhs) const {
    if (type != rhs.type)
      return true;
    return id != rhs.id;
  }
};

std::ostream &operator<<(std::ostream &os, const Reg &reg);

struct MappingInfo;
struct Func;
struct Inst;
struct AsmContext;
struct StackObject;

enum InstCond { Always, Eq, Ne, Ge, Gt, Le, Lt };

struct CmpInfo {
  InstCond cond;
  Reg lhs = Reg(-1, ScalarType::Int);
  Reg rhs = Reg(-1, ScalarType::Int);
  bool is_float;
};

struct Block {
  std::string name;
  bool label_used;
  int depth;
  int thread_id;
  std::list<std::unique_ptr<Inst>> insts;
  std::vector<Block *> in_edge, out_edge;
  std::set<Reg> live_use, def, live_in, live_out;

  Block(std::string _name);
  void construct(IR::BB *ir_bb, Func *func, MappingInfo *info,
                 Block *next_block, std::map<Reg, CmpInfo> &cmp_info);
  void push_back(std::unique_ptr<Inst> inst);
  void push_back(std::list<std::unique_ptr<Inst>> inst_list);
  void insert_before_jump(std::unique_ptr<Inst> inst);
  void gen_asm(std::ostream &out, AsmContext *ctx);
  void print(std::ostream &out);

  decltype(insts)::iterator _it;
  void for_each(std::function<void(Inst *)> f) {
    for_each_until([&](Inst *x) -> bool {
      f(x);
      return 0;
    });
  }
  decltype(_it) cur_iter() { return std::prev(_it); }
  void replace(Inst *x) { *std::prev(_it) = std::unique_ptr<Inst>(x); }
  bool _del = 0;
  void move() {
    (void)std::prev(_it)->release();
    del();
  }
  void del() { _del = 1; }
  void ins(Inst *x) { insts.insert(std::prev(_it), std::unique_ptr<Inst>(x)); }
  void ins(decltype(insts) &&ls) {
    for (auto &x : ls) {
      ins(x.release());
    }
    ls.clear();
  }
  void replace(decltype(insts) &&ls) {
    ins(std::move(ls));
    del();
  }
  bool for_each_until(std::function<bool(Inst *)> f) {
    for (_it = insts.begin(); _it != insts.end();) {
      auto it0 = _it;
      ++_it;
      _del = 0;
      bool ret = f(it0->get());
      if (_del) {
        insts.erase(it0);
      }
      if (ret)
        return 1;
    }
    return 0;
  }
};

struct MappingInfo {
  std::map<IR::MemObject *, StackObject *> obj_mapping;
  std::map<int, Reg> reg_mapping;
  std::map<IR::BB *, Block *> block_mapping;
  std::map<Block *, IR::BB *> rev_block_mapping;
  std::map<int, int32_t> constant_value;
  int reg_n;

  MappingInfo();

  Reg new_reg();
  Reg from_ir_reg(IR::Reg ir_reg);

  std::set<Reg> float_regs;
  std::map<Reg, std::vector<Reg>> maybe_float_assign;
  void set_float(Reg reg);
  void set_maybe_float_assign(Reg &r1, Reg &r2);
};

} // namespace ARMv7
