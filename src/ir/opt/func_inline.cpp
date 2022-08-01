#include "ir/opt/opt.hpp"
#include <iostream>

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

  // std::cerr << "Start move func" << std::endl;

  struct InlineState {
    std::unordered_map<MemObject *, MemObject *> map_mm;
    int cnt = 0;
  };
  static std::unordered_map<NormalFunc *, InlineState> is;

  Case(NormalFunc, son_func, call->f) {
    // map local var to fa
    // move all local vars to fa

    // std::cerr << fa->name << " " << son_func->name << std::endl;

    if (!is.count(son_func)) {
      son_func->scope.for_each([&](MemObject *x0, MemObject *x1) {
        assert(!x1->global);
        if (x1->arg) {
          delete x1;
        } else {
          // alloc local vars
          is[son_func].map_mm[x0] = x1;
          fa->scope.add(x1);
        }
      });
    }

    // std::cerr << "pass count" << std::endl;

    std::unordered_map<BB *, BB *> map_bb;
    std::unordered_map<Reg, Reg> map_reg;
    std::unordered_map<MemObject *, MemObject *> &map_mm = is[son_func].map_mm;
    ++is[son_func].cnt;
    son_func->for_each([&](BB *bb) {
      map_bb[bb] = fa->new_BB();
      bb->for_each([&](Instr *instr) {
        Case(RegWriteInstr, rwinstr, instr) {
          Reg r = rwinstr->d1;
          if (!map_reg.count(r)) {
            map_reg[r] =
                fa->new_Reg(son_func->get_name(r) + "_" + son_func->name + "_" +
                            std::to_string(is[son_func].cnt));
            // std::cerr << fa->reg_names[map_reg[r].id] << std::endl;
          }
        }
      });
    });
    // std::cerr << "start to copy all BBs" << std::endl;
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

    auto map_reg_f = [&](Reg &reg) {
      // if (!map_reg.count(reg)) {
      //   std::cerr << son_func->name << " " << son_func->reg_names.size() <<
      //   std::endl; std::cerr << "unknown reg: " << reg << " " <<
      //   son_func->get_name(reg)
      //             << "\n";
      //   assert(0);
      // }
      // std::cerr << "map reg (" << reg;
      reg = map_reg.at(reg);
      // std::cerr << ")" << std::endl;
    };
    auto map_bb_f = [&](BB *&bb) {
      // std::cerr << "map bb (";
      bb = map_bb.at(bb);
      // std::cerr << ")" << std::endl;
    };
    auto map_mem_f = [&](MemObject *&mm) {
      if (!mm->global) {
        // std::cerr << "map mem (";
        mm = map_mm.at(mm);
        // std::cerr << ")" << std::endl;
      }
    };

    son_func->for_each([&](BB *bb) {
      // copy all BBs
      // std::cerr << "bb->name : " << bb->name << std::endl;
      BB *bb1 = map_bb.at(bb);
      // std::cerr << "start" << std::endl;
      bb->for_each([&](Instr *instr) {
        // instr->print(std::cerr);
        Instr *instr1 = nullptr;
        Case(LocalVarDef, local_val_def, instr) {
          if (local_val_def->data->arg)
            return;
        }
        Case(LoadArg, load_arg, instr) {
          UnaryOpInstr *uo_instr = new UnaryOpInstr(
              load_arg->d1, call->args.at(load_arg->id), UnaryCompute::ID);
          map_reg_f(uo_instr->d1);
          instr1 = uo_instr;
        }
        else Case(ReturnInstr, return_instr, instr) {
          UnaryOpInstr *uo_instr =
              new UnaryOpInstr(call->d1, return_instr->s1, UnaryCompute::ID);
          // std::cerr << "call->d1 = " << call->d1 << "\nReturn Instr"
          // << std::endl;
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
    fa_bb->pop();
    fa_bb->push(new JumpInstr(map_bb.at(son_func->entry)));
    phi_src_rewrite(nxt, fa_bb);
    fa_bb = nxt;
    return;
  }

  assert(0);
}

void search_call_instr(IR::NormalFunc *func) {
  // std::cerr << "start search call instr" << std::endl;
  std::vector<IR::BB *> bbs;
  func->for_each([&](IR::BB *bb) { bbs.push_back(bb); });
  for (auto bb : bbs) {
    // std::cerr << "auto bb : bbs" << std::endl;
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Case(IR::CallInstr, call, it->get()) {
        Case(IR::NormalFunc, func_t, call->f) {
          if (func_t != func) {
            std::cerr << func->name << " -> " << func_t->name << std::endl;
            // std::cerr << "start move func" << std::endl;
            move_func(func, call, bb);
            // std::cerr << "end move func" << std::endl;
            break;
          }
        }
      }
    }
  }
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

  /* 1.
     拓扑排序，所有非循环函数进行内联，这样可以保证所有函数都被内联，但是会导致代码过于冗余；
     2. 迭代若干次，根据 call graph
     的边的权值进行内联，目前是想直接根据指令数来决定。
  */
  std::vector<IR::NormalFunc *> order;
  for (auto &[func, state] : map) {
    if (state.count == 0) {
      order.push_back(func);
    }
  }
  while (!order.empty()) {
    std::cerr << "order size " << order.size() << std::endl;
    IR::NormalFunc *func = order.back();
    order.pop_back();

    search_call_instr(func);
    std::cerr << "end search call instr" << std::endl;

    for (auto &in_node : map[func].in_nodes) {
      map[in_node].count--;
      if (map[in_node].count == 0) {
        order.push_back(in_node);
      }
    }
    std::cerr << "! order size " << order.size() << std::endl;
    std::cerr << "topo sort end" << std::endl;
  }
}