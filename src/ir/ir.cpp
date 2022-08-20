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

void SIMDInstr::compute(typeless_scalar_t simd_regs[][4]) {
#define at_(j, type) simd_regs[regs[j]][i].type##_value()
  switch (type) {
  case VCVT_S32_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, float);
    }
    break;
  case VCVT_F32_S32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, int);
    }
    break;
  case VADD_I32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, int) + at_(2, int);
    }
    break;
  case VADD_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, float) + at_(2, float);
    }
    break;
  case VSUB_I32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, int) - at_(2, int);
    }
    break;
  case VSUB_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, float) - at_(2, float);
    }
    break;
  case VMUL_S32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, int) * at_(2, int);
    }
    break;
  case VMUL_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, float) * at_(2, float);
    }
    break;
  case VMLA_S32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) += at_(1, int) * at_(2, int);
    }
    break;
  case VMLA_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) += at_(1, float) * at_(2, float);
    }
    break;
  default:
    assert(0);
  }
#undef case_
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

void compute_data_offset(CompileUnit &c) {
  c.for_each([](MemScope &s) {
    s.size = 0;
    s.for_each([&](MemObject *x) {
      x->offset = s.size;
      s.size += x->size;
    });
  });
}

int exec(CompileUnit &c) {
  compute_data_offset(c);
  // std::cerr<<">>> exec"<<std::endl;
  // simulate IR execute result
  FILE *ifile = fopen("input.txt", "r");
  FILE *ofile = fopen("output.txt", "w");
  bool dbg_step_on = global_config.args["exec-step"] == "1";
  bool ENABLE_PHI = global_config.args["exec-phi"] == "1";
  long long instr_cnt = 0, mem_r_cnt = 0, mem_w_cnt = 0, jump_cnt = 0,
            par_instr_cnt = 0;
  int fork_cnt = 0;
  int sp = c.scope.size, mem_limit = sp + (8 << 20);
  char *mem = new char[mem_limit];
  auto wMem = [&](int addr, typeless_scalar_t v) {
    assert(0 <= addr && addr < mem_limit && !(addr % 4));
    ((typeless_scalar_t *)mem)[addr / 4] = v;
    ++mem_w_cnt;
  };
  auto rMem = [&](int addr) -> typeless_scalar_t {
    assert(0 <= addr && addr < mem_limit && !(addr % 4));
    ++mem_r_cnt;
    return ((typeless_scalar_t *)mem)[addr / 4];
  };
  c.scope.for_each([&](MemObject *x) {
    assert(x->global);
    if (x->initial_value) {
      memcpy(mem + x->offset, x->initial_value, x->size);
    } else {
      memset(mem + x->offset, 0, x->size);
    }
  });
  auto dbg_pr = [&] {
    printf("Instr: %lld\n", instr_cnt);
    printf("Load:  %lld\n", mem_r_cnt);
    printf("Store: %lld\n", mem_w_cnt);
    printf("Jump:  %lld\n", jump_cnt);
    printf("Fork:  %d\n", fork_cnt);
    printf("Parallel:  %lld\n", par_instr_cnt);
    /*c.scope.for_each([&](MemObject *x){
            assert(x->global);
            printf("%s: ",x->name.data());
            int *data=(int*)(mem+x->offset),size=x->size;
            for(int i=0;i<size/4;++i)printf("%d,",data[i]);
            printf("\n");
    });*/
  };
  bool eol = 1;
  typedef std::list<std::unique_ptr<Instr>>::iterator it_t;
  struct ThreadContext {
    BB *cur_bb = NULL;
    it_t cur_instr;
    int id = -1, waiting_id = -1;
    bool waiting = 0;
    std::optional<std::pair<Reg, int32_t>> write_reg;
    typeless_scalar_t simd_regs[8][4];
  } cur_thread;
  cur_thread.id = 0;
  std::queue<ThreadContext> threads;
  std::unordered_set<int> finished_threads;
  BB *&cur = cur_thread.cur_bb;
  it_t &it = cur_thread.cur_instr;
  std::vector<std::pair<BB *, it_t>> call_stack;

  /*auto skip_instr = [&](Instr *) {
    --instr_cnt;
  };*/
  std::function<typeless_scalar_t(NormalFunc *, std::vector<typeless_scalar_t>)>
      run;
  run = [&](NormalFunc *func,
            std::vector<typeless_scalar_t> args) -> typeless_scalar_t {
    BB *last_bb = NULL;
    cur = func->entry;
    assert(func->bbs.size());
    int sz = func->scope.size;
    std::unordered_map<int, typeless_scalar_t> regs, tmps;
    auto wReg = [&](Reg x, typeless_scalar_t v) {
      assert(1 <= x.id && x.id <= func->max_reg_id);
      if (dbg_step_on)
        dbg(x, " := ", v.int_value(), '\n');
      regs[x.id] = v;
    };
    auto rReg = [&](Reg x) -> typeless_scalar_t {
      assert(1 <= x.id && x.id <= func->max_reg_id);
      return regs[x.id];
    };

    int time_to_last_schedule = 0;
    bool dbg_thread_on = 0;

    auto schedule = [&]() {
      assert(!threads.empty());
      time_to_last_schedule = 0;
      for (;;) {
        cur_thread = threads.front();
        threads.pop();
        if (cur_thread.waiting &&
            !finished_threads.count(cur_thread.waiting_id)) {
          if (dbg_thread_on)
            dbg(cur_thread.id, " waiting ", cur_thread.waiting_id, '\n');
          threads.push(cur_thread);
          continue;
        }
        cur_thread.waiting = 0;
        if (auto w = cur_thread.write_reg) {
          wReg(w->first, w->second);
          cur_thread.write_reg = std::nullopt;
        }
        if (dbg_thread_on)
          dbg("schedule: ", cur_thread.id, '\n');
        break;
      }
    };
    for (int i = 0; i < (int)args.size(); ++i) {
      wReg(i + 1, args[i]);
    }
    typeless_scalar_t _ret = 0;
    while (cur) {
      // printf("BB: %s\n",cur->name.data());
      tmps.clear();
      if (ENABLE_PHI)
        for (it = cur->instrs.begin(); it != cur->instrs.end(); ++it) {
          Instr *x0 = it->get();
          Case(PhiInstr, x, x0) {
            for (auto &kv : x->uses)
              if (last_bb == kv.second)
                tmps[x->d1.id] = rReg(kv.first);
          }
        }
      for (it = cur->instrs.begin(); it != cur->instrs.end(); ++it) {
        Instr *x0 = it->get();
        ++instr_cnt;
        if (threads.size())
          ++par_instr_cnt;
        ++time_to_last_schedule;
        Case(PhiInstr, x, x0) {
          if (ENABLE_PHI)
            wReg(x->d1, tmps.at(x->d1.id));
          else
            assert(0);
        }
        else Case(LoadAddr, x, x0) {
          wReg(x->d1, (x->offset->global ? 0 : sp) + x->offset->offset);
        }
        else Case(LoadConst<int32_t>, x, x0) {
          wReg(x->d1, x->value);
        }
        else Case(LoadConst<float>, x, x0) {
          wReg(x->d1, x->value);
        }
        else Case(LoadArg<ScalarType::Int>, x, x0) {
          wReg(x->d1, args.at(x->id));
        }
        else Case(LoadArg<ScalarType::Float>, x, x0) {
          wReg(x->d1, args.at(x->id));
        }
        else Case(UnaryOpInstr, x, x0) {
          typeless_scalar_t s1 = rReg(x->s1), d1 = x->compute(s1);
          wReg(x->d1, d1);
        }
        else Case(BinaryOpInstr, x, x0) {
          typeless_scalar_t s1 = rReg(x->s1), s2 = rReg(x->s2),
                            d1 = x->compute(s1, s2);
          wReg(x->d1, d1);
        }
        else Case(ArrayIndex, x, x0) {
          int32_t s1 = rReg(x->s1).int_value(), s2 = rReg(x->s2).int_value(),
                  d1 = s1 + s2 * x->size;
          wReg(x->d1, d1);
        }
        else Case(SIMDInstr, x, x0) {
          switch (x->type) {
          case SIMDInstr::VDUP_32: {
            auto s1 = rReg(*x->s1);
            for (int i = 0; i < 4; ++i) {
              cur_thread.simd_regs[x->regs[0]][i] = s1;
            }
            break;
          }
          case SIMDInstr::VLDM: {
            int s1 = rReg(*x->s1).int_value();
            for (int j = 0; j < x->size; ++j) {
              for (int i = 0; i < 4; ++i) {
                cur_thread.simd_regs[x->regs[0] + j][i] =
                    rMem(s1 + i * 4 + j * 16);
              }
            }
            break;
          }
          case SIMDInstr::VSTM: {
            int s1 = rReg(*x->s1).int_value();
            for (int j = 0; j < x->size; ++j) {
              for (int i = 0; i < 4; ++i) {
                wMem(s1 + i * 4 + j * 16,
                     cur_thread.simd_regs[x->regs[0] + j][i]);
              }
            }
            break;
          }
          default:
            x->compute(cur_thread.simd_regs);
            break;
          }
        }
        else Case(LoadInstr, x, x0) {
          int addr = rReg(x->addr).int_value() + x->offset;
          if (x->reg_offset) {
            addr += rReg(*x->reg_offset).int_value() * 4;
          }
          wReg(x->d1, rMem(addr));
        }
        else Case(StoreInstr, x, x0) {
          int addr = rReg(x->addr).int_value() + x->offset;
          if (x->reg_offset) {
            addr += rReg(*x->reg_offset).int_value() * 4;
          }
          wMem(addr, rReg(x->s1));
        }
        else Case(JumpInstr, x, x0) {
          last_bb = cur;
          cur = x->target;
          if (last_bb->id + 1 != cur->id)
            ++jump_cnt;
          break;
        }
        else Case(BranchInstr, x, x0) {
          last_bb = cur;
          cur = (rReg(x->cond).int_value() ? x->target1 : x->target0);
          if (last_bb->id + 1 != cur->id)
            ++jump_cnt;
          break;
        }
        else Case(ReturnInstr<ScalarType::Int>, x, x0) {
          _ret = rReg(x->s1);
          last_bb = cur;
          cur = NULL;
          jump_cnt += 2;
          break;
        }
        else Case(ReturnInstr<ScalarType::Float>, x, x0) {
          _ret = rReg(x->s1);
          last_bb = cur;
          cur = NULL;
          jump_cnt += 2;
          break;
        }
        else Case(CallInstr, x, x0) {
          typeless_scalar_t ret = 0;
          std::vector<typeless_scalar_t> args;
          for (auto kv : x->args)
            args.push_back(rReg(kv.first));
          Case(NormalFunc, f, x->f) {
            sp += sz;
            call_stack.emplace_back(cur, it);
            ret = run(f, args);
            std::tie(cur, it) = call_stack.back();
            call_stack.pop_back();
            sp -= sz;
            if (!f->ignore_return_value)
              wReg(x->d1, ret);
          }
          else {
#define FLOAT_FMT "%a"
            if (x->f->name == "__lock") {
              assert(args.size() == 1);
              int addr = args[0].int_value();
              if (rMem(addr).int_value() == 0) {
                wMem(addr, 1);
              } else {
                threads.push(cur_thread);
                schedule();
              }
              continue;
            } else if (x->f->name == "__unlock") {
              assert(args.size() == 1);
              int addr = args[0].int_value();
              wMem(addr, 0);
              continue;
            } else if (x->f->name == "__create_threads") {
              assert(args.size() == 0);
              threads.push(ThreadContext{
                  cur, it, ++fork_cnt, -1, 0, std::make_pair(x->d1, 0), {}});
              cur_thread.waiting_id = fork_cnt;
              if (dbg_thread_on)
                dbg(cur_thread.id, " fork ", fork_cnt, "\n");
              wReg(x->d1, 1);
              time_to_last_schedule = 0;
              // return 1: wait on join
              // return 0: exit on join
              continue;
              // std::cerr<<">>> fork"<<std::endl;
            } else if (x->f->name == "__join_threads") {
              assert(args.size() == 1);
              assert(args[0].int_value() >= 0);
              assert(args[0].int_value() <= 1);
              if (args[0].int_value()) {
                finished_threads.insert(cur_thread.id);
                if (dbg_thread_on)
                  dbg(cur_thread.id, " exited\n");
              } else {
                cur_thread.waiting = 1;
                if (dbg_thread_on)
                  dbg(cur_thread.id, " wait ", cur_thread.waiting_id, '\n');
                threads.push(cur_thread);
              }
              schedule();
              continue;
              // std::cerr<<">>> join"<<std::endl;
            } else
              assert(threads.empty());
            if (x->f->name == "getint") {
              assert(args.size() == 0);
              assert(fscanf(ifile, "%d", &ret.int_value()) == 1);
            } else if (x->f->name == "getfloat") {
              assert(args.size() == 0);
              assert(fscanf(ifile, FLOAT_FMT, &ret.float_value()) == 1);
            } else if (x->f->name == "getch") {
              assert(args.size() == 0);
              ret = fgetc(ifile);
            } else if (x->f->name == "getarray") {
              assert(args.size() == 1);
              assert(fscanf(ifile, "%d", &ret.int_value()) == 1);
              for (int i = 0, x; i < ret.int_value(); ++i) {
                assert(fscanf(ifile, "%d", &x) == 1);
                wMem(args[0].int_value() + i * 4, x);
              }
            } else if (x->f->name == "getfarray") {
              assert(args.size() == 1);
              assert(fscanf(ifile, "%d", &ret.int_value()) == 1);
              for (int i = 0; i < ret.int_value(); ++i) {
                float x;
                assert(fscanf(ifile, FLOAT_FMT, &x) == 1);
                wMem(args[0].int_value() + i * 4, x);
              }
            } else if (x->f->name == "putint") {
              assert(args.size() == 1);
              fprintf(ofile, "%d", args[0].int_value());
              fflush(ofile);
              eol = 0;
            } else if (x->f->name == "putfloat") {
              assert(args.size() == 1);
              fprintf(ofile, FLOAT_FMT, args[0].float_value());
              fflush(ofile);
              eol = 0;
            } else if (x->f->name == "putch") {
              assert(args.size() == 1);
              fputc(args[0].int_value(), ofile);
              fflush(ofile);
              eol = (args[0].int_value() == 10);
            } else if (x->f->name == "putarray") {
              assert(args.size() == 2);
              int n = args[0].int_value(), a = args[1].int_value();
              fprintf(ofile, "%d:", n);
              for (int i = 0; i < n; ++i)
                fprintf(ofile, " %d", rMem(a + i * 4).int_value());
              fputc(10, ofile);
              fflush(ofile);
              eol = 1;
            } else if (x->f->name == "putfarray") {
              assert(args.size() == 2);
              int n = args[0].int_value(), a = args[1].int_value();
              fprintf(ofile, "%d:", n);
              for (int i = 0; i < n; ++i)
                fprintf(ofile, " " FLOAT_FMT, rMem(a + i * 4).float_value());
              fputc(10, ofile);
              fflush(ofile);
              eol = 1;
            } else if (x->f->name == "putf") {
              /*char *c=mem+args[0];
              char buf[1024];
              if(args.size()==1)sprintf(buf,c);
              else if(args.size()==2)sprintf(buf,c,args[1]);
              else if(args.size()==3)sprintf(buf,c,args[1],args[2]);
              else if(args.size()==4)sprintf(buf,c,args[1],args[2],args[3]);
              else
              if(args.size()==5)sprintf(buf,c,args[1],args[2],args[3],args[4]);
              else assert(0);
              fflush(ofile);
              int len=strlen(buf);
              fputs(buf,ofile);
              if(len)eol=(buf[len-1]==10);*/
              assert(0);
            } else if (x->f->name == "starttime") {
            } else if (x->f->name == "stoptime") {
            } else
              Case(LibFunc, f0, x->f) {
                if (f0->impl) {
                  ret = (*(f0->impl))(args.data(), (int)args.size());
                } else {
                  std::cerr << "unimplemented func: " << x->f->name
                            << std::endl;
                  assert(0);
                }
              }
            else {
              std::cerr << "unknown func: " << x->f->name << std::endl;
              assert(0);
            }
          }
          if (!x->f->ignore_return_value && x->return_type != ScalarType::Void)
            wReg(x->d1, ret);
        }
        else assert(0);
        if (time_to_last_schedule > 32) {
          threads.push(cur_thread);
          schedule();
        }
      }
    }
    return _ret;
  };
  int ret = run(c.main(), {}).int_value();
  dbg_pr();
  delete[] mem;
  if (!eol)
    fputc(10, ofile);
  fprintf(ofile, "%u\n", ret & 255);
  printf("return %d\n", ret);
  if (ifile)
    fclose(ifile);
  if (ofile)
    fclose(ofile);
  return ret;
}

void print_all_bb(CompileUnit &c, std::ostream &os) {
  c.for_each([&](IR::NormalFunc *func) {
    func->for_each([&](IR::BB *bb) { bb->print(os); });
  });
}
} // namespace IR
