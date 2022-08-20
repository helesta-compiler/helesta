#include "ir/opt/opt.hpp"
std::vector<IR::BB *> get_BB_out(IR::BB *w) {
  IR::Instr *i = w->back();
  Case(IR::JumpInstr, jump_instr, i) return {jump_instr->target};
  Case(IR::BranchInstr, branch_instr, i) return {branch_instr->target1,
                                                 branch_instr->target0};
  return {};
}

void phi_src_rewrite(IR::BB *bb_cur, IR::BB *bb_old) {
  for (IR::BB *u : get_BB_out(bb_cur)) {
    u->for_each([&](IR::Instr *instr) {
      Case(IR::PhiInstr, phi_instr, instr) {
        for (auto &kv : phi_instr->uses) {
          if (kv.second == bb_old)
            kv.second = bb_cur;
        }
      }
    });
  }
}

// inline son to fa
void move_func(IR::NormalFunc *fa, IR::CallInstr *call, IR::BB *fa_bb) {
  using namespace IR;

  static std::unordered_map<NormalFunc *, int> is;
  Case(NormalFunc, son_func, call->f) {
    ++is[son_func];
    // map local var to fa
    // move all local vars to fa
    // if (son_func == fa){
    // son_func = copy_func(son_func);
    // }

    std::unordered_map<BB *, BB *> map_bb;
    std::unordered_map<Reg, Reg> map_reg;
    std::unordered_map<MemObject *, MemObject *> map_mm;
    son_func->scope.for_each([&](MemObject *x0, MemObject *x1) {
      assert(!x1->global);
      if (x1->arg) {
        delete x1;
      } else {
        // alloc local vars
        map_mm[x0] = x1;
        fa->scope.add(x1);
      }
    });
    son_func = son_func->copy();

    son_func->for_each([&](BB *bb) {
      map_bb[bb] = fa->new_BB();
      bb->for_each([&](Instr *instr) {
        Case(RegWriteInstr, rwinstr, instr) {
          Reg r = rwinstr->d1;
          if (!map_reg.count(r)) {
            map_reg[r] =
                fa->new_Reg(son_func->get_name(r) + "_" + son_func->name + "_" +
                            std::to_string(is[son_func]));
          }
        }
      });
    });
    BB *nxt = fa->new_BB();
    auto find_nxt_instr = [](BB *bb, CallInstr *instr) {
      for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
        Case(CallInstr, call, it->get()) {
          if (call == instr)
            return ++it;
        }
      }
      return bb->instrs.end();
    };
    nxt->instrs.splice(nxt->instrs.begin(), fa_bb->instrs,
                       find_nxt_instr(fa_bb, call), fa_bb->instrs.end());

    auto map_reg_f = [&](Reg &reg) { reg = map_reg.at(reg); };
    auto map_bb_f = [&](BB *&bb) { bb = map_bb.at(bb); };
    auto map_mem_f = [&](MemObject *&mm) {
      if (!mm->global) {
        mm = map_mm.at(mm);
      }
    };
    son_func->for_each([&](BB *bb) {
      // copy all BBs
      BB *bb1 = map_bb.at(bb);
      bb->for_each([&](Instr *instr) {
        Instr *instr1 = nullptr;
        Case(LoadArg<ScalarType::Int>, load_arg, instr) {
          UnaryOpInstr *uo_instr =
              new UnaryOpInstr(load_arg->d1, call->args.at(load_arg->id).first,
                               UnaryCompute::ID);
          map_reg_f(uo_instr->d1);
          instr1 = uo_instr;
        }
        else Case(LoadArg<ScalarType::Float>, load_arg, instr) {
          UnaryOpInstr *uo_instr =
              new UnaryOpInstr(load_arg->d1, call->args.at(load_arg->id).first,
                               UnaryCompute::ID);
          map_reg_f(uo_instr->d1);
          instr1 = uo_instr;
        }
        else Case(ReturnInstr<ScalarType::Int>, return_instr, instr) {
          UnaryOpInstr *uo_instr =
              new UnaryOpInstr(call->d1, return_instr->s1, UnaryCompute::ID);
          map_reg_f(uo_instr->s1);
          bb1->push(uo_instr);
          instr1 = new JumpInstr(nxt);
        }
        else Case(ReturnInstr<ScalarType::Float>, return_instr, instr) {
          UnaryOpInstr *uo_instr =
              new UnaryOpInstr(call->d1, return_instr->s1, UnaryCompute::ID);
          map_reg_f(uo_instr->s1);
          bb1->push(uo_instr);
          instr1 = new JumpInstr(nxt);
        }
        else {
          instr1 = instr->map(map_reg_f, map_bb_f, map_mem_f);
        }
        bb1->push(instr1);
      });
    });
    // fa->print(std::cerr);
    fa_bb->pop();
    fa_bb->push(new JumpInstr(map_bb.at(son_func->entry)));
    // fa_bb->print(std::cerr);
    phi_src_rewrite(nxt, fa_bb);
    fa_bb = nxt;
    // std::cerr << fa_bb->name << " end\n\n\n";
  }
}

