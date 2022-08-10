#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "arm/archinfo.hpp"
#include "arm/block.hpp"
#include "arm/func.hpp"
#include "arm/program.hpp"

namespace ARMv7 {
Block::Block(std::string _name) : name(_name), label_used(false) {}

void Block::construct(IR::BB *ir_bb, Func *func, MappingInfo *info,
                      Block *next_block, std::map<Reg, CmpInfo> &cmp_info) {
  for (auto &i : ir_bb->instrs) {
    IR::Instr *cur = i.get();
    // std::cerr << *cur << std::endl;
    if (auto loadaddr = dynamic_cast<IR::LoadAddr *>(cur)) {
      Reg dst = info->from_ir_reg(loadaddr->d1);
      if (loadaddr->offset->global) {
        push_back(load_symbol_addr(
            dst, mangle_global_var_name(loadaddr->offset->name)));
        func->symbol_reg[dst] = mangle_global_var_name(loadaddr->offset->name);
      } else {
        push_back(std::make_unique<LoadStackAddr>(
            dst, 0, info->obj_mapping[loadaddr->offset]));
        func->stack_addr_reg[dst] = std::pair<StackObject *, int32_t>{
            info->obj_mapping[loadaddr->offset], 0};
      }
    } else if (auto loadconst = dynamic_cast<IR::LoadConst<int32_t> *>(cur)) {
      Reg dst = info->from_ir_reg(loadconst->d1);
      func->constant_reg[dst] = loadconst->value;
      push_back(load_imm(dst, loadconst->value));
    } else if (auto loadconst = dynamic_cast<IR::LoadConst<float> *>(cur)) {
      int as_int;
      memcpy(&as_int, &loadconst->value, sizeof(float));
      auto tmp = info->new_reg();
      Reg dst = info->from_ir_reg(loadconst->d1);
      func->constant_reg[tmp] = as_int;
      push_back(load_imm(tmp, as_int));
      push_back(std::make_unique<MoveReg>(dst, tmp));
    } else if (auto loadarg =
                   dynamic_cast<IR::LoadArg<ScalarType::Int> *>(cur)) {
      push_back(std::make_unique<MoveReg>(info->from_ir_reg(loadarg->d1),
                                          func->args[loadarg->id]));
    } else if (auto loadarg =
                   dynamic_cast<IR::LoadArg<ScalarType::Float> *>(cur)) {
      auto arg_reg = info->from_ir_reg(loadarg->d1);
      info->set_float(arg_reg);
      push_back(std::make_unique<MoveReg>(arg_reg, func->args[loadarg->id]));
    } else if (auto unary = dynamic_cast<IR::UnaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(unary->d1),
          src = info->from_ir_reg(unary->s1);
      switch (unary->op.type) {
      case IR::UnaryCompute::LNOT:
        push_back(std::make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(std::make_unique<RegImmCmp>(RegImmCmp::Cmp, src, 0));
        push_back(
            set_cond(std::make_unique<MoveImm>(MoveImm::Mov, dst, 1), Eq));
        // TODO: this can be done better with "rsbs dst, src, #0; adc dst,
        // src, dst" or "clz dst, src; lsr dst, dst, #5"
        break;
      case IR::UnaryCompute::NEG:
        push_back(
            std::make_unique<RegImmInst>(RegImmInst::RevSub, dst, src, 0));
        break;
      case IR::UnaryCompute::FNEG:
        info->set_float(dst);
        info->set_float(src);
        push_back(std::make_unique<FRegInst>(FRegInst::Neg, dst, src));
        break;
      case IR::UnaryCompute::ID:
        info->set_maybe_float_assign(dst, src);
        push_back(std::make_unique<MoveReg>(dst, src));
        break;
      case IR::UnaryCompute::I2F: {
        Reg tmp = info->new_reg();
        info->set_float(dst);
        info->set_float(tmp);
        push_back(std::make_unique<MoveReg>(tmp, src));
        push_back(std::make_unique<FRegInst>(FRegInst::I2F, dst, tmp));
        break;
      }
      case IR::UnaryCompute::F2I: {
        Reg tmp = info->new_reg();
        info->set_float(src);
        info->set_float(tmp);
        push_back(std::make_unique<FRegInst>(FRegInst::F2I, tmp, src));
        push_back(std::make_unique<MoveReg>(dst, tmp));
        break;
      }
      case IR::UnaryCompute::F2D0:
        info->set_float(dst);
        info->set_float(src);
        push_back(std::make_unique<FRegInst>(FRegInst::F2D0, dst, src));
        break;
      case IR::UnaryCompute::F2D1:
        info->set_float(dst);
        info->set_float(src);
        push_back(std::make_unique<FRegInst>(FRegInst::F2D1, dst, src));
        break;
      default:
        unreachable();
      }
    } else if (auto binary = dynamic_cast<IR::BinaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(binary->d1),
          s1 = info->from_ir_reg(binary->s1),
          s2 = info->from_ir_reg(binary->s2);
      if (binary->op.type == IR::BinaryCompute::ADD ||
          binary->op.type == IR::BinaryCompute::SUB ||
          binary->op.type == IR::BinaryCompute::MUL ||
          binary->op.type == IR::BinaryCompute::DIV) {
        push_back(std::make_unique<RegRegInst>(
            RegRegInst::from_ir_binary_op(binary->op.type), dst, s1, s2));
      } else if (binary->op.type == IR::BinaryCompute::FADD ||
                 binary->op.type == IR::BinaryCompute::FSUB ||
                 binary->op.type == IR::BinaryCompute::FMUL ||
                 binary->op.type == IR::BinaryCompute::FDIV) {
        info->set_float(dst);
        info->set_float(s1);
        info->set_float(s2);
        push_back(std::make_unique<FRegRegInst>(
            FRegRegInst::from_ir_binary_op(binary->op.type), dst, s1, s2));
      } else if (binary->op.type == IR::BinaryCompute::LESS ||
                 binary->op.type == IR::BinaryCompute::LEQ ||
                 binary->op.type == IR::BinaryCompute::EQ ||
                 binary->op.type == IR::BinaryCompute::NEQ) {
        push_back(std::make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(std::make_unique<RegRegCmp>(RegRegCmp::Cmp, s1, s2));
        push_back(set_cond(std::make_unique<MoveImm>(MoveImm::Mov, dst, 1),
                           from_ir_binary_op(binary->op.type)));
        cmp_info[dst].cond = from_ir_binary_op(binary->op.type);
        cmp_info[dst].lhs = s1;
        cmp_info[dst].rhs = s2;
        cmp_info[dst].is_float = 0;
      } else if (binary->op.type == IR::BinaryCompute::FLESS ||
                 binary->op.type == IR::BinaryCompute::FLEQ ||
                 binary->op.type == IR::BinaryCompute::FEQ ||
                 binary->op.type == IR::BinaryCompute::FNEQ) {
        info->set_float(s1);
        info->set_float(s2);
        push_back(std::make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(std::make_unique<FRegRegCmp>(s1, s2));
        push_back(set_cond(std::make_unique<MoveImm>(MoveImm::Mov, dst, 1),
                           from_ir_binary_op(binary->op.type)));
        cmp_info[dst].cond = from_ir_binary_op(binary->op.type);
        cmp_info[dst].lhs = s1;
        cmp_info[dst].rhs = s2;
        cmp_info[dst].is_float = 1;
      } else if (binary->op.type == IR::BinaryCompute::MOD) {
        Reg k = info->new_reg();
        push_back(std::make_unique<RegRegInst>(RegRegInst::Div, k, s1, s2));
        push_back(std::make_unique<ML>(ML::Mls, dst, s2, k, s1));
      } else
        unreachable();
    } else if (auto load = dynamic_cast<IR::LoadInstr *>(cur)) {
      Reg dst = info->from_ir_reg(load->d1),
          addr = info->from_ir_reg(load->addr);
      push_back(std::make_unique<Load>(dst, addr, load->offset));
    } else if (auto store = dynamic_cast<IR::StoreInstr *>(cur)) {
      Reg addr = info->from_ir_reg(store->addr),
          src = info->from_ir_reg(store->s1);
      push_back(std::make_unique<Store>(src, addr, store->offset));
    } else if (auto jump = dynamic_cast<IR::JumpInstr *>(cur)) {
      Block *jump_target = info->block_mapping[jump->target];
      if (jump_target != next_block)
        push_back(std::make_unique<Branch>(jump_target));
      out_edge.push_back(jump_target);
      jump_target->in_edge.push_back(this);
    } else if (auto branch = dynamic_cast<IR::BranchInstr *>(cur)) {
      Reg cond = info->from_ir_reg(branch->cond);
      Block *true_target = info->block_mapping[branch->target1],
            *false_target = info->block_mapping[branch->target0];
      if (cmp_info.find(cond) != cmp_info.end()) {
        if (cmp_info[cond].is_float) {
          push_back(std::make_unique<FRegRegCmp>(cmp_info[cond].lhs,
                                                 cmp_info[cond].rhs));
        } else {
          push_back(std::make_unique<RegRegCmp>(
              RegRegCmp::Cmp, cmp_info[cond].lhs, cmp_info[cond].rhs));
        }
        if (false_target == next_block)
          push_back(set_cond(std::make_unique<Branch>(true_target),
                             cmp_info[cond].cond));
        else if (true_target == next_block)
          push_back(set_cond(std::make_unique<Branch>(false_target),
                             logical_not(cmp_info[cond].cond)));
        else {
          push_back(set_cond(std::make_unique<Branch>(true_target),
                             cmp_info[cond].cond));
          push_back(std::make_unique<Branch>(false_target));
        }
      } else {
        push_back(std::make_unique<RegImmCmp>(RegImmCmp::Cmp, cond, 0));
        if (false_target == next_block)
          push_back(set_cond(std::make_unique<Branch>(true_target), Ne));
        else if (true_target == next_block)
          push_back(set_cond(std::make_unique<Branch>(false_target), Eq));
        else {
          push_back(set_cond(std::make_unique<Branch>(true_target), Ne));
          push_back(std::make_unique<Branch>(false_target));
        }
      }
      out_edge.push_back(true_target);
      out_edge.push_back(false_target);
      true_target->in_edge.push_back(this);
      false_target->in_edge.push_back(this);
    } else if (auto ret =
                   dynamic_cast<IR::ReturnInstr<ScalarType::Int> *>(cur)) {
      if (ret->ignore_return_value) {
        push_back(std::make_unique<Return>(ScalarType::Void));
      } else {
        push_back(std::make_unique<MoveReg>(
            Reg(RegConvention<ScalarType::Int>::ARGUMENT_REGISTERS[0],
                ScalarType::Int),
            info->from_ir_reg(ret->s1)));
        push_back(std::make_unique<Return>(ScalarType::Int));
      }
    } else if (auto ret =
                   dynamic_cast<IR::ReturnInstr<ScalarType::Float> *>(cur)) {
      if (ret->ignore_return_value) {
        push_back(std::make_unique<Return>(ScalarType::Void));
      } else {
        push_back(std::make_unique<MoveReg>(
            Reg(RegConvention<ScalarType::Float>::ARGUMENT_REGISTERS[0],
                ScalarType::Float),
            info->from_ir_reg(ret->s1)));
        push_back(std::make_unique<Return>(ScalarType::Float));
      }
    } else if (auto call = dynamic_cast<IR::CallInstr *>(cur)) {
      int int_arg_cnt = 0, float_arg_cnt = 0;
      for (auto kv : call->args) {
        if (kv.second == ScalarType::Int)
          int_arg_cnt += 1;
        else if (kv.second == ScalarType::Float)
          float_arg_cnt += 1;
        else
          assert(false);
      }
      int int_arg_size = int_arg_cnt, float_arg_size = float_arg_cnt;
      int stack_passed = 0;
      if (int_arg_size >
          RegConvention<ScalarType::Int>::ARGUMENT_REGISTER_COUNT)
        stack_passed += int_arg_size -
                        RegConvention<ScalarType::Int>::ARGUMENT_REGISTER_COUNT;
      if (float_arg_size >
          RegConvention<ScalarType::Float>::ARGUMENT_REGISTER_COUNT)
        stack_passed +=
            float_arg_size -
            RegConvention<ScalarType::Float>::ARGUMENT_REGISTER_COUNT;
      if (stack_passed % 2 == 1) {
        stack_passed += 1;
        push_back(sp_move(-static_cast<int>(INT_SIZE)));
      }
      for (auto it = call->args.rbegin(); it != call->args.rend(); it++) {
        auto kv = *it;
        if (kv.second == ScalarType::Int) {
          int_arg_cnt -= 1;
          if (int_arg_cnt >=
              RegConvention<ScalarType::Int>::ARGUMENT_REGISTER_COUNT) {
            push_back(std::make_unique<Push>(
                std::vector<Reg>{info->from_ir_reg(kv.first)}));
          } else {
            push_back(std::make_unique<MoveReg>(
                Reg(RegConvention<
                        ScalarType::Int>::ARGUMENT_REGISTERS[int_arg_cnt],
                    ScalarType::Int),
                info->from_ir_reg(kv.first)));
          }
        } else {
          float_arg_cnt -= 1;
          auto reg = info->from_ir_reg(kv.first);
          info->set_float(reg);
          if (float_arg_cnt >=
              RegConvention<ScalarType::Float>::ARGUMENT_REGISTER_COUNT) {
            push_back(std::make_unique<Push>(std::vector<Reg>{reg}));
          } else {
            push_back(std::make_unique<MoveReg>(
                Reg(RegConvention<
                        ScalarType::Float>::ARGUMENT_REGISTERS[float_arg_cnt],
                    ScalarType::Float),
                reg));
          }
        }
      }
      push_back(std::make_unique<FuncCall>(call->f->name, int_arg_size,
                                           float_arg_size));
      if (stack_passed > 0) {
        push_back(sp_move(stack_passed * INT_SIZE));
      }
      if (call->return_type == ScalarType::Float) {
        auto ret = info->from_ir_reg(call->d1);
        info->set_float(ret);
        push_back(std::make_unique<MoveReg>(
            ret, Reg(RegConvention<ScalarType::Float>::ARGUMENT_REGISTERS[0],
                     ScalarType::Float)));
      } else if (call->return_type == ScalarType::Int) {
        auto r0 = RegConvention<ScalarType::Int>::ARGUMENT_REGISTERS[0];
        if (call->f->name == "__create_threads") {
          assert(
              info->reg_mapping.insert({call->d1.id, Reg(r0, ScalarType::Int)})
                  .second);
        } else {
          push_back(std::make_unique<MoveReg>(info->from_ir_reg(call->d1),
                                              Reg(r0, ScalarType::Int)));
        }
      }
    } else if (auto array_index = dynamic_cast<IR::ArrayIndex *>(cur)) {
      Reg dst = info->from_ir_reg(array_index->d1),
          s1 = info->from_ir_reg(array_index->s1),
          s2 = info->from_ir_reg(array_index->s2);
      // TODO: optimize when size=2^k
      if (func->constant_reg.count(s2)) {
        int32_t v2 = func->constant_reg[s2] * array_index->size;
        if (is_legal_immediate(v2)) {
          push_back(std::make_unique<RegImmInst>(RegImmInst::Add, dst, s1, v2));
          continue;
        }
      }

      if (array_index->size == 4) {
        push_back(std::make_unique<RegRegInst>(RegRegInst::Add, dst, s1, s2,
                                               Shift(Shift::LSL, 2)));
      } else {
        Reg step = info->new_reg();
        push_back(load_imm(step, array_index->size));
        push_back(std::make_unique<ML>(ML::Mla, dst, s2, step, s1));
      }
    } else {
      unreachable();
    }
  }
}

void Block::push_back(std::unique_ptr<Inst> inst) {
  insts.push_back(std::move(inst));
}

void Block::push_back(std::list<std::unique_ptr<Inst>> inst_list) {
  for (auto &i : inst_list) {
    insts.push_back(std::move(i));
  }
}

void Block::insert_before_jump(std::unique_ptr<Inst> inst) {
  auto i = insts.end();
  while (i != insts.begin()) {
    auto prev_i = std::prev(i);
    if ((*prev_i)->as<Branch>()) {
      i = prev_i;
    } else {
      break;
    }
  }
  insts.insert(i, std::move(inst));
}

void Block::gen_asm(std::ostream &out, AsmContext *ctx) {
  ctx->temp_sp_offset = 0;
  if (label_used)
    out << name << ":\n";
  for (auto &i : insts)
    i->gen_asm(out, ctx);
}

void Block::print(std::ostream &out) {
  out << '\n' << name << ":\n";
  for (auto &i : insts)
    i->print(out);
}
} // namespace ARMv7
