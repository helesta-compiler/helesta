#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "arm/archinfo.hpp"
#include "common/common.hpp"
#include "ir/ir.hpp"
#include "ir/scalar.hpp"

namespace ARMv7 {

struct Block;
struct Func;
struct AsmContext;

struct Reg {
  int id;
  ScalarType type;

  Reg(int _id = -1, ScalarType _type = ScalarType::Int)
      : id(_id), type(_type) {}
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
  bool operator<(const Reg &rhs) const { return id < rhs.id; }
  bool operator==(const Reg &rhs) const { return id == rhs.id; }
  bool operator>(const Reg &rhs) const { return id > rhs.id; }
  bool operator!=(const Reg &rhs) const { return id != rhs.id; }
};

std::ostream &operator<<(std::ostream &os, const Reg &reg);

enum InstCond { Always, Eq, Ne, Ge, Gt, Le, Lt };

InstCond logical_not(InstCond c);

InstCond reverse_operand(InstCond c);

std::ostream &operator<<(std::ostream &os, const InstCond &cond);

InstCond from_ir_binary_op(IR::BinaryCompute op);

struct StackObject {
  int32_t size, position;
};

struct GlobalObject {
  std::string name;
  int size;   // only available when is_int
  void *init; // when !is_int, must not empty
  bool is_const;
  ScalarType scalar_type;
};

struct Shift {
  enum Type {
    LSL, // logical shift left
    LSR, // logical shift right
    ASR, // arithmetic shift right
    ROR, // rotate right
  } type;
  int w;

  Shift() : type(LSL), w(0) {} // default construct to no shifting
  Shift(Type _t, int _w) : type(_t), w(_w) {}

  static bool legal_width(int w) { return w >= 0 && w < 32; }
};
std::ostream &operator<<(std::ostream &os, const Shift &shift);

struct Inst {
  InstCond cond;

  Inst() : cond(Always) {}
  virtual ~Inst() = default;

  virtual std::vector<Reg> def_reg() { return {}; }
  virtual std::vector<Reg> use_reg() { return {}; }
  virtual std::vector<Reg *>
  regs() = 0; // only modifiable register. for example, r0 used by return
              // instruction is not included. pointers returned by Push::regs()
              // is invalidated when Push::src is modified
  virtual bool change_cpsr() { return false; }
  virtual bool side_effect() {
    return false;
  } // side effect apart from assigning value to def_reg() registers and
    // change_cpsr
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) = 0;
  virtual void print(std::ostream &out) { gen_asm(out, nullptr); }
  virtual void maintain_sp(int32_t &) {}

  std::string to_string() {
    std::ostringstream buf;
    print(buf);
    return buf.str();
  }
  bool use_cpsr() { return cond != Always; }
  template <class T> T *as() { return dynamic_cast<T *>(this); }
  void update_live(std::set<Reg> &live) {
    for (Reg i : def_reg())
      if (i.is_pseudo() || i.is_allocable())
        live.erase(i);
    for (Reg i : use_reg())
      if (i.is_pseudo() || i.is_allocable())
        live.insert(i);
  }
  bool def(Reg reg) {
    for (Reg r : def_reg())
      if (r == reg)
        return true;
    return false;
  }
  bool use(Reg reg) {
    for (Reg r : use_reg())
      if (r == reg)
        return true;
    return false;
  }
  bool relate(Reg reg) { return def(reg) || use(reg); }
  void replace_reg(Reg before, Reg after) {
    for (Reg *i : regs())
      if ((*i) == before)
        (*i).id = after.id;
  }
};

std::unique_ptr<Inst> set_cond(std::unique_ptr<Inst> inst, InstCond cond);
std::list<std::unique_ptr<Inst>> set_cond(std::list<std::unique_ptr<Inst>> inst,
                                          InstCond cond);
void insert(std::list<std::unique_ptr<Inst>> &inserted_list,
            std::list<std::unique_ptr<Inst>>::iterator pos,
            std::list<std::unique_ptr<Inst>> inst);
void replace(std::list<std::unique_ptr<Inst>> &inserted_list,
             std::list<std::unique_ptr<Inst>>::iterator pos,
             std::list<std::unique_ptr<Inst>> inst);

