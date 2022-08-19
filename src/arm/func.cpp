#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "arm/func.hpp"
#include "arm/simple_coloring_alloc.hpp"

namespace ARMv7 {

void handle_params(Func *ctx, MappingInfo &info, Block *entry,
                   IR::NormalFunc *ir_func) {
  int int_arg_cnt = 0, float_arg_cnt = 0;
  for (auto arg_type : ir_func->arg_types) {
    Reg cur_reg = info.new_reg();
    if (arg_type == ScalarType::Int) {
      if (int_arg_cnt <
          RegConvention<ScalarType::Int>::ARGUMENT_REGISTER_COUNT) {
        entry->push_back(std::make_unique<MoveReg>(
            cur_reg,
            Reg(RegConvention<ScalarType::Int>::ARGUMENT_REGISTERS[int_arg_cnt],
                ScalarType::Int)));
      } else {
        std::unique_ptr<StackObject> t = std::make_unique<StackObject>();
        t->size = INT_SIZE;
        t->position = -1;
        entry->push_back(std::make_unique<LoadStack>(cur_reg, 0, t.get()));
        ctx->caller_stack_object.push_back(std::move(t));
      }
      int_arg_cnt += 1;
    } else if (arg_type == ScalarType::Float) {
      info.set_float(cur_reg);
      if (float_arg_cnt <
          RegConvention<ScalarType::Float>::ARGUMENT_REGISTER_COUNT) {
        entry->push_back(std::make_unique<MoveReg>(
            cur_reg,
            Reg(RegConvention<
                    ScalarType::Float>::ARGUMENT_REGISTERS[float_arg_cnt],
                ScalarType::Float)));
      } else {
        std::unique_ptr<StackObject> t = std::make_unique<StackObject>();
        t->size = INT_SIZE;
        t->position = -1;
        entry->push_back(std::make_unique<LoadStack>(cur_reg, 0, t.get()));
        ctx->caller_stack_object.push_back(std::move(t));
      }
      float_arg_cnt += 1;
    } else
      assert(false);
    ctx->args.push_back(cur_reg);
  }
}

template <ScalarType type>
std::vector<int> reg_allocate(RegAllocStat *stat, Func *ctx) {
  info << "register allocation for function: " << ctx->name << '\n';
  info << "reg_n = " << ctx->reg_n << '\n';
  stat->spill_cnt = 0;
  ColoringAllocator *allocator = nullptr;
  PassDisabled("irc-alloc") {
    info << "using SimpleColoringAllocator\n";
    allocator = new SimpleColoringAllocator<type>(ctx);
  }
  else {
    info << "using IRCColoringAllocator\n";
    allocator = new IRCColoringAllocator<type>(ctx);
  }
  while (true) {
    allocator->clear();
    std::vector<int> ret = allocator->run(stat);
    if (stat->succeed)
      return ret;
  }
}

bool Func::check_store_stack() {
  bool ret = true;
  for (auto &block : blocks) {
    int32_t sp_offset = 0;
    for (auto i = block->insts.begin(); i != block->insts.end(); ++i) {
      (*i)->maintain_sp(sp_offset);
      InstCond cond = (*i)->cond;
      if (auto store_stk = (*i)->as<StoreStack>()) {
        int32_t total_offset =
            store_stk->target->position + store_stk->offset - sp_offset;
        if (!load_store_offset_range(total_offset)) {
          Reg imm(reg_n++, ScalarType::Int);
          block->insts.insert(
              i, set_cond(std::make_unique<LoadStackOffset>(
                              imm, store_stk->offset, store_stk->target),
                          cond));
          *i = set_cond(std::make_unique<ComplexStore>(
                            store_stk->src, Reg(sp, ScalarType::Int), imm),
                        cond);
          ret = false;
        }
      }
    }
  }
  return ret;
}

void Func::allocate_register() {
  RegAllocStat int_stat, float_stat;
  std::vector<int> int_reg_alloc, float_reg_alloc;
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
    ctx.prologue = [save_int_regs, save_float_regs,
                    stack_size](std::ostream &out) {
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
}

Func::Func(Program *prog, std::string _name, IR::NormalFunc *ir_func)
    : name(_name), entry(nullptr), reg_n(0) {
  MappingInfo info;
  for (size_t i = 0; i < ir_func->scope.objects.size(); ++i) {
    IR::MemObject *cur = ir_func->scope.objects[i].get();
    if (cur->size == 0)
      continue;
    std::unique_ptr<StackObject> res = std::make_unique<StackObject>();
    res->size = cur->size;
    res->position = -1;
    info.obj_mapping[cur] = res.get();
    stack_objects.push_back(std::move(res));
  }
  entry = new Block(".entry_" + name);
  blocks.emplace_back(entry);
  auto dom_ctx = construct_dom_tree(ir_func);
  auto loop_ctx = construct_loop_tree(ir_func, dom_ctx.get());
  for (size_t i = 0; i < ir_func->bbs.size(); ++i) {
    IR::BB *cur = ir_func->bbs[i].get();
    std::string cur_name = ".L" + std::to_string(prog->block_n++);
    std::unique_ptr<Block> res = std::make_unique<Block>(cur_name);
    res->thread_id = ir_func->bbs[i]->thread_id;
    res->depth = loop_ctx->nodes[i]->dep;
    info.block_mapping[cur] = res.get();
    info.rev_block_mapping[res.get()] = cur;
    blocks.push_back(std::move(res));
  }
  handle_params(this, info, entry, ir_func);
  Block *real_entry = info.block_mapping[ir_func->entry];
  if (blocks[1].get() != real_entry)
    entry->push_back(std::make_unique<Branch>(real_entry));
  entry->out_edge.push_back(real_entry);
  real_entry->in_edge.push_back(entry);
  std::map<Reg, CmpInfo> cmp_info;
  for (size_t i = 0; i < blocks.size(); ++i)
    if (blocks[i].get() != entry) {
      IR::BB *cur_ir_bb = info.rev_block_mapping[blocks[i].get()];
      Block *next_block = nullptr;
      if (i + 1 < blocks.size())
        next_block = blocks[i + 1].get();
      blocks[i]->construct(cur_ir_bb, this, &info, next_block,
                           cmp_info); // maintain in_edge, out_edge,
                                      // reg_mapping, ignore phi function
    }
  reg_n = info.reg_n;
  float_regs = std::move(info.float_regs);
  for (auto &block : blocks) {
    for (auto &inst : block->insts) {
      for (Reg *r : inst->regs()) {
        if (r->is_pseudo())
          r->type = float_regs.count(*r) ? ScalarType::Float : ScalarType::Int;
      }
    }
  }

  if (global_config.args.count("ir2")) {
    ir_func->for_each([&](IR::BB *bb0) {
      std::cerr << "================================\n";
      std::cerr << *bb0;
      info.block_mapping[bb0]->print(std::cerr);
    });
  }
}

void Func::gen_asm(std::ostream &out) {
  out << '\n' << name << ":\n";
  ctx.prologue(out);
  for (auto &block : blocks)
    block->gen_asm(out, &ctx);
}
} // namespace ARMv7
