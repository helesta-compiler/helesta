#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/common.hpp"
#include "ir/scalar.hpp"

#define Case(T, a, b) if (auto a = dynamic_cast<T *>(b))
#define CaseNot(T, b)                                                          \
  if (auto _ = dynamic_cast<T *>(b)) {                                         \
    (void)_;                                                                   \
  } else

namespace IR {
using std::function;
using std::list;
using std::ostream;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

struct CompileUnit;
struct Func;
struct LibFunc;
struct NormalFunc;
struct MemScope;
struct MemObject;
struct BB;
struct Instr;
struct Reg;

struct Printable {
  virtual void print(ostream &os) const = 0;
  virtual ~Printable() {}
};

ostream &operator<<(ostream &os, const Printable &r);

template <class T> ostream &operator<<(ostream &os, const std::optional<T> &r) {
  if (r)
    os << r.value();
  else
    os << "(nullopt)";
  return os;
}

struct Reg : Printable {
  int id;
  Reg(int id = 0) : id(id) {}
  // id is unique in a NormalFunc
  void print(ostream &os) const override;
  bool operator==(const Reg &r) const { return id == r.id; }
  bool operator!=(const Reg &r) const { return id != r.id; }
  bool operator<(const Reg &r) const { return id < r.id; }
};

} // namespace IR
namespace std {
template <> struct hash<IR::Reg> {
  size_t operator()(const IR::Reg &r) const { return r.id; }
};
} // namespace std
namespace IR {
struct MemObject : Printable {
  // data stored in memory
  string name;
  MemSize size = 0; // number of bytes, size%4==0
  int offset = 0;   // offset from gp or sp, computed after machine irrelavant
                    // optimization
  bool global;
  bool arg = 0;
  // global=0 arg=0: local variable, but not arg
  // global=0 arg=1: arg of array type
  // global=1 arg=0: global variable
  // global=1 arg=1: stdin or stdout, for side effect analysis
  void *initial_value = NULL;
  ScalarType scalar_type;
  // only for global=1, NULL: zero initialization, non-NULL: init by these bytes

  bool is_const = 0;
  // computed in optimize_passes

  bool is_volatile = 0;
  // cannot be optimized

  std::vector<MemSize> dims;
  // only for int array, array dim size

