#include "arm/opt/foreplay.hpp"

namespace ARMv7 {

struct MIContext {
  typedef std::list<std::unique_ptr<Inst>> List;
  List *_ls;
  List::iterator _it;
  void visit(List &ls, List::iterator it) {
    _it = it;
    _ls = &ls;
  }
  template <class... T> void RegReg(T... args) {
    _ls->insert(_it, std::make_unique<RegRegInst>(args...));
  }
  template <class... T> void RegImm(T... args) {
    _ls->insert(_it, std::make_unique<RegImmInst>(args...));
  }
  void Ins(Inst *x) { _ls->insert(_it, std::unique_ptr<Inst>(x)); }
  void Ins(List &&ls) {
    for (auto &x : ls) {
      _ls->insert(_it, std::move(x));
    }
  }
  void Del() {
    assert(_it != _ls->begin());
    auto p = std::prev(_it);
    *_it = std::move(*p);
    _ls->erase(p);
  }
};

std::pair<int64_t, int> div_opt(int32_t A0) {
  int ex = __builtin_ctz(A0);
  int32_t A = A0 >> ex;
  int64_t L = 1ll << 32;
  int log2L = 32;
  while (L / (A - L % A) < (1ll << 31))
    L <<= 1, ++log2L;
  int64_t B = L / A + 1;
  int s = ex + log2L - 32;
  assert(0 <= B && B < (1ll << 32));
  // std::cerr << ">>> div_any: " << A0 << ' ' << B << ' ' << s << std::endl;
  return {B, s};
}

void merge_instr(Func *f) {
  auto ctx = MIContext();
  for (auto &block : f->blocks) {
    auto &insts = block->insts;
    for (auto it = insts.begin(); it != insts.end(); ++it) {
      ctx.visit(insts, it);
      Inst *inst = it->get();
      if (auto cmp = dynamic_cast<RegRegCmp *>(inst)) {
        if (!f->constant_reg.count(cmp->rhs))
          continue;
        int32_t v = f->constant_reg[cmp->rhs];
        if (is_legal_immediate(v)) {
          ctx.Ins(new RegImmCmp(RegImmCmp::Cmp, cmp->lhs, v));
          ctx.Del();
        }
      } else if (auto bop = dynamic_cast<RegRegInst *>(inst)) {
        if (bop->shift.w)
          continue;
        if (!f->constant_reg.count(bop->rhs))
          continue;
        int32_t v = f->constant_reg[bop->rhs];
        switch (bop->op) {
        case RegRegInst::Add:
        case RegRegInst::Sub: {
          auto op =
              bop->op == RegRegInst::Add ? RegImmInst::Add : RegImmInst::Sub;
          if (is_legal_immediate(v)) {
            ctx.RegImm(op, bop->dst, bop->lhs, v);
            ctx.Del();
          }
          break;
        }
        case RegRegInst::Mul: {
          if (v <= 1)
            break;
          int32_t log2v = __builtin_ctz(v);
          int32_t v0 = 1 << log2v;
          if (v == v0) {
            ctx.RegImm(RegImmInst::Lsl, bop->dst, bop->lhs, log2v);
            ctx.Del();
          } else if (__builtin_popcount(v - v0) == 1) {
            int32_t s = __builtin_ctz(v - v0);
            ctx.RegReg(RegRegInst::Add, bop->dst, bop->lhs, bop->lhs,
                       Shift(Shift::LSL, s - log2v));
            if (log2v)
              ctx.RegImm(RegImmInst::Lsl, bop->dst, bop->dst, log2v);
            ctx.Del();
          } else if (__builtin_popcount(v + v0) == 1) {
            int32_t s = __builtin_ctz(v + v0);
            ctx.RegReg(RegRegInst::RevSub, bop->dst, bop->lhs, bop->lhs,
                       Shift(Shift::LSL, s - log2v));
            if (log2v)
              ctx.RegImm(RegImmInst::Lsl, bop->dst, bop->dst, log2v);
            ctx.Del();
          }
          break;
        }
        case RegRegInst::Div:
          if (v > 1 && v == (v & -v)) {
            int32_t log2v = __builtin_ctz(v);
            assert(v == (1 << log2v));
            auto r0 = bop->lhs;
            auto r1 = bop->dst;
            auto r2 = bop->dst;
            if (v == 2) {
              ctx.RegReg(RegRegInst::Add, r2, r0, r0, Shift(Shift::LSR, 31));
            } else {
              ctx.RegImm(RegImmInst::Asr, r1, r0, 31);
              ctx.RegReg(RegRegInst::Add, r2, r0, r1,
                         Shift(Shift::LSR, 32 - log2v));
            }
            ctx.RegImm(RegImmInst::Asr, bop->dst, r2, log2v);
            ctx.Del();
          } else if (v > 1) {
            auto [B, s] = div_opt(v);
            Reg lo = Reg(r4, ScalarType::Int);
            Reg hi = Reg(r5, ScalarType::Int);
            int32_t B0 = B & 0x7fffffff;
            Reg x = bop->lhs;
            ctx.Ins(load_imm(lo, B0));
            ctx.Ins(new SMulL(lo, hi, x, lo));
            if (B & (1ll << 31)) {
              ctx.RegReg(RegRegInst::Add, hi, hi, x, Shift(Shift::ASR, 1));
              ctx.RegReg(RegRegInst::And, lo, x, lo, Shift(Shift::LSR, 31));
              ctx.RegReg(RegRegInst::Add, hi, hi, lo);
            }
            ctx.RegImm(RegImmInst::Asr, bop->dst, hi, s);
            ctx.RegReg(RegRegInst::Add, bop->dst, bop->dst, hi,
                       Shift(Shift::LSR, 31));
            ctx.Del();
          }
          break;
        case RegRegInst::Mod:
          if (v > 1 && v == (v & -v)) {
            int32_t log2v = __builtin_ctz(v);
            assert(v == (1 << log2v));
            auto r0 = bop->lhs;
            auto r1 = bop->dst;
            auto r2 = bop->dst;
            auto r3 = bop->dst;
            if (v == 2) {
              ctx.RegReg(RegRegInst::Add, r2, r0, r0, Shift(Shift::LSR, 31));
            } else {
              ctx.RegImm(RegImmInst::Asr, r1, r0, 31);
              ctx.RegReg(RegRegInst::Add, r2, r0, r1,
                         Shift(Shift::LSR, 32 - log2v));
            }
            if (is_legal_immediate(v - 1)) {
              ctx.RegImm(RegImmInst::Bic, r3, r2, v - 1);
              ctx.RegReg(RegRegInst::Sub, bop->dst, r0, r3);
            } else {
              ctx.RegImm(RegImmInst::Lsr, r3, r2, log2v);
              ctx.RegReg(RegRegInst::Sub, bop->dst, r0, r3,
                         Shift(Shift::LSL, log2v));
            }
            ctx.Del();
          } else if (v > 1) {
            auto [B, s] = div_opt(v);
            Reg lo = Reg(r4, ScalarType::Int);
            Reg hi = Reg(r5, ScalarType::Int);
            int32_t B0 = B & 0x7fffffff;
            Reg x = bop->lhs;
            ctx.Ins(load_imm(lo, B0));
            ctx.Ins(new SMulL(lo, hi, x, lo));
            if (B & (1ll << 31)) {
              ctx.RegReg(RegRegInst::Add, hi, hi, x, Shift(Shift::ASR, 1));
              ctx.RegReg(RegRegInst::And, lo, x, lo, Shift(Shift::LSR, 31));
              ctx.RegReg(RegRegInst::Add, hi, hi, lo);
            }
            ctx.RegImm(RegImmInst::Asr, bop->dst, hi, s);
            ctx.RegReg(RegRegInst::Add, bop->dst, bop->dst, hi,
                       Shift(Shift::LSR, 31));
            ctx.Ins(load_imm(lo, v));
            ctx.Ins(new ML(ML::Mls, bop->dst, bop->dst, lo, x));
            ctx.Del();
          }
          break;
        default:
          break;
        }
      }
    }
  }
}

void merge_instr(Program *prog) {
  for (auto &f : prog->funcs) {
    merge_instr(f.get());
  }
}
} // namespace ARMv7
