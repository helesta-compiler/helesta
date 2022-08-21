#include "ir/ir.hpp"
#include <cstring>

namespace IR {

void BB::map_phi_use(std::function<void(Reg &)> f1,
                     std::function<void(BB *&)> f2) {
  for (auto &x : instrs) {
    Case(PhiInstr, phi, x.get()) {
      for (auto &[r, bb] : phi->uses) {
        f1(r);
        f2(bb);
      }
    }
  }
}

void BB::map_phi_use(std::function<void(Reg &, BB *&)> f) {
  for (auto &x : instrs) {
    Case(PhiInstr, phi, x.get()) {
      for (auto &[r, bb] : phi->uses) {
        f(r, bb);
      }
    }
  }
}

void BB::map_use(std::function<void(Reg &)> f) {
  for (auto &x : instrs)
    x->map_use(f);
}

PrintContext print_ctx;

ostream &operator<<(ostream &os, const Printable &r) {
  r.print(os);
  return os;
}

void Reg::print(ostream &os) const {
  auto f = print_ctx.f;
  if (f) {
    os << f->get_name(*this);
    if (!print_ctx.disable_reg_id)
      os << "(" << id << ")";
  } else {
    os << "R[" << id << "]";
  }
}

void MemObject::print(ostream &os) const {
  os << "&" << name << " (" << (global ? "gp" : "sp") << "+" << offset
     << "): scalar_type " << scalar_type << " size " << size;
  if (arg)
    os << " (arg)";
}

void MemScope::print(ostream &os) const {
  os << "MemScope(" << name << "){\n";
  for (auto &x : objects)
    os << "\t" << *x << "\n";
  os << "}\n";
}

const std::vector<BB *> BB::getOutNodes() const {
  auto last = back();
  if (auto jump_instr = dynamic_cast<JumpInstr *>(last)) {
    return {jump_instr->target};
  } else if (auto branch_instr = dynamic_cast<BranchInstr *>(last)) {
    return {branch_instr->target0, branch_instr->target1};
  } else {
    assert(dynamic_cast<ReturnInstr<ScalarType::Int> *>(last) != nullptr ||
           dynamic_cast<ReturnInstr<ScalarType::Float> *>(last) != nullptr);
  }
  return {};
}

void BB::print(ostream &os) const {
  os << "BB(" << name << "){\n";
  for (auto &x : instrs) {
    os << "\t" << *x;
    if (print_ctx.instr_comment.count(x.get())) {
      os << "  // " << print_ctx.instr_comment[x.get()];
    }
    os << "\n";
  }
  os << "}\n";
}

void NormalFunc::print(ostream &os) const {
  SetPrintContext _(this);
  os << "NormalFunc: " << name << "\n";
  os << scope << "\n";
  os << "entry: " << entry->name << "\n";
  for (auto &bb : bbs)
    os << *bb;
  os << "\n";
}

void CompileUnit::print(ostream &os) const {
  print_ctx.c = this;
  os << scope << "\n";
  for (auto &f : funcs)
    os << *f.second;
  print_ctx.c = NULL;
}

void LoadAddr::print(ostream &os) const { os << d1 << " = " << *offset; }
template <> void LoadConst<int32_t>::print(ostream &os) const {
  os << d1 << " = " << value;
}
template <> void LoadConst<float>::print(ostream &os) const {
  os << d1 << " = " << value << 'f';
}
template <ScalarType type> void LoadArg<type>::print(ostream &os) const {
  os << d1 << " = arg" << id;
  if (type == ScalarType::Float)
    os << " float";
}
void ArrayIndex::print(ostream &os) const {
  os << d1 << " = " << s1 << " + " << s2 << " * " << size << " : " << limit;
}
void UnaryOpInstr::print(ostream &os) const {
  os << d1 << " = " << op << " " << s1;
}
void BinaryOpInstr::print(ostream &os) const {
  os << d1 << " = " << s1 << " " << op << " " << s2;
}
ostream &operator<<(ostream &os, LoadStoreAddr &a) {
  os << "M[" << a.addr;
  if (a.offset)
    os << " + " << a.offset;
  else if (a.reg_offset)
    os << " + " << *a.reg_offset << " * 4";
  os << "]";
  return os;
}
void LoadInstr::print(ostream &os) const {
  os << d1 << " = " << *(LoadStoreAddr *)this;
}
void StoreInstr::print(ostream &os) const {
  os << *(LoadStoreAddr *)this << " = " << s1;
}
void JumpInstr::print(ostream &os) const { os << "goto " << target->name; }
void BranchInstr::print(ostream &os) const {
  os << "goto " << cond << " ? " << target1->name << " : " << target0->name;
}
template <ScalarType type> void ReturnInstr<type>::print(ostream &os) const {
  os << "return " << s1;
}

void CallInstr::print(ostream &os) const {
  os << d1 << " = " << f->name;
  char c = '(';
  for (auto s : args) {
    os << c << s.first;
    c = ',';
  }
  if (c == '(')
    os << c;
  os << ')';
}
void SIMDInstr::print(ostream &os) const {
  // os << d1 << " = ";
  os << name() << ' ';
  if (s1)
    os << *s1 << ' ';
  bool flag = 0;
  for (int x : regs) {
    if (flag)
      os << ',';
    os << 'q' << get_id(x);
    flag = 1;
  }
  if (size)
    os << "(x" << size << ")";
}
void PhiInstr::print(ostream &os) const {
  os << d1 << " = phi";
  char c = '(';
  for (auto s : uses) {
    os << c << " " << s.first << ":" << s.second->name;
    c = ',';
  }
  if (c == '(')
    os << c;
  os << " )";
}

CompileUnit::CompileUnit() : scope("global", 1) {
  auto pure_func = [&](const char *name) {
    LibFunc *f = new_LibFunc(name, 0);
    f->pure = 1;
    return f;
  };
  LibFunc *f;
  f = new_LibFunc("__create_threads", 0);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__join_threads", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__bind_core", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__lock", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__unlock", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__ld_volatile", 0);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__st_volatile", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__barrier", 1);
  f->in = 1;
  f->out = 1;

  pure_func("__umulmod")->impl = [](typeless_scalar_t *args,
                                    int argc) -> typeless_scalar_t {
    assert(argc == 3);
    uint32_t a = args[0].int_value();
    uint32_t b = args[1].int_value();
    uint32_t c = args[2].int_value();
    return int(1ull * a * b % c);
  };
  pure_func("__u_c_np1_2_mod")->impl = [](typeless_scalar_t *args,
                                          int argc) -> typeless_scalar_t {
    assert(argc == 2);
    uint32_t a = args[0].int_value();
    uint32_t b = args[1].int_value();
    return int((1ull * a * a + a) / 2ull % b);
  };
  pure_func("__s_c_np1_2")->impl = [](typeless_scalar_t *args,
                                      int argc) -> typeless_scalar_t {
    assert(argc == 1);
    int32_t a = args[0].int_value();
    return int((1ll * a * a + a) / 2ll);
  };
  pure_func("__umod")->impl = [](typeless_scalar_t *args,
                                 int argc) -> typeless_scalar_t {
    assert(argc == 2);
    uint32_t a = args[0].int_value();
    uint32_t b = args[1].int_value();
    return int(a % b);
  };
  pure_func("__fixmod")->impl = [](typeless_scalar_t *args,
                                   int argc) -> typeless_scalar_t {
    assert(argc == 2);
    int32_t a = args[0].int_value();
    int32_t b = args[1].int_value();
    a %= b;
    if (a < 0)
      a += b;
    return a;
  };
  pure_func("__mla")->impl = [](typeless_scalar_t *args,
                                int argc) -> typeless_scalar_t {
    assert(argc == 3);
    int32_t a = args[0].int_value();
    int32_t b = args[1].int_value();
    int32_t c = args[2].int_value();
    return a + b * c;
  };
  pure_func("__mls")->impl = [](typeless_scalar_t *args,
                                int argc) -> typeless_scalar_t {
    assert(argc == 3);
    int32_t a = args[0].int_value();
    int32_t b = args[1].int_value();
    int32_t c = args[2].int_value();
    return a - b * c;
  };
  pure_func("__divpow2")->impl = [](typeless_scalar_t *args,
                                    int argc) -> typeless_scalar_t {
    assert(argc == 2);
    int32_t a = args[0].int_value();
    uint32_t b = args[1].int_value();
    if (b >= 32)
      return 0;
    return a / (1 << b);
  };
  pure_func("__f_mov_to_i")->impl = [](typeless_scalar_t *args,
                                       int argc) -> typeless_scalar_t {
    assert(argc == 1);
    return args[0].int_value();
  };

  f = new_LibFunc("__simd", 1);
  f->in = 1;
  f->out = 1;

  for (auto name : {"getint", "getch", "getfloat"}) {
    f = new_LibFunc(name, 0);
    f->in = 1;
  }
  for (auto name : {"getarray", "getfarray"}) {
    f = new_LibFunc(name, 0);
    f->array_args[0] = 1; // write arg0
    f->in = 1;
  }
  for (auto name : {"putint", "putch", "putfloat"}) {
    f = new_LibFunc(name, 1);
    f->out = 1;
  }
  for (auto name : {"putarray", "putfarray"}) {
    f = new_LibFunc(name, 1);
    f->array_args[1] = 0; // read arg1
    f->out = 1;
  }

  f = new_LibFunc("putf", 1);
  f->array_args[0] = 0; // read arg0
  f->out = 1;

  f = new_LibFunc("starttime", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("stoptime", 1);
  f->in = 1;
  f->out = 1;

  auto _in = scope.new_MemObject("stdin");   // input
  auto _out = scope.new_MemObject("stdout"); // output
  scope.set_arg(0, _in);
  scope.set_arg(1, _out);
}

void Instr::map_use(function<void(Reg &)> f1) {
  auto f2 = [](BB *&) {};
  auto f3 = [](MemObject *&) {};
  Case(RegWriteInstr, w, this) {
    map(
        [&](Reg &x) {
          if (&x != &w->d1)
            f1(x);
        },
        f2, f3, 0);
  }
  else {
    map(f1, f2, f3, 0);
  }
}

Instr *Instr::map(function<void(Reg &)> f1, function<void(BB *&)> f2,
                  function<void(MemObject *&)> f3, bool copy) {
  Case(LoadAddr, w, this) {
    auto u = w;
    if (copy)
      u = new LoadAddr(*w);
    f1(u->d1);
    f3(u->offset);
    return u;
  }
  Case(LoadConst<int32_t>, w, this) {
    auto u = w;
    if (copy)
      u = new LoadConst(*w);
    f1(u->d1);
    return u;
  }
  Case(LoadConst<float>, w, this) {
    auto u = w;
    if (copy)
      u = new LoadConst(*w);
    f1(u->d1);
    return u;
  }
  Case(LoadArg<ScalarType::Int>, w, this) {
    auto u = w;
    if (copy)
      u = new LoadArg(*w);
    f1(u->d1);
    return u;
  }
  Case(LoadArg<ScalarType::Float>, w, this) {
    auto u = w;
    if (copy)
      u = new LoadArg(*w);
    f1(u->d1);
    return u;
  }
  Case(UnaryOpInstr, w, this) {
    auto u = w;
    if (copy)
      u = new UnaryOpInstr(*w);
    f1(u->d1);
    f1(u->s1);
    return u;
  }
  Case(BinaryOpInstr, w, this) {
    auto u = w;
    if (copy)
      u = new BinaryOpInstr(*w);
    f1(u->d1);
    f1(u->s1);
    f1(u->s2);
    return u;
  }
  Case(ArrayIndex, w, this) {
    auto u = w;
    if (copy)
      u = new ArrayIndex(*w);
    f1(u->d1);
    f1(u->s1);
    f1(u->s2);
    return u;
  }
  Case(LoadInstr, w, this) {
    auto u = w;
    if (copy)
      u = new LoadInstr(*w);
    f1(u->d1);
    f1(u->addr);
    if (u->reg_offset)
      f1(*u->reg_offset);
    return u;
  }
  Case(StoreInstr, w, this) {
    auto u = w;
    if (copy)
      u = new StoreInstr(*w);
    f1(u->addr);
    f1(u->s1);
    if (u->reg_offset)
      f1(*u->reg_offset);
    return u;
  }
  Case(JumpInstr, w, this) {
    auto u = w;
    if (copy)
      u = new JumpInstr(*w);
    f2(u->target);
    return u;
  }
  Case(BranchInstr, w, this) {
    auto u = w;
    if (copy)
      u = new BranchInstr(*w);
    f1(u->cond);
    f2(u->target1);
    f2(u->target0);
    return u;
  }
  Case(ReturnInstr<ScalarType::Int>, w, this) {
    auto u = w;
    if (copy)
      u = new ReturnInstr<ScalarType::Int>(*w);
    f1(u->s1);
    return u;
  }
  Case(ReturnInstr<ScalarType::Float>, w, this) {
    auto u = w;
    if (copy)
      u = new ReturnInstr<ScalarType::Float>(*w);
    f1(u->s1);
    return u;
  }
  Case(SIMDInstr, w, this) {
    auto u = w;
    if (copy)
      u = new SIMDInstr(*w);
    if (u->s1)
      f1(*u->s1);
    return u;
  }
  Case(CallInstr, w, this) {
    auto u = w;
    if (copy)
      u = new CallInstr(*w);
    f1(u->d1);
    for (auto &x : u->args)
      f1(x.first);
    return u;
  }
  Case(PhiInstr, w, this) {
    auto u = w;
    if (copy)
      u = new PhiInstr(*w);
    f1(u->d1);
    for (auto &x : u->uses)
      f1(x.first), f2(x.second);
    return u;
  }
  assert(0);
  return NULL;
}

typeless_scalar_t UnaryOpInstr::compute(typeless_scalar_t s1) {
  return op.compute(s1);
}

typeless_scalar_t BinaryOpInstr::compute(typeless_scalar_t s1,
                                         typeless_scalar_t s2) {
  return op.compute(s1, s2);
}

void map_use(NormalFunc *f, const std::unordered_map<Reg, Reg> &mp_reg) {
  f->for_each([&](Instr *x) {
    x->map_use([&](Reg &r) {
      auto it = mp_reg.find(r);
      if (it != mp_reg.end())
        r = it->second;
    });
  });
}

void print_all_bb(CompileUnit &c, std::ostream &os) {
  c.for_each([&](IR::NormalFunc *func) {
    func->for_each([&](IR::BB *bb) { bb->print(os); });
  });
}
} // namespace IR