  void init(float *x, MemSize size) {
    initial_value = x;
    scalar_type = ScalarType::Float;
    this->size = size;
  }
  void init(int32_t *x, MemSize size) {
    initial_value = x;
    scalar_type = ScalarType::Int;
    this->size = size;
  }
  void init(char *x, MemSize size) {
    initial_value = x;
    scalar_type = ScalarType::Char;
    this->size = size;
  }
  bool is_single_var() { return !arg && size == 4 && dims.empty(); }
  void print(ostream &os) const override;
  template <class T> T at(MemSize x, T _ = 0) {
    (void)_;
    if (!(x <= size && x + sizeof(T) <= size))
      return 0; // assert
    if (x % sizeof(T) != 0)
      return 0; // assert
    T v = 0;
    if (initial_value)
      v = ((T *)initial_value)[x / sizeof(T)];
    return v;
  }
  template <class T> void set(MemSize x, T y) {
    if (!(x <= size && x + sizeof(T) <= size))
      return; // assert
    if (x % sizeof(T) != 0)
      return; // assert
    assert(initial_value);
    ((T *)initial_value)[x / sizeof(T)] = y;
  }

private:
  friend struct MemScope;
  MemScope *fa;
  MemObject(string name, bool global) : name(name), global(global) {}
};

struct MemScope : Printable {
  string name;
  bool global;
  // global=0: stack frame
  // global=1: global variables
  vector<unique_ptr<MemObject>> objects;
  // list of MemObjects in this scope
  std::unordered_map<int, MemObject *> array_args;
  // map arg of array type to global=0,arg=1 MemObject
  std::unordered_map<MemObject *, int> array_arg_id;
  // map global=0,arg=1 MemObject to arg id in 0..arg_cnt-1
  int size;
  // computed after machine irrelavant optimization
  MemObject *new_MemObject(string _name) {
    MemObject *m = new MemObject(name + "::" + _name, global);
    objects.emplace_back(m);
    m->fa = this;
    return m;
  }
  void for_each(function<void(MemObject *)> f) {
    for (auto &x : objects)
      f(x.get());
  }
  void for_each(function<void(MemObject *, MemObject *)> f) {
    for (auto &x : objects) {
      auto y = new MemObject(*x);
      f(x.get(), y);
    }
  }
  void add(MemObject *m) {
    assert(!m->global);
    m->name = name + "::" + m->name;
    objects.emplace_back(m);
    m->fa = this;
  }
  void set_arg(int id, MemObject *m) {
    assert(has(m));
    assert(!array_args.count(id));
    assert(!array_arg_id.count(m));
    m->arg = 1;
    array_args[id] = m;
    array_arg_id[m] = id;
  }
  bool has(MemObject *m) { return m && m->fa == this; }
  void print(ostream &os) const override;

private:
  friend struct CompileUnit;
  friend struct NormalFunc;
  MemScope(string name, bool global) : name(name), global(global) {}
};

struct BB;

struct Instr : Printable {
  // IR instruction
  void map_use(function<void(Reg &)> f1);
  void map_BB(std::function<void(BB *&)> f) {
    map([](auto &) {}, f, [](auto &) {}, 0);
  }
  Instr *copy() {
    return map([](Reg &) {}, [](BB *&) {}, [](MemObject *&) {}, 1);
  }
  Instr *map(function<void(Reg &)> f1, function<void(BB *&)> f2,
             function<void(MemObject *&)> f3, bool copy = 1);
  // copy this Instr, and map each Reg by f1, map each BB* by f2, map each
  // MemObject by f3
};
struct PhiInstr;
struct BB : Printable, Traversable<BB> {
  // basic block
  string name;
  list<unique_ptr<Instr>> instrs;
  int id;
  bool disable_unroll = 0;
  bool disable_parallel = 0;
  int thread_id = 0;
  // list of instructions in this basic block
  // the last one is ControlInstr, others are RegWriteInstr or StoreInstr
  void print(ostream &os) const override;

  decltype(instrs)::iterator _it;
  void for_each(function<void(Instr *)> f) {
    for_each_until([&](Instr *x) -> bool {
      f(x);
      return 0;
    });
  }
  decltype(_it) cur_iter() { return std::prev(_it); }
  void replace(Instr *x) { *std::prev(_it) = unique_ptr<Instr>(x); }
  bool _del = 0;
  void move() {
    (void)std::prev(_it)->release();
    del();
  }
  void del() { _del = 1; }
  void ins(Instr *x) { instrs.insert(std::prev(_it), unique_ptr<Instr>(x)); }
  void ins(decltype(instrs) &&ls) {
    for (auto &x : ls) {
      ins(x.release());
    }
    ls.clear();
  }
  void push(decltype(instrs) &&ls) {
    for (auto &x : ls) {
      push(x.release());
    }
    ls.clear();
  }
  void push1(decltype(instrs) &&ls) {
    for (auto &x : ls) {
      push1(x.release());
    }
    ls.clear();
  }
  void replace(decltype(instrs) &&ls) {
    ins(std::move(ls));
    del();
  }
  bool for_each_until(function<bool(Instr *)> f) {
    for (_it = instrs.begin(); _it != instrs.end();) {
      auto it0 = _it;
      ++_it;
      _del = 0;
      bool ret = f(it0->get());
      if (_del) {
        instrs.erase(it0);
      }
      if (ret)
        return 1;
    }
    return 0;
  }
  void push_front(Instr *x) { instrs.emplace_front(x); }
  void push(Instr *x) { instrs.emplace_back(x); }
  void push1(Instr *x) { instrs.insert(--instrs.end(), unique_ptr<Instr>(x)); }
  void pop() { instrs.pop_back(); }
  Instr *back() const { return instrs.back().get(); }
  Instr *back1() const { return std::prev(instrs.end(), 2)->get(); }
  void map_BB(std::function<void(BB *&)> f) {
    for (auto &x : instrs)
      x->map_BB(f);
  }
  void map_use(std::function<void(Reg &)> f);
  void map_phi_use(std::function<void(Reg &)> f1,
                   std::function<void(BB *&)> f2);
  void map_phi_use(std::function<void(Reg &, BB *&)> f);