void search_call_instr(IR::CompileUnit *ir, IR::NormalFunc *target_func) {
  ir->for_each([&](IR::NormalFunc *func) {
    bool flag;
    do {
      flag = false;
      std::vector<IR::BB *> bbs;
      func->for_each([&](IR::BB *bb) { bbs.push_back(bb); });
      for (auto bb : bbs) {
        for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
          Case(IR::CallInstr, call, it->get()) {
            Case(IR::NormalFunc, func_t, call->f) {
              if (target_func == func_t) {
                move_func(func, call, bb);
                flag = true;
                break;
              }
            }
          }
        }
      }
    } while (flag);
  });
}

void func_inline(IR::CompileUnit *ir) {
  struct State {
    std::vector<IR::NormalFunc *> in_nodes;
    int count;
  };
  std::unordered_map<IR::NormalFunc *, State> map;
  ir->for_each([&](IR::NormalFunc *func) {
    func->for_each([&](IR::BB *bb) {
      bb->for_each([&](IR::Instr *instr) {
        Case(IR::CallInstr, call_instr, instr) {
          Case(IR::NormalFunc, func_t, call_instr->f) {
            map[func].count++;
            map[func_t].in_nodes.push_back(func);
          }
        }
      });
    });
  });

  //  1.
  //    拓扑排序，所有非循环函数进行内联，这样可以保证所有函数都被内联，但是会导致代码过于冗余；
  //    2. 迭代若干次，根据 call graph
  //    的边的权值进行内联，目前是想直接根据指令数来决定。

  std::vector<IR::NormalFunc *> order;
  for (auto &[func, state] : map) {
    if (state.count == 0) {
      order.push_back(func);
    }
  }
  while (!order.empty()) {
    IR::NormalFunc *func = order.back();
    order.pop_back();

    search_call_instr(ir, func);

    for (auto &in_node : map[func].in_nodes) {
      map[in_node].count--;
      if (map[in_node].count == 0) {
        order.push_back(in_node);
      }
    }
  }
  // 迭代 3 次内联所有函数
  for (int i = 0; i < 3; ++i) {
    ir->for_each([&](IR::NormalFunc *func) {
      std::vector<IR::BB *> bbs;
      func->for_each([&](IR::BB *bb) { bbs.push_back(bb); });
      for (auto bb : bbs) {
        for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
          Case(IR::CallInstr, call_instr, it->get()) {
            Case(IR::NormalFunc, func_t, call_instr->f) {
              int instr_cnt = 0;
              func->for_each(
                  [&](IR::BB *bb) { instr_cnt += bb->instrs.size(); });
              if (instr_cnt >= 1000)
                continue;
              // std::cerr << func->name << "(" << bb->name << ") -> " <<
              // func_t->name << " [" << i << "," << instr_cnt << "," <<
              // func->bbs.size()
              // << "]" << std::endl;
              move_func(func, call_instr, bb);
              // std::cerr << "end move func" << std::endl;
              break;
            }
          }
        }
      }
    });
  }
  // std::cerr << "end func inline" << std::endl;
}