// (reg, reg) -> reg
// shift is invalid for mul and div
struct RegRegInst : Inst {
  enum Type { Add, Sub, Mul, Div, RevSub, Mod, And } op;
  Reg dst, lhs, rhs;
  Shift shift;
  RegRegInst(Type _op, Reg _dst, Reg _lhs, Reg _rhs)
      : op(_op), dst(_dst), lhs(_lhs), rhs(_rhs) {}
  RegRegInst(Type _op, Reg _dst, Reg _lhs, Reg _rhs, Shift _shift)
      : op(_op), dst(_dst), lhs(_lhs), rhs(_rhs), shift(_shift) {
    if (shift.w != 0)
      assert(op == Add || op == Sub || op == RevSub || op == And);
  }
  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst, lhs, rhs};
    else
      return {lhs, rhs};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &lhs, &rhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;

  static Type from_ir_binary_op(IR::BinaryCompute op) {
    switch (op) {
    case IR::BinaryCompute::ADD:
      return Add;
    case IR::BinaryCompute::SUB:
      return Sub;
    case IR::BinaryCompute::MUL:
      return Mul;
    case IR::BinaryCompute::DIV:
      return Div;
    case IR::BinaryCompute::MOD:
      return Mod;
    default:
      unreachable();
      return Add;
    }
  }
};

struct FRegRegInst : Inst {
  enum Type { Add, Sub, Mul, Div } op;
  Reg dst, lhs, rhs;
  FRegRegInst(Type _op, Reg _dst, Reg _lhs, Reg _rhs)
      : op(_op), dst(_dst), lhs(_lhs), rhs(_rhs) {}
  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst, lhs, rhs};
    else
      return {lhs, rhs};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &lhs, &rhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;

  static Type from_ir_binary_op(IR::BinaryCompute op) {
    switch (op) {
    case IR::BinaryCompute::FADD:
      return Add;
    case IR::BinaryCompute::FSUB:
      return Sub;
    case IR::BinaryCompute::FMUL:
      return Mul;
    case IR::BinaryCompute::FDIV:
      return Div;
    default:
      unreachable();
      return Add;
    }
  }
};

// mla dst, s1, s2, s3: dst = s1 * s2 + s3
// mls dst, s1, s2, s3: dst = s3 - s1 * s2
struct ML : Inst {
  enum Type { Mla, Mls } op;
  Reg dst, s1, s2, s3;
  ML(Type _op, Reg _dst, Reg _s1, Reg _s2, Reg _s3)
      : op(_op), dst(_dst), s1(_s1), s2(_s2), s3(_s3) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst, s1, s2, s3};
    else
      return {s1, s2, s3};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &s1, &s2, &s3}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

struct SMulL : Inst {
  Reg d1, d2, s1, s2;
  SMulL(Reg _d1, Reg _d2, Reg _s1, Reg _s2)
      : d1(_d1), d2(_d2), s1(_s1), s2(_s2) {}

  virtual std::vector<Reg> def_reg() override { return {d1, d2}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {d1, d2, s1, s2};
    else
      return {s1, s2};
  }
  virtual std::vector<Reg *> regs() override { return {&d1, &d2, &s1, &s2}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// (reg, imm) -> reg
// precondition: is_legal_immediate(rhs)
struct RegImmInst : Inst {
  enum Type { Add, Sub, RevSub, Lsl, Lsr, Asr, Bic } op;
  Reg dst, lhs;
  int32_t rhs;
  RegImmInst(Type _op, Reg _dst, Reg _lhs, int32_t _rhs)
      : op(_op), dst(_dst), lhs(_lhs), rhs(_rhs) {}
  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst, lhs};
    else
      return {lhs};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &lhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// dst = src
struct MoveReg : Inst {
  Reg dst, src;
  MoveReg(Reg _dst, Reg _src) : dst(_dst), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst, src};
    else
      return {src};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &src}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// dst = op src
struct FRegInst : Inst {
  enum Type { Neg, I2F, F2I, F2D0, F2D1 } op;
  Reg dst, src;
  FRegInst(Type _op, Reg _dst, Reg _src) : op(_op), dst(_dst), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {src}; }
  virtual std::vector<Reg *> regs() override { return {&dst, &src}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// dst = shift(src)
struct ShiftInst : Inst {
  Reg dst, src;
  Shift shift;
  ShiftInst(Reg _dst, Reg _src, Shift _shift)
      : dst(_dst), src(_src), shift(_shift) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst, src};
    else
      return {src};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &src}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// Mov: dst = src
// Mvn: dst = ~src
// precondition: is_legal_immediate(src)
struct MoveImm : Inst {
  enum Type { Mov, Mvn } op;
  Reg dst;
  int32_t src;
  MoveImm(Type _op, Reg _dst, int32_t _src) : op(_op), dst(_dst), src(_src) {
    assert(dst.type == ScalarType::Int);
  }

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst};
    else
      return {};
  }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// top half word of dst = 0