  const std::vector<BB *> getOutNodes() const override;
  void addOutNode(BB *) override {
    // unreachable
    assert(false);
  }

private:
  friend struct NormalFunc;
  BB(string name) : name(name) {}
  std::vector<BB *> outs;
};

struct Func : Printable {
  // function
  string name;
  bool ignore_return_value = 0;
  Func(string name) : name(name) {}
};

struct LibFunc : Func {
  // extern function
  void print(ostream &os) const override { os << "LibFunc: " << name; }
  std::unordered_map<int, bool> array_args;
  // read/write args of array type
  // (arg_id,1): read and write
  // (arg_id,0): read only
  bool in = 0,
       out = 0; // IO side effect, in: stdin changed, out: stdout changed
  bool pure = 0;

private:
  friend struct CompileUnit;
  LibFunc(string name) : Func(name) {}
};

struct NormalFunc : Func {
  // function defined in compile unit (.sy file)
  MemScope scope;
  // local variables on stack, and args of array type
  BB *entry = NULL;
  // first basic block to excute
  vector<unique_ptr<BB>> bbs;
  // list of basic blocks in this function
  int max_reg_id = 0, max_bb_id = 0;
  // for id allocation
  vector<string> reg_names;
  std::vector<ScalarType> arg_types;

  std::unordered_set<Reg> thread_local_regs;
  Reg new_Reg() { return new_Reg("R" + to_string(max_reg_id + 1)); }
  Reg new_Reg(string _name) {
    reg_names.push_back(_name);
    // reg id : 1 .. max_reg_id
    // 1 .. param_cnt : arguments
    return Reg(++max_reg_id);
  }
  BB *new_BB(string _name = "BB") {
    auto bb_name = _name + std::to_string(++max_bb_id);
    auto prefix = name + "::";
    if (!bb_name.compare(0, prefix.size(), prefix)) {
      bb_name = prefix + bb_name;
    }
    BB *bb = new BB(bb_name);
    bbs.emplace_back(bb);
    return bb;
  }
  void for_each(function<void(BB *)> f) {
    for (auto &bb : bbs)
      f(bb.get());
  }
  bool for_each_until(function<bool(BB *)> f) {
    for (auto &bb : bbs)
      if (f(bb.get()))
        return 1;
    return 0;
  }
  void for_each(function<void(Instr *)> f) {
    for (auto &bb : bbs)
      bb->for_each(f);
  }
  void print(ostream &os) const override;
  string get_name(Reg r) const { return reg_names.at(r.id); }

private:
  friend struct CompileUnit;
  NormalFunc(string name) : Func(name), scope(name, 0) {
    reg_names.push_back("?");
  }
};

struct CompileUnit : Printable {
  // the whole program
  MemScope scope; // global arrays
  std::map<string, unique_ptr<NormalFunc>> funcs;
  std::vector<NormalFunc *> _funcs;
  // functions defined in .sy file
  std::map<string, unique_ptr<LibFunc>> lib_funcs;
  // functions defined in library
  CompileUnit();
  NormalFunc *main() { return funcs.at("main").get(); }
  NormalFunc *new_NormalFunc(string _name) {
    NormalFunc *f = new NormalFunc(_name);
    funcs[_name] = unique_ptr<NormalFunc>(f);
    _funcs.push_back(f);
    return f;
  }
  void print(ostream &os) const override;
  void map(function<void(CompileUnit &)> f) { f(*this); }
  void for_each(function<void(NormalFunc *)> f) {
    for (auto &x : _funcs)
      f(x);
  }
  void for_each(function<void(MemScope &)> f) {
    f(scope);
    for (auto &x : _funcs)
      f(x->scope);
  }

private:
  LibFunc *new_LibFunc(string _name, bool ignore_return_value) {
    LibFunc *f = new LibFunc(_name);
    f->ignore_return_value = ignore_return_value;
    lib_funcs[_name] = unique_ptr<LibFunc>(f);
    return f;
  }
};

struct UnaryOp : Printable {
  UnaryCompute type;
  typeless_scalar_t compute(typeless_scalar_t x) {
    return IR::compute(type, x);
  }
  UnaryOp(ScalarType _type, UnaryCompute x) : type(x) {
    assert(input_type() == ScalarType::Int);
    assert(ret_type() == ScalarType::Int);
    switch (_type) {
    case ScalarType::Int:
      break;
    case ScalarType::Float:
      switch (type) {
      case UnaryCompute::LNOT:
        assert(0);
        break;
      case UnaryCompute::NEG:
        type = UnaryCompute::FNEG;
        break;
      default:
        assert(0);
      }
      break;
    default:
      assert(0);
    }
  }
  UnaryOp(UnaryCompute x) : type(x) {}
  ScalarType input_type() {
    return type == UnaryCompute::FNEG || type == UnaryCompute::F2I ||
                   type == UnaryCompute::F2D0 || type == UnaryCompute::F2D1
               ? ScalarType::Float
               : ScalarType::Int;
  }
  ScalarType ret_type() {
    return type == UnaryCompute::FNEG || type == UnaryCompute::I2F ||
                   type == UnaryCompute::F2D0 || type == UnaryCompute::F2D1
               ? ScalarType::Float
               : ScalarType::Int;
  }
  const char *get_name() const {
    static const char *names[] = {
        "!", "-", "+", "(-F)", "(int)", "(float)", "(double.0)", "(double.1)"};
    return names[(int)type];
  }
  void print(ostream &os) const override { os << get_name(); }
};

struct BinaryOp : Printable {
  BinaryCompute type;
  BinaryOp(ScalarType _type, BinaryCompute x) : type(x) {
    assert(ret_type() == ScalarType::Int);
    switch (_type) {
    case ScalarType::Int:
      break;
    case ScalarType::Float:
      switch (type) {
      case BinaryCompute::ADD:
        type = BinaryCompute::FADD;
        break;
      case BinaryCompute::SUB:
        type = BinaryCompute::FSUB;
        break;
      case BinaryCompute::MUL:
        type = BinaryCompute::FMUL;
        break;
      case BinaryCompute::DIV:
        type = BinaryCompute::FDIV;
        break;
      case BinaryCompute::LESS:
        type = BinaryCompute::FLESS;
        break;
      case BinaryCompute::LEQ:
        type = BinaryCompute::FLEQ;
        break;
      case BinaryCompute::EQ:
        type = BinaryCompute::FEQ;
        break;
      case BinaryCompute::NEQ:
        type = BinaryCompute::FNEQ;
        break;
      default:
        assert(0);
      }
      break;
    default:
      assert(0);
    }
  }
  BinaryOp(BinaryCompute x) : type(x) {}
  typeless_scalar_t compute(typeless_scalar_t x, typeless_scalar_t y) {
    return IR::compute(type, x, y);
  }

