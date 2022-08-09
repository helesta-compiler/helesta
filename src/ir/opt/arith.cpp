#include "ir/opt/dag_ir.hpp"

struct Mod2Div : ForwardLoopVisitor<std::map<std::pair<Reg, Reg>, Reg>>,
                 CounterOutput {
  NormalFunc *f;
  Mod2Div(NormalFunc *_f) : CounterOutput("Mod2Div"), f(_f) {}
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
