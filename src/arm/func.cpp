#include <iostream>
#include <vector>

#include "arm/func.hpp"
#include "arm/simple_coloring_alloc.hpp"

namespace ARMv7 {

template <ScalarType type>
std::vector<int> reg_allocate(RegAllocStat *stat, Func *ctx) {
  info << "register allocation for function: " << ctx->name << '\n';
  info << "reg_n = " << ctx->reg_n << '\n';
  stat->spill_cnt = 0;
  info << "using SimpleColoringAllocator\n";
  while (true) {
    SimpleColoringAllocator<type> allocator(ctx);
    std::vector<int> ret = allocator.run(stat);
    if (stat->succeed)
      return ret;
  }
}

void Func::gen_asm(std::ostream &out) {
  RegAllocStat int_stat, float_stat;
  std::vector<int> int_reg_alloc, float_reg_alloc;
  AsmContext ctx;
  std::function<void(std::ostream & out)> prologue;
  while (true) {
    int_reg_alloc = reg_allocate<ScalarType::Int>(&int_stat, this);
    float_reg_alloc = reg_allocate<ScalarType::Float>(&float_stat, this);
    int32_t stack_size = 0;
    for (auto i = stack_objects.rbegin(); i != stack_objects.rend(); ++i) {
      (*i)->position = stack_size;
      stack_size += (*i)->size;
    }
    std::vector<Reg> save_int_regs, save_float_regs;
    bool used_int[RegConvention<ScalarType::Int>::Count] = {};
    bool used_float[RegConvention<ScalarType::Float>::Count] = {};
    for (int i : int_reg_alloc)
      if (i >= 0)
        used_int[i] = true;
    for (int i = 0; i < RegConvention<ScalarType::Int>::Count; ++i)
      if (RegConvention<ScalarType::Int>::REGISTER_USAGE[i] ==
              RegisterUsage::callee_save &&
          used_int[i])
        save_int_regs.emplace_back(Reg(i, ScalarType::Int));
    for (int i : float_reg_alloc)
      if (i >= 0)
        used_float[i] = true;
    for (int i = 0; i < RegConvention<ScalarType::Float>::Count; ++i)
      if (RegConvention<ScalarType::Float>::REGISTER_USAGE[i] ==
              RegisterUsage::callee_save &&
          used_float[i])
        save_float_regs.emplace_back(Reg(i, ScalarType::Float));
    size_t save_reg_cnt = save_int_regs.size() + save_float_regs.size();
    if ((stack_size + save_reg_cnt * 4) % 8)
      stack_size += 4;
    prologue = [save_int_regs, save_float_regs, stack_size](std::ostream &out) {
      if (save_int_regs.size()) {
        out << "push {";
        for (size_t i = 0; i < save_int_regs.size(); ++i) {
          if (i > 0)
            out << ',';
          out << save_int_regs[i];
        }
        out << "}\n";
      }
      for (auto reg : save_float_regs) {
        out << "vpush {" << reg << "}" << std::endl;
      }
      if (stack_size != 0)
        sp_move_asm(-stack_size, out);
    };
    ctx.epilogue = [save_int_regs, save_float_regs,
                    stack_size](std::ostream &out) -> bool {
      if (stack_size != 0)
        sp_move_asm(stack_size, out);
      for (auto it = save_float_regs.rbegin(); it != save_float_regs.rend();
           it++) {
        auto reg = *it;
        out << "vpop {" << reg << "}" << std::endl;
      }
      bool pop_lr = false;
      if (save_int_regs.size()) {
        out << "pop {";
        for (size_t i = 0; i < save_int_regs.size(); ++i) {
          if (i > 0)
            out << ',';
          if (save_int_regs[i].id == lr) {
            pop_lr = true;
            out << "pc";
          } else
            out << save_int_regs[i];
        }
        out << "}\n";
      }
      return pop_lr;
    };
    int cur_pos = stack_size + save_reg_cnt * INT_SIZE;
    for (auto &i : caller_stack_object) {
      i->position = cur_pos;
      cur_pos += i->size;
    }
    if (check_store_stack())
      break;
  }
  replace_with_reg_alloc(int_reg_alloc, float_reg_alloc);
  replace_complex_inst();
  remove_trivial_inst();
  out << '\n' << name << ":\n";
  prologue(out);
  for (auto &block : blocks)
    block->gen_asm(out, &ctx);
}
} // namespace ARMv7