  ScalarType input_type() {
    return type == BinaryCompute::FLESS || type == BinaryCompute::FLEQ ||
                   type == BinaryCompute::FEQ || type == BinaryCompute::FNEQ
               ? ScalarType::Float
               : ret_type();
  }
  ScalarType ret_type() {
    return type == BinaryCompute::FADD || type == BinaryCompute::FSUB ||
                   type == BinaryCompute::FMUL || type == BinaryCompute::FDIV
               ? ScalarType::Float
               : ScalarType::Int;
  }
  bool comm() {
    return type == BinaryCompute::ADD || type == BinaryCompute::MUL ||
           type == BinaryCompute::EQ || type == BinaryCompute::NEQ;
  }
  const char *get_name() const {
    static const char *names[] = {"+",      "-",      "*",     "/",     "<",
                                  "<=",     "==",     "!=",    "%",     "<<",
                                  "(F+F)",  "(F-F)",  "(F*F)", "(F/F)", "(F<F)",
                                  "(F<=F)", "(F==F)", "(F!=F)"};
    return names[(int)type];
  }
  void print(ostream &os) const override { os << get_name(); }
};

struct RegWriteInstr : Instr {
  // any instr that write a reg
  // no instr write multiple regs
  Reg d1;
  RegWriteInstr(Reg d1) : d1(d1) {}
};

struct LoadAddr : RegWriteInstr {
  // load address of offset to d1
  // d1 = addr
  MemObject *offset;
  LoadAddr(Reg d1, MemObject *offset) : RegWriteInstr(d1), offset(offset) {
    assert(!offset->arg);
  }
  void print(ostream &os) const override;
};

template <typename Scalar> struct LoadConst : RegWriteInstr {
  // load value to d1
  // d1 = value
  Scalar value;
  LoadConst(Reg d1, Scalar value) : RegWriteInstr(d1), value(value) {}
  void print(ostream &os) const override;
};

template <ScalarType type> struct LoadArg : RegWriteInstr {
  // load arg with arg_id=id to d1
  // d1 = arg
  int id;
  LoadArg(Reg d1, int id) : RegWriteInstr(d1), id(id) {}
  void print(ostream &os) const override;
};

struct UnaryOpInstr : RegWriteInstr {
  // d1 = op(s1)
  UnaryOpInstr(Reg d1, Reg s1, UnaryOp op)
      : RegWriteInstr(d1), s1(s1), op(op) {}
  Reg s1;
  UnaryOp op;
  typeless_scalar_t compute(typeless_scalar_t x);
  void print(ostream &os) const override;
};

struct BinaryOpInstr : RegWriteInstr {
  // d1 = op(s1,s2)
  BinaryOpInstr(Reg d1, Reg s1, Reg s2, BinaryOp op)
      : RegWriteInstr(d1), s1(s1), s2(s2), op(op) {}
  Reg s1, s2;
  BinaryOp op;
  typeless_scalar_t compute(typeless_scalar_t x, typeless_scalar_t y);
  void print(ostream &os) const override;
};

struct LoadInstr : RegWriteInstr {
  // memory read, used in ssa, but not in array-ssa
  // d1 = M[addr]
  LoadInstr(Reg d1, Reg addr) : RegWriteInstr(d1), addr(addr) {}
  Reg addr;
  int32_t offset = 0;
  void print(ostream &os) const override;
};

struct StoreInstr : Instr {
  // memory write, used in ssa, but not in array-ssa
  // M[addr] = s1
  StoreInstr(Reg addr, Reg s1) : addr(addr), s1(s1) {}
  Reg addr;
  Reg s1;
  int32_t offset = 0;
  void print(ostream &os) const override;
};

struct ControlInstr : Instr {
  // any instr except call that change PC
};

struct JumpInstr : ControlInstr {
  // PC = target
  BB *target;
  JumpInstr(BB *target) : target(target) {}
  void print(ostream &os) const override;
};

struct BranchInstr : ControlInstr {
  // if (cond) then PC = value
  Reg cond;
  BB *target1, *target0;
  BranchInstr(Reg cond, BB *target1, BB *target0)
      : cond(cond), target1(target1), target0(target0) {}
  void print(ostream &os) const override;
};

template <ScalarType type> struct ReturnInstr : ControlInstr {
  // return s1
  Reg s1;
  bool ignore_return_value;
  ReturnInstr(Reg s1, bool ignore_return_value)
      : s1(s1), ignore_return_value(ignore_return_value) {}
  void print(ostream &os) const override;
};

struct CallInstr : RegWriteInstr {
  // d1 = f(args[0],args[1],...)
  vector<std::pair<Reg, ScalarType>> args;
  Func *f;
  ScalarType return_type;
  bool no_store = 0, no_load = 0;
  CallInstr(Reg d1, Func *f, vector<std::pair<Reg, ScalarType>> args,
            ScalarType return_type_)
      : RegWriteInstr(d1), args(args), f(f), return_type(return_type_) {
    assert(f);
    Case(LibFunc, f0, f) {
      if (f0->pure) {
        no_load = 1;
        no_store = 1;
      }
    }
  }
  void print(ostream &os) const override;
};

struct ArrayIndex : RegWriteInstr {
  // d1 = s1+s2*size, 0 <= s2 < limit
  Reg s1, s2;
  int size, limit;
  ArrayIndex(Reg d1, Reg s1, Reg s2, int size, int limit)
      : RegWriteInstr(d1), s1(s1), s2(s2), size(size), limit(limit) {}
  void print(ostream &os) const override;
};

struct SIMDInstr : CallInstr {
  enum Type {
    VADD_I32 = 0,
    VADD_F32 = 1,
    VSUB_I32 = 2,
    VSUB_F32 = 3,
    VMUL_S32 = 4,
    VMUL_F32 = 5,
    VMLA_S32 = 6,
    VMLA_F32 = 7,
    VDUP_32 = 8,
    VLDM = 9,
    VSTM = 10,
    VCVT_S32_F32 = 11,
    VCVT_F32_S32 = 12,
  } type;
  std::optional<Reg> s1;
  std::vector<int> regs;
  int size = 0;
  SIMDInstr(Reg r, Func *f, Type _type, Reg _s1, std::vector<int> _regs)
      : CallInstr(r, f, {}, ScalarType::Void), type(_type), s1(_s1),
        regs(_regs) {
    switch (type) {
    case VDUP_32:
      assert(regs.size() == 1);
      break;
    case VLDM:
    case VSTM:
      assert(regs.size() == 1);
      size = 1;
      break;
    default:
      assert(0);
    }
  }
  SIMDInstr(Reg r, Func *f, Type _type, std::vector<int> _regs)
      : CallInstr(r, f, {}, ScalarType::Void), type(_type), s1(std::nullopt),
        regs(_regs) {
    switch (type) {
    case VADD_I32:
    case VADD_F32:
    case VSUB_I32:
    case VSUB_F32:
    case VMUL_S32:
    case VMUL_F32:
    case VMLA_S32:
    case VMLA_F32:
      assert(regs.size() == 3);
      break;
    case VCVT_S32_F32:
    case VCVT_F32_S32:
      assert(regs.size() == 2);
      break;
    default:
      assert(0);
    }
  }
  inline static const char *names[] = {
      "vadd.i32", "vadd.f32",     "vsub.i32",    "vsub.f32", "vmul.s32",
      "vmul.f32", "vmla.s32",     "vmla.f32",    "vdup.32",  "vldm",
      "vstm",     "vcvt.s32.f32", "vcvt.f32.s32"};
  const char *name() const { return names[(int)type]; }
  void compute(typeless_scalar_t simd_regs[][4]);
  int get_id(int x) const {
    assert(0 <= x && x < 8);
    return x + 8;
  }
  void print(ostream &os) const override;
};

// for ssa

struct PhiInstr : RegWriteInstr {
  // only for ssa and array-ssa
  vector<std::pair<Reg, BB *>> uses;
  PhiInstr(Reg d1) : RegWriteInstr(d1) {}
  void add_use(Reg r, BB *pos) { uses.emplace_back(r, pos); }
  void print(ostream &os) const override;
};

// for each (R1,R2):mp_reg, change the usage of R1 to R2, but defs are not
// changed
void map_use(NormalFunc *f, const std::unordered_map<Reg, Reg> &mp_reg);

template <class T>
std::function<void(T)> repeat(bool (*f)(T), int max = 1000000) {
  return [=](T x) {
    for (int i = 0; i < max && f(x); ++i)
      ;
  };
}

template <class T> std::function<void(T &)> partial_map(T from, T to) {
  return [from, to](T &x) {
    if (x == from)
      x = to;
  };
}
template <class T>
std::function<void(T &)> partial_map(std::unordered_map<T, T> &mp) {
  auto *p_mp = &mp;
  return [p_mp](T &x) {
    auto it = p_mp->find(x);
    if (it != p_mp->end())
      x = it->second;
  };
}
template <class T>
std::function<void(T &)> sequential(std::function<void(T &)> a,
                                    std::function<void(T &)> b) {
  return [a, b](T &x) {
    a(x);
    b(x);
  };
}

int exec(CompileUnit &c);

void print_all_bb(CompileUnit &c, std::ostream &os);

} // namespace IR