// bottom half word of dst = src
// precondition: 0 <= src < 65536
struct MoveW : Inst {
  Reg dst;
  int32_t src;
  MoveW(Reg _dst, int32_t _src) : dst(_dst), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst};
    else
      return {};
  }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// top half word of dst = src
// bottom half word of dst remains
// precondition: 0 <= src < 65536
struct MoveT : Inst {
  Reg dst;
  int32_t src;
  MoveT(Reg _dst, int32_t _src) : dst(_dst), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

struct LoadSymbolAddrLower16 : Inst {
  Reg dst;
  std::string symbol;
  LoadSymbolAddrLower16(Reg _dst, std::string _symbol)
      : dst(_dst), symbol(std::move(_symbol)) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst};
    else
      return {};
  }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

struct LoadSymbolAddrUpper16 : Inst {
  Reg dst;
  std::string symbol;
  LoadSymbolAddrUpper16(Reg _dst, std::string _symbol)
      : dst(_dst), symbol(std::move(_symbol)) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

std::list<std::unique_ptr<Inst>> load_imm(Reg dst, int32_t imm);
void load_imm_asm(std::ostream &out, Reg dst, int32_t imm, InstCond cond);
std::list<std::unique_ptr<Inst>> reg_imm_sum(Reg dst, Reg lhs,
                                             int32_t rhs); // dst != lhs
std::list<std::unique_ptr<Inst>> load_symbol_addr(Reg dst,
                                                  const std::string &symbol);

bool load_store_offset_range(int32_t offset);
bool load_store_offset_range(int64_t offset);
// check if offset is in [-4095, 4095]

// dst = [base + offset_imm]
// precondition: load_store_offset_range(offset_imm)
struct Load : Inst {
  Reg dst, base;
  int32_t offset_imm;
  Load(Reg _dst, Reg _base, int32_t _offset_imm)
      : dst(_dst), base(_base), offset_imm(_offset_imm) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {base, dst};
    else
      return {base};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &base}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// [base + offset_imm] = src
// precondition: load_store_offset_range(offset_imm)
struct Store : Inst {
  Reg src, base;
  int32_t offset_imm;
  Store(Reg _src, Reg _base, int32_t _offset_imm)
      : src(_src), base(_base), offset_imm(_offset_imm) {}

