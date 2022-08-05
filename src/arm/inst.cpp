#include "arm/inst.hpp"

#include "arm/program.hpp"
#include "common/common.hpp"
#include "ir/scalar.hpp"

using std::list;
using std::make_unique;
using std::ostream;
using std::string;
using std::unique_ptr;
#if 0
#undef assert
#define assert(x)                                                              \
  if (!(x))                                                                    \
    fprintf(stderr, "assert failed: %s %d %s\n", __FILE__, (int)__LINE__, #x);
#endif

namespace ARMv7 {

InstCond logical_not(InstCond c) {
  switch (c) {
  case Eq:
    return Ne;
  case Ne:
    return Eq;
  case Ge:
    return Lt;
  case Gt:
    return Le;
  case Le:
    return Gt;
  case Lt:
    return Ge;
  default:
    assert(0);
    return Lt;
  }
}

InstCond reverse_operand(InstCond c) {
  switch (c) {
  case Ge:
    return Le;
  case Gt:
    return Lt;
  case Le:
    return Ge;
  case Lt:
    return Gt;
  default:
    return c;
  }
}

ostream &operator<<(ostream &os, const Reg &reg) {
  if (reg.type == ScalarType::Float)
    os << 's' << reg.id;
  else if (reg.id == sp)
    os << "sp";
  else if (reg.id == pc)
    os << "pc";
  else if (reg.id == lr)
    os << "lr";
  else
    os << 'r' << reg.id;
  return os;
}

ostream &operator<<(ostream &os, const InstCond &cond) {
  switch (cond) {
  case Always:
    break;
  case Eq:
    os << "eq";
    break;
  case Ne:
    os << "ne";
    break;
  case Ge:
    os << "ge";
    break;
  case Gt:
    os << "gt";
    break;
  case Le:
    os << "le";
    break;
  case Lt:
    os << "lt";
    break;
  default:
    unreachable();
  }
  return os;
}

InstCond from_ir_binary_op(IR::BinaryCompute op) {
  switch (op) {
  case IR::BinaryCompute::LEQ:
  case IR::BinaryCompute::FLEQ:
    return Le;
  case IR::BinaryCompute::LESS:
  case IR::BinaryCompute::FLESS:
    return Lt;
  case IR::BinaryCompute::EQ:
  case IR::BinaryCompute::FEQ:
    return Eq;
  case IR::BinaryCompute::NEQ:
  case IR::BinaryCompute::FNEQ:
    return Ne;
  default:
    unreachable();
    return Eq;
  }
}

ostream &operator<<(ostream &os, const Shift &shift) {
  if (shift.w != 0) {
    os << ',';
    switch (shift.type) {
    case Shift::LSL:
      os << "LSL";
      break;
    case Shift::LSR:
      os << "LSR";
      break;
    case Shift::ASR:
      os << "ASR";
      break;
    case Shift::ROR:
      os << "ROR";
      break;
    }
    os << " #" << shift.w;
  }
  return os;
}

unique_ptr<Inst> set_cond(unique_ptr<Inst> inst, InstCond cond) {
  inst->cond = cond;
  return inst;
}

list<unique_ptr<Inst>> set_cond(list<unique_ptr<Inst>> inst, InstCond cond) {
  for (auto &i : inst)
    i->cond = cond;
  return inst;
}

void insert(list<unique_ptr<Inst>> &inserted_list,
            list<unique_ptr<Inst>>::iterator pos, list<unique_ptr<Inst>> inst) {
  for (auto &i : inst)
    inserted_list.insert(pos, std::move(i));
}

void replace(list<unique_ptr<Inst>> &inserted_list,
             list<unique_ptr<Inst>>::iterator pos,
             list<unique_ptr<Inst>> inst) {
  for (auto i = inst.begin(); i != inst.end(); ++i) {
    if (std::next(i) != inst.end())
      inserted_list.insert(pos, std::move(*i));
    else
      *pos = std::move(*i);
  }
}

void RegRegInst::gen_asm(ostream &out, AsmContext *) {
  switch (op) {
  case Add:
    out << "add";
    break;
  case Sub:
    out << "sub";
    break;
  case Mul:
    out << "mul";
    break;
  case Div:
    out << "sdiv";
    break;
  case RevSub:
    out << "rsb";
    break;
  case And:
    out << "and";
    break;
  default:
    unreachable();
  }
  assert(dst.type == ScalarType::Int);
  assert(lhs.type == ScalarType::Int);
  if (lhs.type == ScalarType::Float)
    out << "???\n";
  assert(rhs.type == ScalarType::Int);
  out << cond << ' ' << dst << ',' << lhs << ',' << rhs << shift << '\n';
}

void FRegRegInst::gen_asm(ostream &out, AsmContext *) {
  switch (op) {
  case Add:
    out << "vadd.f32";
    break;
  case Sub:
    out << "vsub.f32";
    break;
  case Mul:
    out << "vmul.f32";
    break;
  case Div:
    out << "vdiv.f32";
    break;
  default:
    unreachable();
  }
  assert(dst.type == ScalarType::Float);
  assert(lhs.type == ScalarType::Float);
  assert(rhs.type == ScalarType::Float);
  out << cond << ' ' << dst << ',' << lhs << ',' << rhs << '\n';
}

void ML::gen_asm(ostream &out, AsmContext *) {
  if (op == Mla)
    out << "mla";
  else if (op == Mls)
    out << "mls";
  else
    unreachable();
  assert(dst.type == ScalarType::Int);
  assert(s1.type == ScalarType::Int);
  assert(s2.type == ScalarType::Int);
  assert(s3.type == ScalarType::Int);
  out << cond << ' ' << dst << ',' << s1 << ',' << s2 << ',' << s3 << '\n';
}

void SMulL::gen_asm(ostream &out, AsmContext *) {
  assert(d1.type == ScalarType::Int);
  assert(d2.type == ScalarType::Int);
  assert(s1.type == ScalarType::Int);
  assert(s2.type == ScalarType::Int);
  out << "smull" << cond << ' ' << d1 << ',' << d2 << ',' << s1 << ',' << s2
      << '\n';
}

void RegImmInst::gen_asm(ostream &out, AsmContext *) {
  switch (op) {
  case Add:
    out << "add";
    break;
  case Sub:
    out << "sub";
    break;
  case RevSub:
    out << "rsb";
    break;
  case Lsl:
    out << "lsl";
    break;
  case Lsr:
    out << "lsr";
    break;
  case Asr:
    out << "asr";
    break;
  case Bic:
    out << "bic";
    break;
  default:
    unreachable();
  }
  assert(dst.type == ScalarType::Int);
  assert(lhs.type == ScalarType::Int);
  out << cond << ' ' << dst << ',' << lhs << ",#" << rhs << '\n';
}

void MoveReg::gen_asm(ostream &out, AsmContext *) {
  if (dst.type == ScalarType::Float) {
    if (src.type == ScalarType::Float) {
      if (src != dst) {
        out << "vmov.f32" << cond << ' ' << dst << ',' << src << '\n';
      }
    } else {
      out << "vmov" << cond << ' ' << dst << ',' << src << '\n';
    }
  } else {
    if (src.type == ScalarType::Float) {
      out << "vmov" << cond << ' ' << dst << ',' << src << '\n';
    } else {
      if (src != dst) {
        out << "mov" << cond << ' ' << dst << ',' << src << '\n';
      }
    }
  }
}

void FRegInst::gen_asm(ostream &out, AsmContext *) {
  assert(dst.type == ScalarType::Float);
  assert(src.type == ScalarType::Float);
  switch (op) {
  case Neg:
    out << "vneg.f32";
    break;
  case F2I:
    out << "vcvt.s32.f32";
    break;
  case I2F:
    out << "vcvt.f32.s32";
    break;
  case F2D0:
    out << "vcvt.f64.f32 d8," << src << '\n';
    out << "vmov.f32 " << dst << ",s16\n";
    return;
  case F2D1:
    out << "vcvt.f64.f32 d8," << src << '\n';
    out << "vmov.f32 " << dst << ",s17\n";
    return;
  default:
    assert(0);
  }
  out << ' ' << dst << ',' << src << '\n';
}

void ShiftInst::gen_asm(ostream &out, AsmContext *) {
  assert(dst.type == ScalarType::Int);
  assert(src.type == ScalarType::Int);
  out << "mov" << cond << ' ' << dst << ',' << src << shift << '\n';
}

void MoveImm::gen_asm(ostream &out, AsmContext *) {
  switch (op) {
  case Mov:
    out << "mov";
    break;
  case Mvn:
    out << "mvn";
    break;
  default:
    unreachable();
  }
  assert(dst.type == ScalarType::Int);
  out << cond << ' ' << dst << ",#" << src << '\n';
}

void MoveW::gen_asm(ostream &out, AsmContext *) {
  assert(dst.type == ScalarType::Int);
  out << "movw" << cond << ' ' << dst << ",#" << src << '\n';
}

void MoveT::gen_asm(ostream &out, AsmContext *) {
  assert(dst.type == ScalarType::Int);
  out << "movt" << cond << ' ' << dst << ",#" << src << '\n';
}

void LoadSymbolAddrLower16::gen_asm(ostream &out, AsmContext *) {
  assert(dst.type == ScalarType::Int);
  out << "movw" << cond << ' ' << dst << ",#:lower16:" << symbol << '\n';
}

void LoadSymbolAddrUpper16::gen_asm(ostream &out, AsmContext *) {
  assert(dst.type == ScalarType::Int);
  out << "movt" << cond << ' ' << dst << ",#:upper16:" << symbol << '\n';
}

std::list<unique_ptr<Inst>> load_imm(Reg dst, int32_t imm) {
  std::list<unique_ptr<Inst>> ret;
  if (is_legal_immediate(imm))
    ret.push_back(make_unique<MoveImm>(MoveImm::Mov, dst, imm));
  else if (is_legal_immediate(~imm))
    ret.push_back(make_unique<MoveImm>(MoveImm::Mvn, dst, ~imm));
  else {
    uint32_t u = static_cast<uint32_t>(imm);
    uint32_t top = u >> 16, bot = u & 0xffffu;
    ret.push_back(make_unique<MoveW>(dst, static_cast<int32_t>(bot)));
    if (top)
      ret.push_back(make_unique<MoveT>(dst, static_cast<int32_t>(top)));
  }
  return ret;
}

void load_imm_asm(ostream &out, Reg dst, int32_t imm, InstCond cond) {
  if (is_legal_immediate(imm))
    out << "mov" << cond << ' ' << dst << ",#" << imm << '\n';
  else if (is_legal_immediate(~imm))
    out << "mvn" << cond << ' ' << dst << ",#" << ~imm << '\n';
  else {
    uint32_t u = static_cast<uint32_t>(imm);
    uint32_t top = u >> 16, bot = u & 0xffffu;
    out << "movw" << cond << ' ' << dst << ",#" << bot << '\n';
    if (top)
      out << "movt" << cond << ' ' << dst << ",#" << top << '\n';
  }
}

std::list<unique_ptr<Inst>> reg_imm_sum(Reg dst, Reg lhs, int32_t rhs) {
  std::list<unique_ptr<Inst>> ret;
  if (is_legal_immediate(rhs))
    ret.push_back(make_unique<RegImmInst>(RegImmInst::Add, dst, lhs, rhs));
  else if (is_legal_immediate(-rhs))
    ret.push_back(make_unique<RegImmInst>(RegImmInst::Sub, dst, lhs, -rhs));
  else {
    // TODO: can be done better if rhs is sum of 2 legal immediates
    for (auto &i : load_imm(dst, rhs))
      ret.push_back(std::move(i));
    ret.push_back(make_unique<RegRegInst>(RegRegInst::Add, dst, lhs, dst));
  }
  return ret;
}

std::list<unique_ptr<Inst>> load_symbol_addr(Reg dst, const string &symbol) {
  std::list<unique_ptr<Inst>> ret;
  ret.push_back(make_unique<LoadSymbolAddrLower16>(dst, symbol));
  ret.push_back(make_unique<LoadSymbolAddrUpper16>(dst, symbol));
  return ret;
}

bool load_store_offset_range(int32_t offset) {
  return offset >= -1023 && offset <= 1023;
}

bool load_store_offset_range(int64_t offset) {
  return offset >= -1023 && offset <= 1023;
}

void Load::gen_asm(ostream &out, AsmContext *) {
  assert(load_store_offset_range(offset_imm));
  if (dst.type == ScalarType::Float) {
    out << "v";
  }
  out << "ldr" << cond << ' ' << dst << ",[" << base << ",#" << offset_imm
      << "]\n";
}

void Store::gen_asm(ostream &out, AsmContext *) {
  assert(load_store_offset_range(offset_imm));
  if (src.type == ScalarType::Float) {
    out << "v";
  }
  out << "str" << cond << ' ' << src << ",[" << base << ",#" << offset_imm
      << "]\n";
}

void ComplexLoad::gen_asm(ostream &out, AsmContext *) {
  if (dst.type == ScalarType::Float) {
    out << "ldr" << cond << ' ' << Reg(dst.id, ScalarType::Float) << ",["
        << base << ',' << offset << shift << "]\n";
    out << "vmov" << cond << ' ' << dst << ',' << Reg(dst.id, ScalarType::Float)
        << "\n";
  } else {
    out << "ldr" << cond << ' ' << dst << ",[" << base << ',' << offset << shift
        << "]\n";
  }
}

void ComplexStore::gen_asm(ostream &out, AsmContext *) {
  if (src.type == ScalarType::Float) {
    out << "vmov" << cond << ' ' << Reg(src.id, ScalarType::Float) << ',' << src
        << "\n";
    out << "str" << cond << ' ' << Reg(src.id, ScalarType::Float) << ",["
        << base << ',' << offset << shift << "]\n";
  } else {
    out << "str" << cond << ' ' << src << ",[" << base << ',' << offset << shift
        << "]\n";
  }
}

void LoadStack::gen_asm(ostream &out, AsmContext *ctx) {
  int32_t total_offset = src->position + offset - ctx->temp_sp_offset;
  assert(load_store_offset_range(total_offset));
  if (dst.type == ScalarType::Float)
    out << 'v';
  out << "ldr" << cond << ' ' << dst << ",[sp,#" << total_offset << "]\n";
}

void StoreStack::gen_asm(ostream &out, AsmContext *ctx) {
  int32_t total_offset = target->position + offset - ctx->temp_sp_offset;
  assert(load_store_offset_range(total_offset));
  if (src.type == ScalarType::Float)
    out << 'v';
  out << "str" << cond << ' ' << src << ",[sp,#" << total_offset << "]\n";
}

void Push::gen_asm(ostream &out, AsmContext *ctx) {
  for (Reg r : src) {
    if (r.type == ScalarType::Float) {
      out << 'v';
    }
    out << "push" << cond << " {" << r << "}\n";
  }
  ctx->temp_sp_offset -= static_cast<int32_t>(INT_SIZE * src.size());
}

void ChangeSP::gen_asm(ostream &out, AsmContext *ctx) {
  if (is_legal_immediate(change)) {
    out << "add" << cond << " sp,sp,#" << change << '\n';
  } else if (is_legal_immediate(-change)) {
    out << "sub" << cond << "sp,sp,#" << -change << '\n';
  }
  ctx->temp_sp_offset += change;
}

std::list<unique_ptr<Inst>> sp_move(int32_t change) {
  if (change == 0)
    return {};
  std::list<unique_ptr<Inst>> ret;
  if (is_legal_immediate(change) || is_legal_immediate(-change))
    ret.push_back(make_unique<ChangeSP>(change));
  else if (change > 0) {
    for (int i = 0; i < 4; ++i) {
      int32_t cur = change & (0xff << (i * 8));
      if (cur)
        ret.push_back(make_unique<ChangeSP>(cur));
    }
  } else {
    for (int i = 0; i < 4; ++i) {
      int32_t cur = (-change) & (0xff << (i * 8));
      if (cur)
        ret.push_back(make_unique<ChangeSP>(-cur));
    }
  }
  return ret;
}

void sp_move_asm(int32_t change, ostream &out) {
  if (change == 0)
    return;
  if (is_legal_immediate(change)) {
    out << "add sp,sp,#" << change << '\n';
    return;
  }
  if (is_legal_immediate(-change)) {
    out << "sub sp,sp,#" << -change << '\n';
    return;
  }
  if (change > 0) {
    for (int i = 0; i < 4; ++i) {
      int32_t cur = change & (0xff << (i * 8));
      if (cur)
        out << "add sp,sp,#" << cur << '\n';
    }
  } else {
    for (int i = 0; i < 4; ++i) {
      int32_t cur = (-change) & (0xff << (i * 8));
      if (cur)
        out << "sub sp,sp,#" << cur << '\n';
    }
  }
}

void LoadStackAddr::gen_asm(ostream &, AsmContext *) { unreachable(); }

void LoadStackOffset::gen_asm(ostream &, AsmContext *) { unreachable(); }

/*
void LoadGlobalAddr::gen_asm(ostream &out, AsmContext *ctx) {
    out << "ldr" << cond << ' ' << dst << ",=" << name << '\n';
}
*/

void RegRegCmp::gen_asm(ostream &out, AsmContext *) {
  switch (op) {
  case Cmp:
    out << "cmp";
    break;
  case Cmn:
    out << "cmn";
    break;
  default:
    unreachable();
  }
  assert(lhs.type == ScalarType::Int);
  assert(rhs.type == ScalarType::Int);
  out << cond << ' ' << lhs << ',' << rhs << '\n';
}

void FRegRegCmp::gen_asm(ostream &out, AsmContext *) {
  assert(lhs.type == ScalarType::Float);
  assert(rhs.type == ScalarType::Float);
  out << "vcmp.f32" << cond << ' ' << lhs << ',' << rhs << '\n';
  out << "vmrs APSR_nzcv,fpscr" << '\n';
}

void RegImmCmp::gen_asm(ostream &out, AsmContext *) {
  switch (op) {
  case Cmp:
    out << "cmp";
    break;
  case Cmn:
    out << "cmn";
    break;
  default:
    unreachable();
  }
  assert(lhs.type == ScalarType::Float);
  out << cond << ' ' << lhs << ",#" << rhs << '\n';
}

void Branch::gen_asm(ostream &out, AsmContext *) {
  out << 'b' << cond << ' ' << target->name << '\n';
}

void FuncCall::gen_asm(ostream &out, AsmContext *) {
  out << "bl" << cond << ' ';
  if (name == "starttime")
    out << "_sysy_starttime";
  else if (name == "stoptime")
    out << "_sysy_stoptime";
  else
    out << name;
  out << '\n';
}

void Return::gen_asm(ostream &out, AsmContext *ctx) {
  if (!ctx->epilogue(out))
    out << "bx lr\n";
}

Branch::Branch(Block *_target) : target(_target) { target->label_used = true; }

} // namespace ARMv7