namespace std {
template <class T1, class T2> struct hash<pair<T1, T2>> {
  size_t operator()(const pair<T1, T2> &r) const {
    return hash<T1>()(r.first) * 1844677 + hash<T2>()(r.second) * 41;
  }
};
template <> struct hash<tuple<int, int, int>> {
  size_t operator()(const tuple<int, int, int> &r) const {
    return get<0>(r) * 293999 + get<1>(r) * 1234577 + get<2>(r) * 29;
  }
};
} // namespace std

namespace IR {
struct PrintContext {
  const CompileUnit *c = NULL;
  const NormalFunc *f = NULL;
  bool disable_reg_id = 0;
  std::unordered_map<Instr *, string> instr_comment;
};

extern PrintContext print_ctx;

struct SetPrintContext {
  const NormalFunc *f0 = NULL;
  SetPrintContext(const NormalFunc *f) {
    f0 = print_ctx.f;
    print_ctx.f = f;
  }
  ~SetPrintContext() { print_ctx.f = f0; }
};
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
    bop(+, ADD) bop(-, SUB) bop(*, MUL) bop(/, DIV) bop(%, MOD) bop(<, LESS)
        bop(<=, LEQ) bop(==, EQ) bop(!=, NEQ)
#undef bop
  };
  RegRef reg(Reg r) { return RegRef{r, this}; }
  RegRef lc(int32_t x) {
    Reg r = f->new_Reg();
    instrs.emplace_back(new LoadConst<int32_t>(r, x));
    return reg(r);
  }
  RegRef la(MemObject *x) {
    Reg r = f->new_Reg();
    instrs.emplace_back(new LoadAddr(r, x));
    return reg(r);
  }
  RegRef ld_volatile(CompileUnit *ir, RegRef x) {
    auto f = ir->lib_funcs.at("__ld_volatile").get();
    return call(f, ScalarType::Int, {{x, ScalarType::Int}});
  }
  void st_volatile(CompileUnit *ir, RegRef x, RegRef y) {
    auto f = ir->lib_funcs.at("__st_volatile").get();
    call(f, ScalarType::Int, {{x, ScalarType::Int}, {y, ScalarType::Int}});
  }
  RegRef ld(RegRef x) {
    Reg r = f->new_Reg();
    instrs.emplace_back(new LoadInstr(r, x.r));
    return reg(r);
  }
  void st(RegRef x, RegRef y) { instrs.emplace_back(new StoreInstr(x.r, y.r)); }
  void branch(RegRef cond, BB *target1, BB *target0) {
    instrs.emplace_back(new BranchInstr(cond.r, target1, target0));
  }
  void jump(BB *target) { instrs.emplace_back(new JumpInstr(target)); }
  RegRef call(Func *f0, ScalarType ret_type,
              std::vector<std::pair<RegRef, ScalarType>> args_ref = {}) {
    Reg r = f->new_Reg();
    std::vector<std::pair<Reg, ScalarType>> args;
    for (auto [x, t] : args_ref)
      args.emplace_back(x.r, ScalarType::Int);
    instrs.emplace_back(new CallInstr(r, f0, args, ret_type));
    return reg(r);
  }
};

} // namespace IR
