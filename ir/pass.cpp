#include "ir/pass.hpp"

#include <cstdlib>

#include "common/common.hpp"

using namespace IR;

DebugStream dbg;

void get_defs(std::unordered_map<Reg, RegWriteInstr *> &def, BB *w) {
  w->for_each([&](Instr *x) {
    Case(RegWriteInstr, x0, x) {
      if (def.count(x0->d1)) {
        dbg << "multiple def: " << x0->d1 << "\n";
        dbg << *w << "\n";
        assert(0);
      }
      def[x0->d1] = x0;
    }
  });
}
void get_use_count(std::unordered_map<Reg, int> &uses, BB *w) {
  w->for_each([&](Instr *i) { i->map_use([&](Reg &r) { ++uses[r]; }); });
}

std::unordered_map<Reg, RegWriteInstr *> build_defs(NormalFunc *f) {
  std::unordered_map<Reg, RegWriteInstr *> def;
  SetPrintContext _(f);
  f->for_each([&](BB *w) { get_defs(def, w); });
  return def;
}

std::unordered_map<Reg, int> build_const_int(NormalFunc *f) {
  std::unordered_map<Reg, int> def;
  SetPrintContext _(f);
  f->for_each([&](Instr *x) {
    Case(LoadConst, x0, x) { def[x0->d1] = x0->value; }
  });
  return def;
}

std::unordered_map<Instr *, BB *> build_in2bb(NormalFunc *f) {
  std::unordered_map<Instr *, BB *> def;
  f->for_each([&](BB *bb) { bb->for_each([&](Instr *i) { def[i] = bb; }); });
  return def;
}

std::unordered_map<Reg, int> build_use_count(NormalFunc *f) {
  std::unordered_map<Reg, int> uses;
  f->for_each([&](BB *w) { get_use_count(uses, w); });
  return uses;
}

void array_index_to_muladd(NormalFunc *f) {
  f->for_each([&](BB *bb) {
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Case(ArrayIndex, x, it->get()) {
        Reg offset = f->new_Reg(), size = f->new_Reg();
        f->entry->push_front(new LoadConst(size, x->size));
        bb->instrs.insert(it, std::unique_ptr<Instr>(new BinaryOpInstr(
                                  offset, x->s2, size, BinaryOp::MUL)));
        *it = std::unique_ptr<Instr>(
            new BinaryOpInstr(x->d1, x->s1, offset, BinaryOp::ADD));
      }
    }
  });
}

