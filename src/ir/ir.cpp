#include "ir/ir.hpp"
#include <cstring>

namespace IR {

PrintContext print_ctx;

ostream &operator<<(ostream &os, const Printable &r) {
  r.print(os);
  return os;
}

void Reg::print(ostream &os) const {
  auto f = print_ctx.f;
  if (f)
    os << f->get_name(*this) << "(" << id << ")";
  else
    os << "R[" << id << "]";
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

const std::vector<BB *> &BB::getOutNodes() const {
  auto last = back();
  if (auto jump_instr = dynamic_cast<JumpInstr *>(last)) {
    return std::move(std::vector<BB *>{jump_instr->target});
  } else if (auto branch_instr = dynamic_cast<BranchInstr *>(last)) {
    return std::move(
        std::vector<BB *>{branch_instr->target0, branch_instr->target1});
  } else {
    assert(dynamic_cast<ReturnInstr *>(last) != nullptr);
  }
  return std::move(std::vector<BB *>{});
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
template <typename Scalar> void LoadConst<Scalar>::print(ostream &os) const {
  os << d1 << " = " << value;
}
void LoadArg::print(ostream &os) const { os << d1 << " = arg" << id; }
void ArrayIndex::print(ostream &os) const {
  os << d1 << " = " << s1 << " + " << s2 << " * " << size << " : " << limit;
}
void LocalVarDef::print(ostream &os) const { os << "define " << data->name; }
void UnaryOpInstr::print(ostream &os) const {
  os << d1 << " = " << op << " " << s1;
}
void BinaryOpInstr::print(ostream &os) const {
  os << d1 << " = " << s1 << " " << op << " " << s2;
}
void LoadInstr::print(ostream &os) const { os << d1 << " = M[" << addr << "]"; }
void StoreInstr::print(ostream &os) const {
  os << "M[" << addr << "] = " << s1;
}
void JumpInstr::print(ostream &os) const { os << "goto " << target->name; }
void BranchInstr::print(ostream &os) const {
  os << "goto " << cond << " ? " << target1->name << " : " << target0->name;
}
void ReturnInstr::print(ostream &os) const { os << "return " << s1; }

void CallInstr::print(ostream &os) const {
  os << d1 << " = " << f->name;
  char c = '(';
  for (auto s : args) {
    os << c << s;
    c = ',';
  }
  if (c == '(')
    os << c;
  os << ')';
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
void MemDef::print(ostream &os) const {
  os << d1 << " = "
     << " memdef " << data->name;
}
void MemUse::print(ostream &os) const { os << s1 << " used"; }
void MemEffect::print(ostream &os) const {
  os << d1 << " = " << s1 << " updated";
}
void MemRead::print(ostream &os) const {
  os << d1 << " = " << mem << " at " << addr;
}
void MemWrite::print(ostream &os) const {
  os << d1 << " = " << mem << " at " << addr << " write " << s1;
}

CompileUnit::CompileUnit() : scope("global", 1) {
  LibFunc *f;
  f = new_LibFunc("__create_threads", 0);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("__join_threads", 1);
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
  Case(LoadArg, w, this) {
    auto u = w;
    if (copy)
      u = new LoadArg(*w);
    f1(u->d1);
    return u;
  }
  Case(LocalVarDef, w, this) {
    auto u = w;
    if (copy)
      u = new LocalVarDef(*w);
    f3(u->data);
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
    return u;
  }
  Case(StoreInstr, w, this) {
    auto u = w;
    if (copy)
      u = new StoreInstr(*w);
    f1(u->addr);
    f1(u->s1);
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
  Case(ReturnInstr, w, this) {
    auto u = w;
    if (copy)
      u = new ReturnInstr(*w);
    f1(u->s1);
    return u;
  }
  Case(CallInstr, w, this) {
    auto u = w;
    if (copy)
      u = new CallInstr(*w);
    f1(u->d1);
    for (auto &x : u->args)
      f1(x);
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
  Case(MemDef, w, this) {
    auto u = w;
    if (copy)
      u = new MemDef(*w);
    f1(u->d1);
    f3(u->data);
    return u;
  }
  Case(MemUse, w, this) {
    auto u = w;
    if (copy)
      u = new MemUse(*w);
    f1(u->s1);
    f3(u->data);
    return u;
  }
  Case(MemEffect, w, this) {
    auto u = w;
    if (copy)
      u = new MemEffect(*w);
    f1(u->d1);
    f1(u->s1);
    f3(u->data);
    return u;
  }
  Case(MemRead, w, this) {
    auto u = w;
    if (copy)
      u = new MemRead(*w);
    f1(u->d1);
    f1(u->mem);
    f1(u->addr);
    f3(u->data);
    return u;
  }
  Case(MemWrite, w, this) {
    auto u = w;
    if (copy)
      u = new MemWrite(*w);
    f1(u->d1);
    f1(u->mem);
    f1(u->addr);
    f1(u->s1);
    f3(u->data);
    return u;
  }
  assert(0);
  return NULL;
}

scalar_t UnaryOpInstr::compute(scalar_t s1) { return op.compute(s1); }
scalar_t UnaryOp::compute(scalar_t s1) {
  int32_t i1 = s1.int_value();
  float f1 = s1.float_value();
  union {
    double d;
    float f0, f1;
  } f2d;
  switch (type) {
  case UnaryOp::LNOT:
    return int32_t(!i1);
  case UnaryOp::NEG:
    return -i1;
  case UnaryOp::ID:
    return i1;
  case UnaryOp::FNEG:
    return -f1;
  case UnaryOp::F2I:
    return int32_t(f1);
  case UnaryOp::I2F:
    return float(i1);
  case UnaryOp::F2D0:
    f2d.d = f1;
    return f2d.f0;
  case UnaryOp::F2D1:
    f2d.d = f1;
    return f2d.f1;
  default:
    assert(0);
    return 0;
  }
}

scalar_t BinaryOpInstr::compute(scalar_t s1, scalar_t s2) {
  return op.compute(s1, s2);
}
scalar_t BinaryOp::compute(scalar_t s1, scalar_t s2) {
  int32_t i1 = s1.int_value(), i2 = s2.int_value();
  float f1 = s1.float_value(), f2 = s2.float_value();
  switch (type) {
  case BinaryOp::ADD:
    return i1 + i2;
  case BinaryOp::SUB:
    return i1 - i2;
  case BinaryOp::MUL:
    return i1 * i2;
  case BinaryOp::DIV:
    return (i2 && !(i1 == -2147483648 && i2 == -1) ? i1 / i2 : 0);
  case BinaryOp::LESS:
    return int32_t(i1 < i2);
  case BinaryOp::LEQ:
    return int32_t(i1 <= i2);
  case BinaryOp::EQ:
    return int32_t(i1 == i2);
  case BinaryOp::NEQ:
    return int32_t(i1 != i2);
  case BinaryOp::MOD:
    return (i2 ? i1 % i2 : 0);
  case BinaryOp::FADD:
    return f1 + f2;
  case BinaryOp::FSUB:
    return f1 - f2;
  case BinaryOp::FMUL:
    return f1 * f2;
  case BinaryOp::FDIV:
    return f1 / f2;
  case BinaryOp::FLESS:
    return int32_t(f1 < f2);
  case BinaryOp::FLEQ:
    return int32_t(f1 <= f2);
  case BinaryOp::FEQ:
    return int32_t(f1 == f2);
  case BinaryOp::FNEQ:
    return int32_t(f1 != f2);
  default:
    assert(0);
    return 0;
  }
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
  long long instr_cnt = 0, mem_r_cnt = 0, mem_w_cnt = 0, jump_cnt = 0,
            fork_cnt = 0, par_instr_cnt = 0;
  int sp = c.scope.size, mem_limit = sp + (8 << 20);
  char *mem = new char[mem_limit];
  auto wMem = [&](int addr, scalar_t v) {
    assert(0 <= addr && addr < mem_limit && !(addr % 4));
    ((scalar_t *)mem)[addr / 4] = v;
    ++mem_w_cnt;
  };
  auto rMem = [&](int addr) -> scalar_t {
    assert(0 <= addr && addr < mem_limit && !(addr % 4));
    ++mem_r_cnt;
    return ((scalar_t *)mem)[addr / 4];
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
    printf("Fork:  %lld\n", fork_cnt);
    printf("Parallel:  %g\n", par_instr_cnt * 1. / instr_cnt);
    /*c.scope.for_each([&](MemObject *x){
            assert(x->global);
            printf("%s: ",x->name.data());
            int *data=(int*)(mem+x->offset),size=x->size;
            for(int i=0;i<size/4;++i)printf("%d,",data[i]);
            printf("\n");
    });*/
  };
  bool in_fork = 0;
  bool eol = 1;
  NormalFunc *fork_func = NULL;
  BB *fork_bb = NULL;
  std::list<std::unique_ptr<Instr>>::iterator fork_instr;

  auto skip_instr = [&](Instr *) {
    --instr_cnt;
    if (in_fork)
      --par_instr_cnt;
  };
  std::function<scalar_t(NormalFunc *, std::vector<scalar_t>)> run;
  run = [&](NormalFunc *func, std::vector<scalar_t> args) -> scalar_t {
    BB *last_bb = NULL;
    BB *cur = func->entry;
    int sz = func->scope.size;
    std::unordered_map<int, scalar_t> regs;
    auto wReg = [&](Reg x, scalar_t v) {
      assert(func->thread_local_regs.count(x) == in_fork);
      assert(1 <= x.id && x.id <= func->max_reg_id);
      regs[x.id] = v;
    };
    auto rReg = [&](Reg x) -> scalar_t {
      assert(1 <= x.id && x.id <= func->max_reg_id);
      return regs[x.id];
    };
    for (int i = 0; i < (int)args.size(); ++i) {
      wReg(i + 1, args[i]);
    }
    scalar_t _ret = 0;
    bool last_in_fork = in_fork;
    while (cur) {
      // printf("BB: %s\n",cur->name.data());
      for (auto it = cur->instrs.begin(); it != cur->instrs.end(); ++it) {
        Instr *x0 = it->get();
        ++instr_cnt;
        if (in_fork)
          ++par_instr_cnt;
        Case(PhiInstr, x, x0) {
          for (auto &kv : x->uses)
            if (last_bb == kv.second)
              wReg(x->d1, rReg(kv.first));
        }
        else Case(LoadAddr, x, x0) {
          wReg(x->d1, (x->offset->global ? 0 : sp) + x->offset->offset);
        }
        else Case(LoadConst<int>, x, x0) {
          wReg(x->d1, x->value);
        }
        else Case(LoadConst<float>, x, x0) {
          wReg(x->d1, x->value);
        }
        else Case(LoadArg, x, x0) {
          wReg(x->d1, args.at(x->id));
        }
        else Case(UnaryOpInstr, x, x0) {
          scalar_t s1 = rReg(x->s1), d1 = x->compute(s1);
          wReg(x->d1, d1);
        }
        else Case(BinaryOpInstr, x, x0) {
          scalar_t s1 = rReg(x->s1), s2 = rReg(x->s2), d1 = x->compute(s1, s2);
          wReg(x->d1, d1);
        }
        else Case(ArrayIndex, x, x0) {
          int32_t s1 = rReg(x->s1).int_value(), s2 = rReg(x->s2).int_value(),
                  d1 = s1 + s2 * x->size;
          wReg(x->d1, d1);
        }
        else Case(LocalVarDef, x, x0) {
          skip_instr(x);
        }
        else Case(LoadInstr, x, x0) {
          wReg(x->d1, rMem(rReg(x->addr).int_value()));
        }
        else Case(StoreInstr, x, x0) {
          wMem(rReg(x->addr).int_value(), rReg(x->s1));
        }
        else Case(MemDef, x, x0) {
          skip_instr(x);
        }
        else Case(MemUse, x, x0) {
          skip_instr(x);
        }
        else Case(MemEffect, x, x0) {
          skip_instr(x);
        }
        else Case(MemRead, x, x0) {
          wReg(x->d1, rMem(rReg(x->addr).int_value()));
        }
        else Case(MemWrite, x, x0) {
          wMem(rReg(x->addr).int_value(), rReg(x->s1));
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
        else Case(ReturnInstr, x, x0) {
          _ret = rReg(x->s1);
          last_bb = cur;
          cur = NULL;
          jump_cnt += 2;
          break;
        }
        else Case(CallInstr, x, x0) {
          scalar_t ret = 0;
          std::vector<scalar_t> args;
          for (Reg p : x->args)
            args.push_back(rReg(p));
          Case(NormalFunc, f, x->f) {
            sp += sz;
            ret = run(f, args);
            sp -= sz;
            if (!f->ignore_return_value)
              wReg(x->d1, ret);
          }
          else {
#define FLOAT_FMT "%a"
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
            } else if (x->f->name == "__create_threads") {
              assert(args.size() == 1);
              assert(args[0].int_value() >= 1);
              assert(!in_fork);
              fork_func = f;
              fork_bb = cur;
              fork_instr = it;
              in_fork = 1;
              ++fork_cnt;
              ret = args[0].int_value() - 1;
              // std::cerr<<">>> fork"<<std::endl;
            } else if (x->f->name == "__join_threads") {
              assert(args.size() == 2);
              assert(args[0].int_value() >= 0);
              assert(args[0].int_value() < args[1].int_value());
              assert(in_fork);
              assert(fork_func == f);
              if (args[0].int_value()) {
                ret = args[0].int_value() - 1;
                cur = fork_bb;
                it = fork_instr;
                Case(CallInstr, call, it->get()) { x = call; }
                else assert(0);
              } else {
                fork_func = NULL;
                fork_bb = NULL;
                ret = 0;
                in_fork = 0;
              }
              // std::cerr<<">>> join"<<std::endl;
            } else {
              assert(0);
            }
          }
          if (!x->f->ignore_return_value && !x->ignore_return_value)
            wReg(x->d1, ret);
        }
        else assert(0);
      }
    }
    assert(last_in_fork == in_fork);
    return _ret;
  };
  int ret = run(c.funcs["main"].get(), {}).int_value();
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

} // namespace IR
