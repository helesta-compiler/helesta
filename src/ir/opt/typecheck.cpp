#include "ir/opt/dag_ir.hpp"

struct TypeCheck : InstrVisitor {
  enum Type {
    Int,
    Float,
    Addr,
  };
  typedef std::variant<Type, Reg> node_t;
  friend std::ostream &operator<<(std::ostream &os, node_t x) {
    if (std::holds_alternative<Type>(x)) {
      switch (std::get<Type>(x)) {
      case Int:
        os << "int";
        break;
      case Float:
        os << "float";
        break;
      case Addr:
        os << "addr";
        break;
      }
    } else {
      os << std::get<Reg>(x);
    }
    return os;
  }
  UnionFind<node_t> mp;
  NormalFunc *f;
  TypeCheck(NormalFunc *_f) : f(_f) {}
  Type type(ScalarType x) {
    if (x == ScalarType::Int)
      return Int;
    if (x == ScalarType::Float)
      return Float;
    assert(0);
    return Int;
  }
  bool success() {
    return mp[Int] != mp[Float] && mp[Int] != mp[Addr] && mp[Float] != mp[Addr];
  }
  void visit(Instr *w0) override {
    // ::info << *w0 << '\n';
    auto merge = [&](node_t x, node_t y) {
      // ::info << x << " merge " << y << '\n';
      mp.merge(x, y);
      if (mp[Int] == mp[Float] || mp[Addr] == mp[Float]) {
        ::info << "bad type: " << *w0 << '\n';
        ::debug << '\n' << *f << '\n';
        assert(0);
      }
    };
    Case(LoadAddr, w, w0) { merge(w->d1, Addr); }
    else Case(LoadConst<int32_t>, w, w0) {
      merge(w->d1, Int);
    }
    else Case(LoadConst<float>, w, w0) {
      merge(w->d1, Float);
    }
    else Case(LoadArg<ScalarType::Int>, w, w0) {
      (void)w;
    }
    else Case(LoadArg<ScalarType::Float>, w, w0) {
      (void)w;
    }
    else Case(UnaryOpInstr, w, w0) {
      if (w->op.type == UnaryCompute::ID) {
        merge(w->d1, w->s1);
      } else {
        merge(w->d1, type(w->op.ret_type()));
        merge(w->s1, type(w->op.input_type()));
      }
    }
    else Case(BinaryOpInstr, w, w0) {
      merge(w->d1, type(w->op.ret_type()));
      merge(w->s1, type(w->op.input_type()));
      merge(w->s2, type(w->op.input_type()));
    }
    else Case(ArrayIndex, w, w0) {
      merge(w->d1, Addr);
      merge(w->s1, Addr);
      merge(w->s2, Int);
    }
    else Case(LoadInstr, w, w0) {
      merge(w->addr, Addr);
    }
    else Case(StoreInstr, w, w0) {
      merge(w->addr, Addr);
    }
    else Case(JumpInstr, w, w0) {
      (void)w;
    }
    else Case(BranchInstr, w, w0) {
      merge(w->cond, Int);
    }
    else Case(ReturnInstr<ScalarType::Int>, w, w0) {
      (void)w;
    }
    else Case(ReturnInstr<ScalarType::Float>, w, w0) {
      (void)w;
    }
    else Case(CallInstr, w, w0) {
      if (w->return_type == ScalarType::Float)
        merge(w->d1, Float);
      else if (w->return_type == ScalarType::Int)
        merge(w->d1, Int);
    }
    else Case(PhiInstr, w, w0) {
      for (auto &kv : w->uses) {
        merge(w->d1, kv.first);
      }
    }
    else assert(0);
  }
};

bool type_check(NormalFunc *f) {
  DAG_IR dag(f);
  TypeCheck w(f);
  dag.visit(w);
  return w.success();
}