void cfg_check(CompileUnit &c) {
  dbg << "```cpp\n" << c << "```\n\n";
  c.for_each([&](NormalFunc *f) {
    auto defs = build_defs(f);
    std::unordered_set<MemObject *> local_vars;
    f->for_each([&](BB *bb) {
      assert(bb->instrs.size());
      auto last = bb->back();
      bb->for_each([&](Instr *i) {
        i->map_use([&](Reg &r) {
          if (!defs.count(r)) {
            std::cerr << "use before def: " << r << ": " << f->get_name(r)
                      << " in " << f->name << "\n";
            assert(0);
          }
        });
        Case(ControlInstr, _, i) { assert(i == last); }
        else {
          assert(i != last);
          Case(LoadAddr, i0, i) {
            if (i0->offset->global) {
              assert(c.scope.has(i0->offset));
            } else {
              assert(f->scope.has(i0->offset));
            }
          }
          else Case(LocalVarDef, i0, i) {
            assert(f->scope.has(i0->data));
            assert(local_vars.insert(i0->data).second);
          }
        }
      });
    });
    f->scope.for_each([&](MemObject *m) {
      if (!local_vars.count(m)) {
        f->entry->push_front(new LocalVarDef(m));
        local_vars.insert(m);
        std::cerr << "[Warning] no LocalVarDef of " << *m << "\n";
        assert(0);
      }
    });
    assert(local_vars.size() == f->scope.objects.size());
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

void code_reorder_naive(NormalFunc *f) {
  auto S = build_dom_tree(f);
  std::function<void(BB *)> dfs;
  std::unordered_map<BB *, int> order;
  int tk = 0;
  dfs = [&](BB *w) {
    if (order[w])
      return;
    order[w] = ++tk;
    Instr *x = w->back();
    Case(JumpInstr, y, x) { dfs(y->target); }
    else Case(BranchInstr, y, x) {
      dfs(y->target1);
      dfs(y->target0);
    }
    else Case(ReturnInstr, y, x) {
      ;
    }
    else assert(0);
  };
  dfs(f->entry);
  std::sort(f->bbs.begin(), f->bbs.end(),
            [&](const std::unique_ptr<BB> &x, const std::unique_ptr<BB> &y) {
              return order[x.get()] < order[y.get()];
            });
}

void code_reorder(NormalFunc *f) {
  auto prob = estimate_BB_prob(f);
  auto S = build_dom_tree(f);
  std::function<void(BB *)> dfs;
  int tk = 0;
#if 1
  f->for_each([&](BB *bb) { bb->id = 0; });
  dfs = [&](BB *w) {
    if (w->id)
      return;
    w->id = ++tk;
    Instr *x = w->back();
    Case(JumpInstr, y, x) { dfs(y->target); }
    else Case(BranchInstr, y, x) {
      if (prob[y->target1] > prob[y->target0]) {
        dfs(y->target1);
        dfs(y->target0);
      } else {
        dfs(y->target0);
        dfs(y->target1);
      }
    }
    else Case(ReturnInstr, y, x) {
      ;
    }
    else assert(0);
  };
  dfs(f->entry);
  std::sort(f->bbs.begin(), f->bbs.end(),
            [&](const std::unique_ptr<BB> &x, const std::unique_ptr<BB> &y) {
              return x->id < y->id;
            });
#else
  f->for_each([&](BB *bb) { bb->id = ++tk; });
#endif
}

void compute_thread_local(CompileUnit &c) {
  auto fork = c.lib_funcs.at("__create_threads").get();
  auto join = c.lib_funcs.at("__join_threads").get();
  c.for_each([&](NormalFunc *f) {
    auto S = build_dom_tree(f);
    f->for_each([&](BB *bb) {
      bool flag = 0;
      bb->for_each([&](Instr *x) {
        Case(CallInstr, call, x) {
          if (call->f == fork) {
            flag = 1;
          } else if (call->f == join) {
            flag = 0;
          }
        }
        if (flag)
          Case(RegWriteInstr, x0, x) { f->thread_local_regs.insert(x0->d1); }
      });
      if (flag) {
        std::unordered_set<BB *> visited;
        std::queue<BB *> q;
        auto visit = [&](BB *bb) {
          if (visited.count(bb))
            return;
          visited.insert(bb);
          q.push(bb);
        };
        visit(bb);
        while (!q.empty()) {
          BB *bb1 = q.front();
          q.pop();
          if (bb1 != bb) {
            bool flag = 1;
            bb1->for_each([&](Instr *x) {
              Case(CallInstr, call, x) {
                if (call->f == join) {
                  flag = 0;
                }
              }
              if (flag)
                Case(RegWriteInstr, x0, x) {
                  f->thread_local_regs.insert(x0->d1);
                }
            });
            if (!flag)
              continue;
          }
          for (BB *bb2 : S[bb1].out)
            visit(bb2);
        }
      }
    });
  });
}

MemScope *scope = NULL, *gscope = NULL;
BB *bb = NULL;
NormalFunc *__f = NULL;
CompileUnit *__c = NULL;

struct ProgDef {
  CompileUnit &c;
  ProgDef(CompileUnit &c) : c(c) {
    gscope = scope = &c.scope;
    __c = &c;
  }
  ~ProgDef() {
    gscope = NULL;
    __c = NULL;
  }
};

struct FuncDef {
  std::string name;
  Func *f;
  FuncDef(ProgDef &p, std::string name) : name(name) {
    f = __f = __c->new_NormalFunc(name);
    scope = &__f->scope;
  }
  operator Func *() { return f; }
};

struct Value {
  Reg r;
  Value(int x) {
    r = __f->new_Reg();
    bb->push(new LoadConst(r, x));
  }
  Value(Reg r) : r(r) {}
};

#define DEF_BIN_OP(op, name)                                                   \
  Value operator op(const Value &a, const Value &b) {                          \
    Value c(__f->new_Reg());                                                   \
    bb->push(new BinaryOpInstr(c.r, a.r, b.r, BinaryOp::name));                \
    return c;                                                                  \
  }

DEF_BIN_OP(+, ADD)
DEF_BIN_OP(-, SUB)
DEF_BIN_OP(*, MUL)
DEF_BIN_OP(/, DIV)
DEF_BIN_OP(<, LESS)
DEF_BIN_OP(<=, LEQ)
DEF_BIN_OP(==, EQ)
DEF_BIN_OP(!=, NEQ)
DEF_BIN_OP(%, MOD)

Value operator>(const Value &a, const Value &b) { return b < a; }
Value operator>=(const Value &a, const Value &b) { return b <= a; }

#define DEF_UOP(op, name)                                                      \
  Value operator op(Value &a) {                                                \
    Value c(__f->new_Reg());                                                   \
    bb->push(new UnaryOpInstr(c.r, a.r, UnaryOp::name));                       \
    return c;                                                                  \
  }

DEF_UOP(-, NEG)
DEF_UOP(+, ID)
DEF_UOP(!, LNOT)

struct LValue {
  std::string name;
  Reg r;
  std::vector<int> dims;
  LValue(std::string name, Reg r, std::vector<int> dims)
      : name(name), r(r), dims(dims) {}
  void operator=(const Value &v) const {
    assert(dims.empty());
    bb->push(new StoreInstr(r, v.r));
  }
  void operator=(const LValue &v) { *this = Value(v); }
  void operator=(const LValue &v) const { *this = Value(v); }
  operator Value() const {
    Reg r0 = __f->new_Reg();
    bb->push(new LoadInstr(r0, r));
    return Value(r0);
  }
  LValue operator[](const Value &b) const {
    assert(dims.size());
    auto dims0 = dims;
    dims0.erase(dims0.begin());
    int sz = 4;
    for (int x : dims0)
      sz *= x;
    Reg r0 = __f->new_Reg();
    bb->push(new ArrayIndex(r0, r, b.r, sz, dims[0]));
    // return LValue(name,(Value(r)+b*sz).r,dims0);
    return LValue(name, r0, dims0);
  }
};

struct VarDef {
  std::string name;
  std::vector<int> dims;
  MemObject *m;
  VarDef(std::string name, std::vector<int> dims = {})
      : name(name), dims(dims) {
    m = scope->new_MemObject(name);
    m->size = 4;
    for (int x : dims)
      m->size *= x;
    if (__f) {
      assert(bb);
      bb->push(new LocalVarDef(m));
      dbg << "local var: " << *m << "\n";
    } else {
      dbg << "global var: " << *m << "\n";
    }
  }
  void operator=(const VarDef &v) const { *this = Value(v); }
  ~VarDef() {}
  LValue operator[](const Value &b) {
    assert(dims.size());
    LValue a(name, la(), dims);
    return a[b];
  }
  Reg la() const {
    Reg r0 = __f->new_Reg();
    bb->push(new LoadAddr(r0, m));
    return r0;
  }
  void operator=(const Value &v) const {
    assert(dims.empty());
    LValue a(name, la(), dims);
    a = v;
  }
  operator Value() const { return LValue(name, la(), dims); }
};

struct _BB {
  BB *w;
};

#define newbb(name)                                                            \
  _BB name{__f->new_BB(#name)};                                                \
  if (!__f->entry)                                                             \
    __f->entry = name.w;
#define func(name)                                                             \
  FuncDef name(ctx, #name);                                                    \
  {
#define endfunc                                                                \
  __f = NULL;                                                                  \
  scope = gscope;                                                              \
  }
#define var VarDef
#define defbb(name) bb = name.w;
#define endbb                                                                  \
  Case(ControlInstr, _, bb->back());                                           \
  else assert(0);                                                              \
  bb = NULL;

void Goto(const _BB &x) { bb->push(new JumpInstr(x.w)); }
void Branch(Value v, const _BB &t1, const _BB &t2) {
  bb->push(new BranchInstr(v.r, t1.w, t2.w));
}
Value Call(Func *fn, std::vector<Value> args) {
  std::vector<Reg> regs;
  for (Value v : args)
    regs.push_back(v.r);
  Reg r = __f->new_Reg();
  bb->push(new CallInstr(r, fn, regs, 0));
  return Value(r);
}
void Return(Value v) { bb->push(new ReturnInstr(v.r, 0)); }
void Return() {
  Reg r = __f->new_Reg();
  bb->push(new ReturnInstr(r, 1));
}
Value Arg(int id) {
  Reg r = __f->new_Reg();
  bb->push(new LoadArg(r, id));
  return r;
}