  virtual std::vector<Reg> use_reg() override { return {src, base}; }
  virtual bool side_effect() override { return true; }
  virtual std::vector<Reg *> regs() override { return {&src, &base}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// dst = [base + offset]
struct ComplexLoad : Inst {
  Reg dst, base, offset;
  Shift shift;
  ComplexLoad(Reg _dst, Reg _base, Reg _offset)
      : dst(_dst), base(_base), offset(_offset) {}
  ComplexLoad(Reg _dst, Reg _base, Reg _offset, Shift _shift)
      : dst(_dst), base(_base), offset(_offset), shift(_shift) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {base, offset, dst};
    else
      return {base, offset};
  }
  virtual std::vector<Reg *> regs() override { return {&dst, &base, &offset}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// [base + offset] = src
struct ComplexStore : Inst {
  Reg src, base, offset;
  Shift shift;
  ComplexStore(Reg _src, Reg _base, Reg _offset)
      : src(_src), base(_base), offset(_offset) {}
  ComplexStore(Reg _src, Reg _base, Reg _offset, Shift _shift)
      : src(_src), base(_base), offset(_offset), shift(_shift) {}

  virtual std::vector<Reg> use_reg() override { return {src, base, offset}; }
  virtual bool side_effect() override { return true; }
  virtual std::vector<Reg *> regs() override { return {&src, &base, &offset}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// precondition: load_store_offset_range(src->position + offset - temporary sp
// offset) if not, must be replaced to several instructions to load immediate
// and load with register offset
struct LoadStack : Inst {
  Reg dst;
  int32_t offset;
  StackObject *src;
  LoadStack(Reg _dst, int32_t _offset, StackObject *_src)
      : dst(_dst), offset(_offset), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst};
    else
      return {};
  }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override {
    out << dst << " = LoadStack(" << src << ", " << offset << ")\n";
  }
};

// precondition: load_store_offset_range(target->position + offset - temporary
// sp offset) if not, must be replaced to several instructions to load immediate
// and store with register offset
struct StoreStack : Inst {
  Reg src;
  int32_t offset;
  StackObject *target;
  StoreStack(Reg _src, int32_t _offset, StackObject *_target)
      : src(_src), offset(_offset), target(_target) {}

  virtual std::vector<Reg> use_reg() override { return {src}; }
  virtual bool side_effect() override { return true; }
  virtual std::vector<Reg *> regs() override { return {&src}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override {
    out << "StoreStack(" << src << ", " << target << ", " << offset << ")\n";
  }
};

// order in stack is increasing order of register id
struct Push : Inst {
  std::vector<Reg> src;
  Push(std::vector<Reg> _src) : src(_src) {
    if (src[0].type == ScalarType::Int) {
      for (auto x : src)
        assert(x.type == ScalarType::Int);
    } else {
      std::sort(src.begin(), src.end());
      for (auto x : src)
        assert(x.type == ScalarType::Float);
      assert(src.back().id - src.front().id ==
             static_cast<int>(src.size() - 1));
    }
  }

  virtual std::vector<Reg> use_reg() override { return src; }
  virtual bool side_effect() override { return true; }
  virtual void maintain_sp(int32_t &sp_offset) override {
    sp_offset -= INT_SIZE * src.size();
  }
  virtual std::vector<Reg *> regs() override {
    std::vector<Reg *> ret;
    for (size_t i = 0; i < src.size(); ++i)
      ret.push_back(&src[i]);
    return ret;
  }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override {
    out << ((src[0].type == ScalarType::Int) ? "push" : "vpush") << cond
        << " {";
    for (size_t i = 0; i < src.size(); ++i) {
      if (i > 0)
        out << ',';
      out << src[i];
    }
    out << "}\n";
  }
};

// add sp, sp, #change or sub sp, sp, #-change
// precondition: is_legal_immediate(change) || is_legal_immediate(-change)
struct ChangeSP : Inst {
  int32_t change;
  ChangeSP(int32_t _change) : change(_change) {}

  virtual bool side_effect() override { return true; }
  virtual void maintain_sp(int32_t &sp_offset) override { sp_offset += change; }
  virtual std::vector<Reg *> regs() override { return {}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override {
    if (is_legal_immediate(change)) {
      out << "add" << cond << " sp,sp,#" << change << '\n';
    } else if (is_legal_immediate(-change)) {
      out << "sub" << cond << "sp,sp,#" << -change << '\n';
    }
  }
};
std::list<std::unique_ptr<Inst>> sp_move(int32_t change);
void sp_move_asm(int32_t change, std::ostream &out);

// must be replaced
struct LoadStackAddr : Inst {
  Reg dst;
  int32_t offset;
  StackObject *src;
  LoadStackAddr(Reg _dst, int32_t _offset, StackObject *_src)
      : dst(_dst), offset(_offset), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst};
    else
      return {};
  }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override {
    out << dst << " = LoadStackAddr(" << src << ", " << offset << ")\n";
  }
};

// must be replaced
struct LoadStackOffset : Inst {
  Reg dst;
  int32_t offset;
  StackObject *src;
  LoadStackOffset(Reg _dst, int32_t _offset, StackObject *_src)
      : dst(_dst), offset(_offset), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return {dst};
    else
      return {};
  }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override {
    out << dst << " = LoadStackOffset(" << src << ", " << offset << ")\n";
  }
};

// Cmp: compare lhs, rhs
// Cmn: compare lhs, -rhs
struct RegRegCmp : Inst {
  enum Type { Cmp, Cmn } op;
  Reg lhs, rhs;
  RegRegCmp(Type _op, Reg _lhs, Reg _rhs) : op(_op), lhs(_lhs), rhs(_rhs) {}

  virtual std::vector<Reg> use_reg() override { return {lhs, rhs}; }
  virtual bool change_cpsr() override { return true; }
  virtual std::vector<Reg *> regs() override { return {&lhs, &rhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

struct FRegRegCmp : Inst {
  Reg lhs, rhs;
  FRegRegCmp(Reg _lhs, Reg _rhs) : lhs(_lhs), rhs(_rhs) {}

  virtual std::vector<Reg> use_reg() override { return {lhs, rhs}; }
  virtual bool change_cpsr() override { return true; }
  virtual std::vector<Reg *> regs() override { return {&lhs, &rhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// precondition: is_legal_immediate(rhs)
// Cmp: compare lhs, rhs
// Cmn: compare lhs, -rhs
struct RegImmCmp : Inst {
  enum Type { Cmp, Cmn } op;
  Reg lhs;
  int32_t rhs;
  RegImmCmp(Type _op, Reg _lhs, int32_t _rhs) : op(_op), lhs(_lhs), rhs(_rhs) {}

  virtual std::vector<Reg> use_reg() override { return {lhs}; }
  virtual bool change_cpsr() override { return true; }
  virtual std::vector<Reg *> regs() override { return {&lhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

struct Branch : Inst {
  Block *target;
  Branch(Block *_target);

  virtual bool side_effect() override { return true; }
  virtual std::vector<Reg *> regs() override { return {}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

struct FuncCall : Inst {
  std::string name;
  int int_arg_cnt, float_arg_cnt;
  FuncCall(std::string _name, int _int_arg_cnt, int _float_reg_cnt)
      : name(_name), int_arg_cnt(_int_arg_cnt), float_arg_cnt(_float_reg_cnt) {}

  virtual std::vector<Reg> use_reg() override {
    if (cond != Always)
      return def_reg();
    std::vector<Reg> ret;
    for (int i = 0;
         i < std::min(int_arg_cnt,
                      RegConvention<ScalarType::Int>::ARGUMENT_REGISTER_COUNT);
         ++i)
      ret.emplace_back(
          Reg(RegConvention<ScalarType::Int>::ARGUMENT_REGISTERS[i],
              ScalarType::Int));
    for (int i = 0;
         i <
         std::min(float_arg_cnt,
                  RegConvention<ScalarType::Float>::ARGUMENT_REGISTER_COUNT);
         ++i)
      ret.emplace_back(
          Reg(RegConvention<ScalarType::Float>::ARGUMENT_REGISTERS[i],
              ScalarType::Float));
    return ret;
  }
  virtual std::vector<Reg> def_reg() override {
    std::vector<Reg> ret;
    for (int i = 0; i < RegConvention<ScalarType::Int>::Count; ++i)
      if (RegConvention<ScalarType::Int>::REGISTER_USAGE[i] ==
          RegisterUsage::caller_save)
        ret.emplace_back(Reg(i, ScalarType::Int));
    for (int i = 0; i < RegConvention<ScalarType::Float>::Count; ++i)
      if (RegConvention<ScalarType::Float>::REGISTER_USAGE[i] ==
          RegisterUsage::caller_save)
        ret.emplace_back(Reg(i, ScalarType::Float));
    ret.emplace_back(lr);
    return ret;
  }
  virtual bool side_effect() override { return true; }
  virtual std::vector<Reg *> regs() override { return {}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
};

// precondition: cond == Always
struct Return : Inst {
  ScalarType return_type;
  Return(ScalarType _return_type) : return_type(_return_type) {}

  virtual std::vector<Reg> use_reg() override {
    if (return_type == ScalarType::Int)
      return {Reg{RegConvention<ScalarType::Int>::ARGUMENT_REGISTERS[0],
                  ScalarType::Int}};
    if (return_type == ScalarType::Float)
      return {Reg{RegConvention<ScalarType::Float>::ARGUMENT_REGISTERS[0],
                  ScalarType::Float}};
    else
      return {};
  }
  virtual bool side_effect() override { return true; }
  virtual std::vector<Reg *> regs() override { return {}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override;
  virtual void print(std::ostream &out) override { out << "bx lr\n"; }
};

} // namespace ARMv7
