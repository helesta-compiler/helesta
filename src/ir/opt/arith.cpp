#include "add_expr.hpp"
#include "ir/opt/dag_ir.hpp"
namespace IR {
struct Mod2Div : ForwardLoopVisitor<std::map<std::pair<Reg, Reg>, Reg>>,
                 Defs,
                 CounterOutput {
  Mod2Div(NormalFunc *_f) : Defs(_f), CounterOutput("Mod2Div") {}
  void visitBB(BB *bb) {
    auto &w = info[bb];
    w.out = w.in;
    bb->for_each([&](Instr *x) {
      Case(BinaryOpInstr, bop, x) {
        switch (bop->op.type) {
        case BinaryCompute::DIV:
          w.out[{bop->s1, bop->s2}] = bop->d1;
          break;
        case BinaryCompute::MOD: {
          if (auto c = get_const(bop->s2); c && *c == 2) {
            break;
          }
          auto key = std::make_pair(bop->s1, bop->s2);
          if (w.out.count(key)) {
            CodeGen cg(f);
            cg.reg(bop->d1).set_last_def(cg.reg(bop->s1) -
                                         cg.reg(w.out[key]) * cg.reg(bop->s2));
            bb->replace(std::move(cg.instrs));
            ++cnt;
          }
          break;
        }
        default:
          break;
        }
      }
    });
  }
};

void mod2div(NormalFunc *f) {
  DAG_IR dag(f);
  Mod2Div w(f);
  dag.visit(w);
}

struct MulDiv : ForwardLoopVisitor<std::map<Reg, Reg>>, Defs, CounterOutput {
  MulDiv(NormalFunc *_f) : Defs(_f), CounterOutput("MulDiv") {}
  void visitBB(BB *bb) {
    auto &w = info[bb];
    w.out = w.in;
    bb->for_each([&](Instr *x) {
      replace_reg(x);
      Case(BinaryOpInstr, bop, x) {
        if (bop->op.type == BinaryCompute::DIV) {
          Case(BinaryOpInstr, bop2, defs.at(bop->s1)) {
            // dbg(*bop2, " >>> ", *bop, '\n');
            if (bop2->op.type == BinaryCompute::MUL && bop2->s2 == bop->s2) {
              replace_reg(bb->cur_iter(), bop->d1, bop2->s1);
              ++cnt;
            }
          }
        }
      }
    });
  }
};

void muldiv(NormalFunc *f) {
  DAG_IR dag(f);
  MulDiv w(f);
  dag.visit(w);
}

void merge_inst(CompileUnit *ir, NormalFunc *f) {
  auto Int = ScalarType::Int;
  auto use_count = build_use_count(f);
  auto mla = ir->lib_funcs.at("__mla").get();
  auto mls = ir->lib_funcs.at("__mls").get();
  f->for_each([&](BB *bb) {
    std::unordered_map<Reg, std::pair<Reg, Reg>> muls;
    bb->for_each([&](Instr *x) {
      Case(BinaryOpInstr, bop, x) {
        switch (bop->op.type) {
        case BinaryCompute::ADD: {
          Reg s1 = bop->s1, s2 = bop->s2;
          if (!muls.count(s2))
            std::swap(s1, s2);
          if (muls.count(s2)) {
            auto [r1, r2] = muls.at(s2);
            bb->replace(new CallInstr(bop->d1, mla,
                                      {{s1, Int}, {r1, Int}, {r2, Int}}, Int));
          }
          break;
        }
        case BinaryCompute::SUB:
          if (muls.count(bop->s2)) {
            auto [r1, r2] = muls.at(bop->s2);
            bb->replace(new CallInstr(
                bop->d1, mls, {{bop->s1, Int}, {r1, Int}, {r2, Int}}, Int));
          }
          break;
        case BinaryCompute::MUL:
          if (use_count[bop->d1] == 1) {
            muls[bop->d1] = {bop->s1, bop->s2};
          }
          break;
        default:
          break;
        }
      }
    });
  });
  remove_unused_def_func(f);
}
} // namespace IR

struct LoadStoreOffset
    : ForwardLoopVisitor<
          std::map<std::pair<Reg, AddExpr>, std::pair<Reg, int32_t>>>,
      CounterOutput {
  std::unordered_map<Reg, AddExpr> add;
  std::map<Reg, std::pair<Reg, int32_t>> mp;
  LoadStoreOffset() : CounterOutput("LoadStoreOffset") {}
  void visitBB(BB *bb) {
    auto check = [&](Reg &addr, int32_t &offset) {
      assert(!offset);
      if (mp.count(addr)) {
        auto [r, c] = mp[addr];
        if (c) {
          addr = r;
          offset = c * 4;
          ++cnt;
        }
      }
    };
    auto &w = info[bb];
    w.out = w.in;
    bb->for_each([&](Instr *x) {
      Case(RegWriteInstr, rw, x) {
        Case(LoadConst<int32_t>, lc, x) { add[lc->d1].add_eq(lc->value); }
        else Case(BinaryOpInstr, bop, x) {
          if (add.count(bop->s1) && add.count(bop->s2))
            switch (bop->op.type) {
            case BinaryCompute::ADD:
              add[bop->d1] = add[bop->s1];
              add[bop->d1].add_eq(add[bop->s2], 1);
              break;
            case BinaryCompute::SUB:
              add[bop->d1] = add[bop->s1];
              add[bop->d1].add_eq(add[bop->s2], -1);
              break;
            default:
              break;
            }
        }
        if (!add.count(rw->d1)) {
          add[rw->d1].add_eq(rw->d1, 1);
        }
      }
      Case(LoadAddr, la, x) {
        AddExpr zero;
        w.out[{la->d1, zero}] = {la->d1, 0};
        mp[la->d1] = {la->d1, 0};
      }
      else Case(ArrayIndex, ai, x) {
        if (ai->size == 4) {
          AddExpr a0 = add.at(ai->s2);
          if (a0.bad)
            return;
          int32_t c = a0.c;
          a0.c = 0;
          bool flag = 0;
          if (w.out.count({ai->s1, a0})) {
            auto [r, c0] = w.out[{ai->s1, a0}];
            if (std::abs(c - c0) < 256) {
              mp[ai->d1] = {r, c - c0};
              flag = 1;
            }
          }
          if (!flag) {
            w.out[{ai->s1, a0}] = {ai->d1, c};
            mp[ai->d1] = {ai->d1, 0};
          }
        }
      }
      else Case(LoadInstr, ld, x) {
        check(ld->addr, ld->offset);
      }
      else Case(StoreInstr, st, x) {
        check(st->addr, st->offset);
      }
    });
  }
};

void remove_unused_def_func(NormalFunc *f);
void load_store_offset(NormalFunc *f) {
  DAG_IR dag(f);
  LoadStoreOffset w;
  dag.visit(w);
  if (w.cnt)
    remove_unused_def_func(f);
}
